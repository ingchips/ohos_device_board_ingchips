#include <stdio.h>
#include <string.h>
#include "platform_api.h"
#include "ll_api.h"
#include "att_db.h"
#include "gap.h"
#include "l2cap.h"
#include "att_dispatch.h"
#include "btstack_util.h"
#include "btstack_event.h"
#include "btstack_defines.h"
#include "gatt_client.h"
#include "le_device_db.h"
#include "sig_uuid.h"
#include "sm.h"

#include "los_task.h"
#include "los_swtmr.h"
#include <string.h>
#include <stdlib.h>

#include "ad_parser.h"

#include "zt_comm.h"

#include "app_cfg.h"
#include "app_cfg.h"

#include "dt_698_645.h"

#include "log_flash.h"
#include "main.h"
#include "uart_driver.h"
#include "profile.h"
#include "ota_service.h"
void connection_state(int flag);

// GATT characteristic handles
enum
{
    HANDLE_DATE_RECEPTION=3,
    HANDLE_DATE_NOTIFICATION=5,
    HANDLE_DATE_NOTIFICATION_CLIENT_CHAR_CONFIG=6,
    HANDLE_DATE_RECEPTION_OFFSET=73,
    HANDLE_DATE_NOTIFICATION_OFFSET=122
};

#define FLAG_OFFSET   19

sm_persistent_t sm_persistent =
{
    .identity_addr_type     = BD_ADDR_TYPE_LE_RANDOM,
};

uint8_t g_adv_data[] = {
    #include "../data/advertising.adv"
};

#include "../data/advertising.const"

uint8_t g_scan_data[31] = {
    #include "../data/scan_response.adv"
};

uint8_t profile_data[] = {
#if (RELEASE_VERSION == RTK_VEISHENG_VERSION)
	#include "../data/gatt1.profile"
#else
    #include "../data/gatt.profile"
#endif	
};
// GATT characteristic handles
#include "../data/gatt.const"

int g_adv_data_len = 31;
int g_scan_data_len = 28;
bd_addr_t temp_taiti_mac;

#define INITIATING_OFF      0xff
#define INITIATING_AUTO     0xfe

uint8_t initiating_id = INITIATING_OFF;
uint8_t is_scanning = 0;
uint8_t is_ble_off = 0;
static UINT32 create_conn_timer = 0;
static UINT32 reset_timer = 0;
static UINT32 adv_report_timer = 0;
static UINT32 ble_off_timer = 0;
static UINT32 master_comm_timeout_timer = 0;

#define INVALID_HANDLE      (0xffff)
const uint8_t UUID_METER[] = {0x6E,0x40,0x00,0x01,0xB5,0xA3,0xF3,0x93,0xE0,0xA9,0xE5,0x0E,0x24,0xDC,0x41,0x79};
const uint8_t UUID_RX[]    = {0x6E,0x40,0x00,0x02,0xB5,0xA3,0xF3,0x93,0xE0,0xA9,0xE5,0x0E,0x24,0xDC,0x41,0x79};
const uint8_t UUID_TX[]    = {0x6E,0x40,0x00,0x03,0xB5,0xA3,0xF3,0x93,0xE0,0xA9,0xE5,0x0E,0x24,0xDC,0x41,0x79};

#define OGF_STATUS_PARAMETERS       0x05
#define OPCODE(ogf, ocf)            (ocf | ogf << 10)
#define OPCODE_READ_RSSI            OPCODE(OGF_STATUS_PARAMETERS, 0x05)

#pragma pack (push, 1)
typedef struct read_rssi_complete
{
    uint8_t  status;
    uint16_t handle;
    int8_t   rssi;
} read_rssi_complete_t;
#pragma pack (pop)

typedef struct slave_info
{
    gatt_client_service_t                   meter_service;
    gatt_client_characteristic_t            rx_char;
    gatt_client_characteristic_t            noti_char;
    gatt_client_characteristic_descriptor_t noti_desc;
    gatt_client_notification_t              noti_notify;
    dt698_645_fsm_t  comm_fsm;
    uint16_t    id;
    int16_t     rssi;
    uint16_t    conn_handle;
    uint8_t     bonding_clear;
    uint8_t     rssi_req;
    bd_addr_t   addr;
} slave_info_t;

typedef struct
{
    dt698_645_fsm_t comm_fsm;
    bd_addr_t addr; // 01 00 00 00 00 C1
    bd_addr_type_t addr_type;
    uint16_t conn_handle;
    int16_t  rssi;
    uint8_t  notify_enable;
    uint8_t  bonding_clear;
    uint8_t  rssi_req;
#if(RELEASE_VERSION != RTK_VEISHENG_VERSION)	
    uint8_t  comm_time_out_en;
    uint16_t comm_time_out;
    uint16_t no_comm_timer_cnt;
#endif
} peripheral_cfg_t;

extern const char module_ver[6];
extern const char module_sn[20];

#define ADV_REPORT_BUFF_SIZE        ((31 + 8) * SCAN_FILTER_MAX_NUM)
int adv_report_size = 0;
uint8_t adv_report_buffer[ADV_REPORT_BUFF_SIZE];

#define INIT_FIELDS .conn_handle = INVALID_HANDLE, \
                    .meter_service = { .start_group_handle = INVALID_HANDLE}, \
                    .rx_char       = { .value_handle = INVALID_HANDLE}, \
                    .noti_char     = { .value_handle = INVALID_HANDLE}, \
                    .noti_desc     = { .handle = INVALID_HANDLE}, \
                    .bonding_clear = 1

slave_info_t slave_lst[SLAVE_MAX_NUM] =
{
    {.id = 0, INIT_FIELDS},
    {.id = 1, INIT_FIELDS},
    {.id = 2, INIT_FIELDS}
};

peripheral_cfg_t peripheral_cfgs[MASTER_MAX_NUM] = {
    {.notify_enable = 0, .conn_handle = INVALID_HANDLE, .bonding_clear = 1},
    {.notify_enable = 0, .conn_handle = INVALID_HANDLE, .bonding_clear = 1},
};
#if (RELEASE_VERSION == RTK_VEISHENG_VERSION)
#define SCAN_INTERVAL 200 // * 0.625ms
#if DEBUG_DISCONNECT
#define SCAN_WINDOW 80 // * 0.625ms
#else
#define SCAN_WINDOW 180 // * 0.625ms
#endif
#define SCAN_INTERVAL2 150 // * 0.625ms
#define SCAN_WINDOW2 90 // * 0.625ms
#define I_AM_MASTER_CON_MIN_INTERVAL 120 // * 1.25ms
#define I_AM_MASTER_CON_MAX_INTERVAL 120 // * 1.25ms
#define I_AM_MASTER_TIMEOUT 600 // * 10ms
#define I_AM_MASTER_CE_LEN 20
#else
#define SCAN_INTERVAL 96 // * 0.625ms
#define SCAN_WINDOW 16 // * 0.625ms
#define SCAN_INTERVAL2 SCAN_INTERVAL // * 0.625ms
#define SCAN_WINDOW2 SCAN_WINDOW // * 0.625ms
#define I_AM_MASTER_CON_MIN_INTERVAL 48 // * 1.25ms
#define I_AM_MASTER_CON_MAX_INTERVAL 48 // * 1.25ms
#define I_AM_MASTER_TIMEOUT 400 // * 10ms
#define I_AM_MASTER_CE_LEN 10
#endif
const static initiating_phy_config_t phy_configs[] =
{
    {
        .phy = PHY_1M,
        .conn_param =
        {
            .scan_int = SCAN_INTERVAL,
            .scan_win = SCAN_WINDOW,
            .interval_min = I_AM_MASTER_CON_MIN_INTERVAL,
            .interval_max = I_AM_MASTER_CON_MAX_INTERVAL,
            .latency = 0,
            .supervision_timeout = I_AM_MASTER_TIMEOUT,
            .min_ce_len = I_AM_MASTER_CE_LEN,
            .max_ce_len = I_AM_MASTER_CE_LEN
        }
    }
};

scan_phy_config_t scan_configs[] =
{
    {
        .phy = PHY_1M,
        .type = SCAN_PASSIVE,
        .interval = SCAN_INTERVAL2,
        .window = SCAN_WINDOW2
    }
};

void update_conn_params(const settings_t *settings)
{
    return;
//    int i;
//    for (i = 0; i < sizeof(phy_configs) / sizeof(phy_configs[0]); i++)
//    {
//        phy_configs[i].conn_param.supervision_timeout = settings->master_conn_param.sup_timer / 10;
//        phy_configs[i].conn_param.latency = settings->master_conn_param.latency;
//        phy_configs[i].conn_param.interval_min = (uint32_t)settings->master_conn_param.interval * 1000 / 625;
//        phy_configs[i].conn_param.interval_max = phy_configs[i].conn_param.interval_min;
//    }
}

static void initiating(const settings_t *settings, uint8_t init_id)
{
    if (is_ble_off) return;

    initiating_id = init_id;
    if (INITIATING_OFF == init_id) return;

    const dev_settings_t *targets = get_targets();
    update_conn_params(settings);
    if (INITIATING_AUTO == init_id)
    {
        gap_ext_create_connection(INITIATING_ADVERTISER_FROM_LIST, // Initiator_Filter_Policy,
                                  BD_ADDR_TYPE_LE_RANDOM,          // Own_Address_Type,
                                  BD_ADDR_TYPE_LE_RANDOM,          // Peer_Address_Type,
                                  NULL,                            // Peer_Address,
                                  sizeof(phy_configs) / sizeof(phy_configs[0]),
                                  phy_configs);
    }
    else
    {
        gap_ext_create_connection(INITIATING_ADVERTISER_FROM_PARAM,// Initiator_Filter_Policy,
                                  BD_ADDR_TYPE_LE_RANDOM,          // Own_Address_Type,
                                  BD_ADDR_TYPE_LE_RANDOM,          // Peer_Address_Type,
                                  targets[init_id].addr,           // Peer_Address,
                                  sizeof(phy_configs) / sizeof(phy_configs[0]),
                                  phy_configs);
    }
    LOS_SwtmrStart(create_conn_timer);
}

static int is_invalid_mac(const uint8_t *mac)
{
    const uint32_t *p1 = (const uint32_t *)mac;
    const uint16_t *p2 = (const uint16_t *)(mac + 4);
    if ((*p1 == 0xffffffffu) && (*p2 == 0xffffu))
        return 1;
#if DEBUG_DISCONNECT
	else if ((*p1 == 0x00000000u) && (*p2 == 0x0000u))
        return 1;
#endif
    else
        return 0;
}

static uint8_t cmd_scan_en_state = 0;
static void auto_connect(const settings_t *settings)
{
    int i;
    int con_dev_cnt = 0;
    int invalid_mac_cnt = 0;
    const uint8_t *addr;

    dev_settings_t *targets = get_targets();
    if (settings == NULL) settings = get_settings();

    if (is_scanning) {
        gap_set_ext_scan_enable(0, 0, 0, 0);
        is_scanning = 0;
        //return;
    }

    gap_clear_white_lists();

    for (i = 0; i < SLAVE_MAX_NUM; i++) {
        addr = targets[i].addr;
		log_printf("now addr:%x-%x-%x-%x-%x-%x\n", 
						addr[0],addr[1],addr[2],addr[3],addr[4],addr[5]);
        if (is_invalid_mac(addr)) {
            invalid_mac_cnt++;
			log_printf("invalid_mac_cnt=%d, handle=%d\n", slave_lst[i].conn_handle);
            if (slave_lst[i].conn_handle != INVALID_HANDLE) {
                gap_disconnect(slave_lst[i].conn_handle);
                log_printf("at dis r\n");
            } else {
                continue;
            }
        } else {
            if (targets[i].addr_changed != 0) {
                if (slave_lst[i].conn_handle != INVALID_HANDLE) {
                    gap_disconnect(slave_lst[i].conn_handle);
                    log_printf("at dis s\n");
					log_printf("dd_whitelist1:%x-%x-%x-%x-%x-%x\n", 
						addr[0],addr[1],addr[2],addr[3],addr[4],addr[5]);
                    gap_add_whitelist(addr, BD_ADDR_TYPE_LE_RANDOM);
                    con_dev_cnt++;
                } else {
					log_printf("dd_whitelist2:%x-%x-%x-%x-%x-%x\n", 
						addr[0],addr[1],addr[2],addr[3],addr[4],addr[5]);
                    gap_add_whitelist(addr, BD_ADDR_TYPE_LE_RANDOM);
                    con_dev_cnt++;
                }
                targets[i].addr_changed = 0;
            } else {
                if (slave_lst[i].conn_handle != INVALID_HANDLE) {
                    continue;
                } else {
					log_printf("dd_whitelist3:%x-%x-%x-%x-%x-%x\n", 
						addr[0],addr[1],addr[2],addr[3],addr[4],addr[5]);
                    gap_add_whitelist(addr, BD_ADDR_TYPE_LE_RANDOM);
                    con_dev_cnt++;
                }
            }
        }
    }

    if (con_dev_cnt) {
        initiating(settings, INITIATING_AUTO);
    } 
#if (RELEASE_VERSION != RTK_VEISHENG_VERSION)
    else if (invalid_mac_cnt == SLAVE_MAX_NUM) {
        scan_control(settings, 1);
    } else if (invalid_mac_cnt != 0){
        if ((cmd_scan_en_state != 0) && (is_scanning == 0)) {
            scan_control(settings, 1);
        }
    }
#endif
}

#define ARRAY_LEN(arr)  ((sizeof(arr) / sizeof(arr[0])))

peripheral_cfg_t *peripheral_from_conn_handle(hci_con_handle_t handle)
{
    int i;
    for (i = 0; i < MASTER_MAX_NUM; i++)
        if (peripheral_cfgs[i].conn_handle == handle)
            return &peripheral_cfgs[i];
    return NULL;
}

void print_addr(const uint8_t *addr)
{
    log_printf("%02X:%02X:%02X:%02X:%02X:%02X\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}

static uint16_t att_read_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset,
                                  uint8_t * buffer, uint16_t buffer_size)
{
    switch (att_handle)
    {
	default:
		  return ota_read_callback(att_handle, offset, buffer, buffer_size);
    }
}

static btstack_packet_callback_registration_t hci_event_callback_registration;

static void ble_frame_rx_byte(dt698_645_fsm_t *fsm, const uint8_t *data, int data_len)
{
    switch (data_mode) {
    case DATA_MODE_UNDEFINED:
        break;
    case DATA_MODE_CACHED:
        dt698_645_frame_rx_byte(fsm, data, data_len);
        break;
    case DATA_MODE_DIRECT:
        common_frame_rx_byte(fsm, data, data_len);
        break;
    default:
        break;
    }
    return;
}

static int att_write_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t transaction_mode,
                              uint16_t offset, const uint8_t *buffer, uint16_t buffer_size)
{
    const settings_t *settings = get_settings();
    peripheral_cfg_t *peripheral;
    log_printf("write %d\r\n", buffer_size);
    switch (att_handle)
    {
    case HANDLE_DATE_RECEPTION:
        peripheral = peripheral_from_conn_handle(connection_handle);
        if (NULL == peripheral) return 0;
		#if (RELEASE_VERSION != RTK_VEISHENG_VERSION)
        peripheral->no_comm_timer_cnt = 0;
		#endif
        ble_frame_rx_byte(&peripheral->comm_fsm, buffer, buffer_size);
        return 0;
    case HANDLE_DATE_NOTIFICATION_CLIENT_CHAR_CONFIG:
        peripheral = peripheral_from_conn_handle(connection_handle);
        if (NULL == peripheral) return 0;
 		#if (RELEASE_VERSION != RTK_VEISHENG_VERSION)
		peripheral->no_comm_timer_cnt = 0;
		#endif
        if (*(uint16_t *)buffer == GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION) {
			#if (RELEASE_VERSION != RTK_VEISHENG_VERSION)
            sm_request_pairing(connection_handle);
			#endif
            peripheral->notify_enable = 1;
            log_printf("notify_enable\r\n");
        } else {
            peripheral->notify_enable = 0;
        }
        return 0;
    default:
        return ota_write_callback(att_handle, transaction_mode, offset, buffer, buffer_size);
    }
}

static bd_addr_t connected_devs[SLAVE_MAX_NUM];

int get_connected_all_mac(const settings_t *settings, conn_dev_info_t *info)
{
    int i;
    int total = 0;
    info->active = 0;
    for (i = 0; i < MASTER_MAX_NUM; i++)
    {
        if (INVALID_HANDLE == peripheral_cfgs[i].conn_handle)
#if (RELEASE_VERSION == RTK_VEISHENG_VERSION)
            memset(info->addr[total],0xff,6);
            else
            {
                memcpy(info->addr[total], peripheral_cfgs[i].addr, BD_ADDR_LEN); 
                info->active |= 1 << i;
            }
#else
            continue;
        reverse_bd_addr(peripheral_cfgs[i].addr, info->addr[total]);        
        info->active |= 1 << i;
#endif        
        total++;
    }
    for (i = 0; i < SLAVE_MAX_NUM; i++)
    {
        if (INVALID_HANDLE == slave_lst[i].conn_handle) 
#if (RELEASE_VERSION == RTK_VEISHENG_VERSION)
        memset(info->addr[total],0xff,6);
        else
        {
            info->active |= 1 << (MASTER_MAX_NUM + i);       
            memcpy(info->addr[total], slave_lst[i].addr, BD_ADDR_LEN);
        }
#else
        continue;
        info->active |= 1 << (MASTER_MAX_NUM + i);
        memcpy(info->addr[total], slave_lst[i].addr, BD_ADDR_LEN);
#endif
        total++;
    }
    return total;
}

const uint8_t *get_connected_slaves_mac(const settings_t *settings, int *slave_num)
{
    int i;
    *slave_num = 0;
    for (i = 0; i < SLAVE_MAX_NUM; i++)
    {
        if (INVALID_HANDLE == slave_lst[i].conn_handle) continue;
        memcpy(connected_devs[*slave_num], slave_lst[i].addr, BD_ADDR_LEN);
        *slave_num = *slave_num + 1;
    }
    return connected_devs[0];
}

const uint8_t *get_connected_masters_mac(const settings_t *settings, int *master_num)
{
    int i;
    *master_num = 0;
    for (i = 0; i < MASTER_MAX_NUM; i++)
    {
        if (INVALID_HANDLE == peripheral_cfgs[i].conn_handle) continue;
        memcpy(connected_devs[*master_num], peripheral_cfgs[i].addr, BD_ADDR_LEN);
        *master_num = *master_num + 1;
    }
    return connected_devs[0];
}

int get_target_index(const settings_t *settings, const uint8_t *addr_le)
{
    int i;
    bd_addr_t rev;
    reverse_bd_addr(addr_le, rev);
    const dev_settings_t *targets = get_targets();
    for (i = 0; i < SLAVE_MAX_NUM; i++)
    {
        if (memcmp(targets[i].addr, rev, BD_ADDR_LEN) == 0)
            return i;
    }
    return -1;
}

slave_info_t *get_slave_by_conn(const uint16_t conn_handle)
{
    int i;
    for (i = 0; i < SLAVE_MAX_NUM; i++)
    {
        if (slave_lst[i].conn_handle == conn_handle)
            return &slave_lst[i];
    }
    return NULL;
}

static int gap_is_connection_cancel = 0;
void cancel_initiating(void)
{
    if (initiating_id != INITIATING_OFF)
    {
        gap_create_connection_cancel();
		log_printf("connection_cancel=%d\n", gap_is_connection_cancel);
        if (gap_is_connection_cancel == 1) {
            gap_is_connection_cancel = 0;
            const settings_t *settings;
            settings = get_settings();
            initiating_id = INITIATING_OFF;
            auto_connect(settings);
        } else {
            gap_is_connection_cancel = 1;
        }
    }
}

void disconnect_all(void)
{
    int i;
    cancel_initiating();

    for (i = 0; i < MASTER_MAX_NUM; i++)
    {
        if (peripheral_cfgs[i].conn_handle != INVALID_HANDLE)
        {
            gap_disconnect(peripheral_cfgs[i].conn_handle);
            log_printf("all dis r\n");
            peripheral_cfgs[i].conn_handle = INVALID_HANDLE;
        }
    }

    for (i = 0; i < SLAVE_MAX_NUM; i++)
    {
        if (slave_lst[i].conn_handle != INVALID_HANDLE)
        {
            gap_disconnect(slave_lst[i].conn_handle);
            log_printf("all dis s\n");
            slave_lst[i].conn_handle = INVALID_HANDLE;
        }
    }
}

void local_pair_mode_updated(void)
{
    settings_t *settings = get_settings();
    if (settings->local.smp_level == SMP_MIMT_KEY)
    {
        profile_data[HANDLE_DATE_RECEPTION_OFFSET - FLAG_OFFSET] = 0x07;
        profile_data[HANDLE_DATE_NOTIFICATION_OFFSET - FLAG_OFFSET] = 0x06;
    }
    else
    {
        profile_data[HANDLE_DATE_RECEPTION_OFFSET - FLAG_OFFSET] = 0x03;
        profile_data[HANDLE_DATE_NOTIFICATION_OFFSET - FLAG_OFFSET] = 0x02;
    }
}

void adv_info_report_control(uint8_t status, uint8_t interval)
{
    if (status == 0)
    {
        LOS_SwtmrStop(adv_report_timer);
        return;
    }
    //xTimerChangePeriod(adv_report_timer, pdMS_TO_TICKS(interval * 1000), portMAX_DELAY);
    LOS_SwtmrStart(adv_report_timer);
}

uint8_t scan_control(const settings_t *settings, uint8_t enable)
{
    if (is_scanning == enable) return COMM_ERR_SUCC;
    is_scanning = enable;
    if (0 == enable)
    {
        gap_set_ext_scan_enable(0, 0, 0, 0);
        if (INITIATING_OFF == initiating_id)
            auto_connect(settings);
        return COMM_ERR_SUCC;
    }

    adv_report_size = 0;

    if (initiating_id != INITIATING_OFF)
        cancel_initiating();

    gap_clear_white_lists();
#if(RELEASE_VERSION == RTK_VEISHENG_VERSION)
    scan_configs[0].interval = settings->scan_interval/0.625;
    scan_configs[0].window = (settings->scan_interval) / 2;
#endif
    //PRINT("scan %u %u %u\r\n", scan_configs[0].interval, scan_configs[0].window, settings->filter_mac_cnt);

    gap_set_ext_scan_para(BD_ADDR_TYPE_LE_RANDOM, SCAN_ACCEPT_ALL_EXCEPT_NOT_DIRECTED,
                        sizeof(scan_configs) / sizeof(scan_configs[0]),
                        scan_configs);

    gap_set_ext_scan_enable(1, 0, 0, 0);   // filter enabled
    return COMM_ERR_SUCC;
}

void *is_addr_in_report(const uint8_t *addr)
{
    int pos = 0;
    while (pos < adv_report_size)
    {
        int len = adv_report_buffer[pos];
        pos += 1;
        if (memcmp(&adv_report_buffer[pos], addr, BD_ADDR_LEN) == 0) {
            //log_printf("addr in report %x %x\r\n", addr[0], adv_report_buffer[pos]);
            return adv_report_buffer + pos;
        }
        pos += len;
    }

    return NULL;
}

int filter_dev_type(const settings_t *settings, const le_ext_adv_report_t *report)
{
    int i;
    const uint8_t *found_data = NULL;
    uint16_t found_length;
#if(RELEASE_VERSION == RTK_VEISHENG_VERSION)
uint8_t type_mask = 0x02;
    if (settings->filter_type_cnt == 0 || ((settings->filter_onoff&type_mask) == 0))
    {
        return 0;
    }
#else
   if (settings->filter_type_cnt == 0)
    {
        return 0;
    }
#endif

    #define MANU_DEFINE_DATA (0xFF)
    found_data = 0;//ad_data_from_type(report->data_len, (uint8_t *)&(report->data[0]), MANU_DEFINE_DATA, &found_length);
    if (found_data == NULL) {
        return -1;
    }

    for (i = 0; i < settings->filter_type_cnt; i++) {
        if ((report->address[BD_ADDR_LEN - 1] == settings->filter_types[i]) &&
            (found_data[0] == settings->filter_types[i])) {
            return 0;
        }
    }

#if (AVOID_TENG_HE_BUG == 1)
    if ((report->address[BD_ADDR_LEN - 1] == found_data[0]) &&
        ((found_data[0] & 0xF0) == 0xC0)) {
        return 0;
    }
#endif

    return -1;
}

int filter_dev_mac(const settings_t *settings, bd_addr_t addr)
{


    bd_addr_t filter_ff = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	//bd_addr_t filter_f1 = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF1};
#if(RELEASE_VERSION == RTK_VEISHENG_VERSION)
	uint8_t mac_mask = 0x04;
    if (settings->filter_mac_cnt > 0 && ((settings->filter_onoff&mac_mask) == mac_mask)) 
#else
	if (settings->filter_mac_cnt > 0) 
#endif
	{
        int i;
        //log_printf("%02x %02x %02x %02x %02x %02x \r\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
        if ((memcmp(settings->filter_macs[0], filter_ff, BD_ADDR_LEN) == 0) &&
            (addr[0] == 0xC4)) {
            return 0;
        }

        for (i = 0; i < settings->filter_mac_cnt; i++)
        {
            //log_printf("%02x %02x %02x %02x %02x %02x \r\n", settings->filter_macs[i][0], settings->filter_macs[i][1], settings->filter_macs[i][2], settings->filter_macs[i][3], settings->filter_macs[i][4], settings->filter_macs[i][5]);
            if (memcmp(settings->filter_macs[i], addr, BD_ADDR_LEN) == 0) {
                return 0;
            }
        }
        return -1;
    } else {
        return 0;
    }
}

int addr_is_connected(bd_addr_t addr_be)
{
    int i;

    for (i = 0; i < SLAVE_MAX_NUM; i++) {
        if (slave_lst[i].conn_handle != INVALID_HANDLE) {
            if (memcmp(slave_lst[i].addr, addr_be, BD_ADDR_LEN) == 0) {
                return 1;
            }
        }
    }
    return 0;
}

static uint8_t avoid_power_on_no_pin_adv = 0;

void rx_adv_data(const settings_t *settings, const le_ext_adv_report_t *report)
{
    bd_addr_t addr_be;
    #define ADV_RECORD_INFO_SIZE (1 + BD_ADDR_LEN + 2)
    if (adv_report_size + ADV_RECORD_INFO_SIZE + report->data_len > ADV_REPORT_BUFF_SIZE) {
        return;
    }
    
#if(RELEASE_VERSION != RTK_VEISHENG_VERSION)
    if (avoid_power_on_no_pin_adv == 0) {
        uint64_t rtc;
    
        rtc = RTC_Current();
        rtc = rtc / 32768;
        if (rtc < 10) {
            return;
        } else {
            avoid_power_on_no_pin_adv = 1;
        }
    }
#endif
//    reverse_bd_addr(report->address, addr_be);
//    print_addr(addr_be);

    if (filter_dev_type(settings, report) != 0) {
        return;
    }

    reverse_bd_addr(report->address, addr_be);

    if (filter_dev_mac(settings, addr_be) != 0) {
        return;
    }

    if (is_addr_in_report(addr_be)) {
        return;
    }

    if (addr_is_connected(addr_be)) {
        return;
    }

    adv_report_buffer[adv_report_size] = report->data_len + BD_ADDR_LEN + 2;
    adv_report_size += 1;

    memcpy(&adv_report_buffer[adv_report_size], addr_be, BD_ADDR_LEN);
    adv_report_size += BD_ADDR_LEN;

    adv_report_buffer[adv_report_size] = (report->rssi & 0xFF);
    adv_report_buffer[adv_report_size + 1] = ((report->rssi >> 8) & 0xFF);
    adv_report_size += 2;

    memcpy(&adv_report_buffer[adv_report_size], report->data, report->data_len);
    adv_report_size += report->data_len;

    send_response_id_frame(CTRL_CODE_PROACTIVE_REPORT, CMD_GET_FILTER_INFO, adv_report_size, adv_report_buffer);
}

uint8_t update_slave_role_conn_params(const conn_param_t *param)
{
    int i;
    int interval = param->interval * 1000 / 625;
    for (i = 0; i < MASTER_MAX_NUM; i++)
    {
        if (INVALID_HANDLE == peripheral_cfgs[i].conn_handle) continue;

        l2cap_request_connection_parameter_update(peripheral_cfgs[i].conn_handle,
            interval, interval, param->latency,
            param->sup_timer / 10);
    }

    return COMM_ERR_SUCC;
}

void ble_collect_rssi(void)
{
    int i;
    for (i = 0; i < MASTER_MAX_NUM; i++)
    {
        if (INVALID_HANDLE == peripheral_cfgs[i].conn_handle)
        {
            peripheral_cfgs[i].rssi_req = 0;
            continue;
        }
        gap_read_rssi(peripheral_cfgs[i].conn_handle);
        peripheral_cfgs[i].rssi_req = 1;
    }
    for (i = 0; i < SLAVE_MAX_NUM; i++)
    {
        if (INVALID_HANDLE == slave_lst[i].conn_handle)
        {
            slave_lst[i].rssi_req = 0;
            continue;
        }
        gap_read_rssi(slave_lst[i].conn_handle);
        slave_lst[i].rssi_req = 1;
    }
}

uint8_t link_rssi_report[1 + (BD_ADDR_LEN + 2) * (MASTER_MAX_NUM + SLAVE_MAX_NUM)] = {0};

void check_rssi_report(void)
{
    int i;
    for (i = 0; i < MASTER_MAX_NUM; i++)
    {
        if (INVALID_HANDLE == peripheral_cfgs[i].conn_handle)
        {
            peripheral_cfgs[i].rssi_req = 0;
            continue;
        }
        if (peripheral_cfgs[i].rssi_req) return;
    }
    for (i = 0; i < SLAVE_MAX_NUM; i++)
    {
        if (INVALID_HANDLE == slave_lst[i].conn_handle)
        {
            slave_lst[i].rssi_req = 0;
            continue;
        }
        if (slave_lst[i].rssi_req) return;
    }

    // all rssi request are done
    uint8_t *p = link_rssi_report;
    uint8_t mask = 0;
    for (i = 0; i < MASTER_MAX_NUM; i++)
    {
        if (INVALID_HANDLE == peripheral_cfgs[i].conn_handle) continue;
        mask |= 1 << i;
        memcpy(p, peripheral_cfgs[i].addr, BD_ADDR_LEN);
        p += BD_ADDR_LEN;
        p[0] = ((uint16_t)peripheral_cfgs[i].rssi) >> 8;
        p[1] = ((uint16_t)peripheral_cfgs[i].rssi) & 0xff;
        p += 2;
    }
    for (i = 0; i < SLAVE_MAX_NUM; i++)
    {
        if (INVALID_HANDLE == slave_lst[i].conn_handle) continue;

        mask |= 1 << (i + MASTER_MAX_NUM);
        memcpy(p, slave_lst[i].addr, BD_ADDR_LEN);
        p += BD_ADDR_LEN;
        p[0] = ((uint16_t)slave_lst[i].rssi) >> 8;
        p[1] = ((uint16_t)slave_lst[i].rssi) & 0xff;
        p += 2;
    }
    //link_rssi_report[0] = mask;
    send_response_id_frame(CTRL_CODE_RSP_GET_PARAM, CMD_LINK_RSSI, p - link_rssi_report, link_rssi_report);
}

void targets_updated(void)
{
    //disconnect_all();
    auto_connect(get_settings());
}

void peripheral_role_update(const settings_t *settings);

peripheral_cfg_t *get_peripheral_from_addr(const uint8_t *addr_le)
{
    int i;
    for (i = 0; i < MASTER_MAX_NUM; i++)
    {
        if (INVALID_HANDLE == peripheral_cfgs[i].conn_handle)
            continue;
        if (memcmp(peripheral_cfgs[i].addr, addr_le, BD_ADDR_LEN) == 0)
            return  peripheral_cfgs + i;
    }
    return NULL;
}

static void remove_target_from_list_by_mac(const uint8_t *addr)
{
    int target_index;
    dev_settings_t *targets = (dev_settings_t *)get_targets();
    bd_addr_t disc_dev_addr;
    reverse_bd_addr(addr, disc_dev_addr);
    for (target_index = 0; target_index < SLAVE_MAX_NUM; target_index++) {
        if (memcmp(targets[target_index].addr, disc_dev_addr, BD_ADDR_LEN) == 0) {
            memset(targets[target_index].addr, 0xFF, BD_ADDR_LEN);
            log_printf("rm target %02x ok\r\n", disc_dev_addr[0]);
        }
    }
}

static int add_target_to_list_by_mac(int slave_no, const uint8_t *addr)
{
    int index = slave_no - 1;
    dev_settings_t *targets = (dev_settings_t *)get_targets();
    bd_addr_t disc_dev_addr;
    slave_info_t *slave = &slave_lst[index];

    if (slave->conn_handle != INVALID_HANDLE)
    {
        return -1;
    } else {
        reverse_bd_addr(addr, disc_dev_addr);
        memcpy(targets[index].addr, disc_dev_addr, BD_ADDR_LEN);
        return 0;
    }
}

int i_am_slave_disconnect_master(bd_addr_t peer_addr)
{
    peripheral_cfg_t *p = get_peripheral_from_addr(peer_addr);
    log_printf("disc %02x %02x %02x %02x %02x %02x ", peer_addr[0], peer_addr[1], peer_addr[2], peer_addr[3], peer_addr[4], peer_addr[5]);
    if (p) { // i am slave
        gap_disconnect(p->conn_handle);
        log_printf("slv %d\r\n", p->conn_handle);
        return 0;
    }
    return -1;
}

int disconnect_device(const settings_t *settings, const uint8_t *addr)
{
    peripheral_cfg_t *p = get_peripheral_from_addr(addr);
    log_printf("disc %02x %02x %02x %02x %02x %02x ", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    if (p) { // i am slave
        gap_disconnect(p->conn_handle);
        log_printf("slv %d\r\n", p->conn_handle);
        return 0;
    } else {
        int index = get_target_index(settings, addr);
        log_printf("mas %d ", index);
        if ((index >= 0) && (index < SLAVE_MAX_NUM)) {
            slave_info_t *slave = &slave_lst[index];
            log_printf("hld %d ", slave->conn_handle);
            if (slave->conn_handle != INVALID_HANDLE) {
                remove_target_from_list_by_mac(addr);
                gap_disconnect(slave->conn_handle);
                return 0;
            }
        }
    }
    return -1;
}

int connect_slave_device(int slave_no, const settings_t *settings, const uint8_t *addr)
{
    log_printf("con %02x %02x %02x %02x %02x %02x ", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

    if ((slave_no < 1) ||
        (slave_no > SLAVE_MAX_NUM)) {
        return -1;
    }

    if (add_target_to_list_by_mac(slave_no, addr) != 0) {
        return -1;
    }

    auto_connect(get_settings());
    return 0;
}

static void del_bonding_info(int addr_type, const uint8_t *addr)
{
    int index;
    le_device_memory_db_t *dev = le_device_db_find(addr_type, addr, &index);
    if (dev)
        le_device_db_remove_key(index);
}

uint8_t bonding_ctrl(const settings_t *settings, uint8_t cmd, const uint8_t *addr)
{
    int flag = 1;
    int index = get_target_index(settings, addr);
    int addr_type = BD_ADDR_TYPE_LE_RANDOM;
    if (index >= 0)
    {
        slave_info_t *slave = &slave_lst[index];
        if (slave->conn_handle != INVALID_HANDLE)
        {
            slave->bonding_clear = 1;
            gap_disconnect(slave->conn_handle);
            log_printf("bnd dis r\n");
            return COMM_ERR_SUCC;
        }
        flag = 0;
    }
    else
    {
        peripheral_cfg_t *cfg = get_peripheral_from_addr(addr);
        if (cfg)
        {
            if (cfg->conn_handle != INVALID_HANDLE)
            {
                cfg->bonding_clear = 1;
                gap_disconnect(cfg->conn_handle);
                log_printf("bnd dis p\n");
                return COMM_ERR_SUCC;
            }
            addr_type = cfg->addr_type;
            flag = 0;
        }
    }

    if (flag)
        return COMM_ERR_BAD_TARGET_ADDR;

    del_bonding_info(addr_type, addr);
    return COMM_ERR_SUCC;
}

void send_to_central(peripheral_cfg_t *peripheral, const uint8_t *data, int len)
{
    uint16_t send_packet_len;
	int ret;
    if ((NULL == peripheral) || (0 == peripheral->notify_enable))
        return;
	#if (RELEASE_VERSION != RTK_VEISHENG_VERSION)
    peripheral->no_comm_timer_cnt = 0;
	#endif
    send_packet_len = att_server_get_mtu(peripheral->conn_handle) - 3;
    while (len)
    {
        int size = len > send_packet_len ? send_packet_len : len;
        ret = att_server_notify(peripheral->conn_handle,
                          HANDLE_DATE_NOTIFICATION,
                          (uint8_t *)data, size);
		log_printf("ntf_ret:%d\n", ret);

        data += size;
        len -= size;
    }
}

void send_to_dev(const uint8_t *addr_le, const uint8_t *data, int len)
{
    const settings_t *settings = get_settings();
    int i = get_target_index(settings, addr_le);
    if ((i >= 0) && (i < SLAVE_MAX_NUM))
    {
        uint16_t send_packet_len;
        slave_info_t *slave = slave_lst + i;
        if ((INVALID_HANDLE == slave->conn_handle) ||
            (INVALID_HANDLE == slave->rx_char.value_handle))
            return;

        gatt_client_get_mtu(slave->conn_handle, &send_packet_len);
        send_packet_len -= 3;

        while (len)
        {
            int size = len > send_packet_len ? send_packet_len : len;
            gatt_client_write_value_of_characteristic_without_response(slave->conn_handle,
                                                                       slave->rx_char.value_handle,
                                                                       size, (uint8_t *)data);

            data += size;
            len -= size;
        }
    }
    else
    {
        peripheral_cfg_t *peripheral = get_peripheral_from_addr(addr_le);
        send_to_central(peripheral, data, len);
    }
}

uint32_t get_sig_short_uuid(const uint8_t *uuid128)
{
    return uuid_has_bluetooth_prefix(uuid128) ? big_endian_read_32(uuid128, 0) : 0;
}

static void output_notification_handler(uint8_t packet_type, uint16_t channel, const uint8_t *packet, uint16_t size)
{
    slave_info_t *slave;
    const gatt_event_value_packet_t *value_packet;
    uint16_t value_size;
    switch (packet[0])
    {
    case GATT_EVENT_NOTIFICATION:
        value_packet = gatt_event_notification_parse(packet, size, &value_size);
        slave = get_slave_by_conn(channel);
        if (NULL == slave) break;

        ble_frame_rx_byte(&slave->comm_fsm, value_packet->value, value_size);
        break;
    }
}

void discover_complete(void)
{

}

void dummy_callback(uint8_t packet_type, uint16_t channel, const uint8_t *packet, uint16_t size)
{
    switch (packet[0])
    {
    case GATT_EVENT_QUERY_COMPLETE:
        log_printf("[%d]cmpl\n", get_slave_by_conn(gatt_event_query_complete_parse(packet)->handle)->id);
        discover_complete();
        break;
    }
}

static uint16_t char_config_notification = GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION;

void descriptor_discovery_callback(uint8_t packet_type, uint16_t _, const uint8_t *packet, uint16_t size)
{
    slave_info_t *slave;
    switch (packet[0])
    {
    case GATT_EVENT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT:
        {
            const gatt_event_all_characteristic_descriptors_query_result_t *result =
                gatt_event_all_characteristic_descriptors_query_result_parse(packet);
            slave = get_slave_by_conn(result->handle);
            if (get_sig_short_uuid(result->descriptor.uuid128) ==
                SIG_UUID_DESCRIP_GATT_CLIENT_CHARACTERISTIC_CONFIGURATION)
            {
                slave->noti_desc = result->descriptor;
                log_printf("[%d]output desc: %d\n", slave->id, slave->noti_desc.handle);
            }
        }
        break;
    case GATT_EVENT_QUERY_COMPLETE:
        slave = get_slave_by_conn(gatt_event_query_complete_parse(packet)->handle);
        if ((NULL == slave)
            || (gatt_event_query_complete_parse(packet)->status != 0)
            || (INVALID_HANDLE == slave->noti_desc.handle))
        {
            log_printf("descriptor not found, disc\n");
            gap_disconnect(gatt_event_query_complete_parse(packet)->handle);
            break;
        }

        gatt_client_write_characteristic_descriptor_using_descriptor_handle(dummy_callback, slave->conn_handle,
            slave->noti_desc.handle, sizeof(char_config_notification),
            (uint8_t *)&char_config_notification);
        gatt_client_listen_for_characteristic_value_updates(&slave->noti_notify, output_notification_handler,
                                                            slave->conn_handle, slave->noti_char.value_handle);
        break;
    }
}

void characteristic_discovery_callback(uint8_t packet_type, uint16_t _, const uint8_t *packet, uint16_t size)
{
    slave_info_t *slave;
    switch (packet[0])
    {
    case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
        {
            const gatt_event_characteristic_query_result_t *result =
                gatt_event_characteristic_query_result_parse(packet);
            slave = get_slave_by_conn(result->handle);
            if (memcmp(result->characteristic.uuid128, UUID_RX, sizeof(UUID_RX)) == 0)
            {
                slave->rx_char = result->characteristic;
                log_printf("[%d]rx handle: %d\n", slave->id, slave->rx_char.value_handle);
            }
            else if (memcmp(result->characteristic.uuid128, UUID_TX, sizeof(UUID_TX)) == 0)
            {
                slave->noti_char = result->characteristic;
                log_printf("[%d]tx handle: %d\n", slave->id, slave->noti_char.value_handle);
            }
        }
        break;
    case GATT_EVENT_QUERY_COMPLETE:
        slave = get_slave_by_conn(gatt_event_query_complete_parse(packet)->handle);
        if ((NULL == slave)
            || (gatt_event_query_complete_parse(packet)->status != 0)
            || (INVALID_HANDLE == slave->noti_char.value_handle))
        {
            log_printf("characteristic not found, disc\n");
            gap_disconnect(gatt_event_query_complete_parse(packet)->handle);
            break;
        }

        gatt_client_discover_characteristic_descriptors(descriptor_discovery_callback,
                                                        slave->conn_handle, &slave->noti_char);
        break;
    }
}

void service_discovery_callback(uint8_t packet_type, uint16_t _, const uint8_t *packet, uint16_t size)
{
    slave_info_t *slave;
    switch (packet[0])
    {
    case GATT_EVENT_SERVICE_QUERY_RESULT:
        {
            slave = get_slave_by_conn(gatt_event_service_query_result_parse(packet)->handle);
            slave->meter_service = gatt_event_service_query_result_parse(packet)->service;
            log_printf("[%d]service handle: %d %d\n",
                    slave->id, slave->meter_service.start_group_handle, slave->meter_service.end_group_handle);
        }
        break;
    case GATT_EVENT_QUERY_COMPLETE:
        slave = get_slave_by_conn(gatt_event_query_complete_parse(packet)->handle);
        if ((NULL == slave)
            || (gatt_event_query_complete_parse(packet)->status != 0)
            || (INVALID_HANDLE == slave->meter_service.start_group_handle))
        {
            log_printf("service not found, disc\n");
            gap_disconnect(gatt_event_query_complete_parse(packet)->handle);
            break;
        }

        log_printf("[%d]disc char\n", slave->id);

        if (slave->id == 6)
            slave->id = 6;
        gatt_client_discover_characteristics_for_service(characteristic_discovery_callback, slave->conn_handle,
                                                       slave->meter_service.start_group_handle,
                                                       slave->meter_service.end_group_handle);
        break;
    }
}

const static ext_adv_set_en_t adv_sets_en[] = { {.handle = 0, .duration = 0, .max_events = 0} };

void update_local_addr(const settings_t *settings)
{
    gap_set_random_device_address(settings->local.addr);
    memcpy(sm_persistent.identity_addr, settings->local.addr, sizeof(sm_persistent.identity_addr));
}

void local_adv_ctrl(uint8_t enable)
{
	log_printf("host enable adv\n");
    gap_set_ext_adv_enable(enable, sizeof(adv_sets_en) / sizeof(adv_sets_en[0]), adv_sets_en);
}

void printf_adv_data(uint16_t length, const uint8_t *data)
{
    int i;
    log_printf("adv ");
    for (i = 0; i < length; i++) {
        log_printf("%02x ", data[i]);
    }
    log_printf("\r\n");
}

void peripheral_role_enable(const settings_t *settings)
{
    int len = 0;
    const uint8_t *adv_data;
    if (is_invalid_mac(settings->local.addr))
        return;
	log_printf("peripheral role enable\n");
    adv_data = get_adv_data(&len);

    gap_set_adv_set_random_addr(0, settings->local.addr);
    gap_set_ext_adv_para(0,
                        CONNECTABLE_ADV_BIT | SCANNABLE_ADV_BIT | LEGACY_PDU_BIT,
						#if ((RELEASE_VERSION == RTK_XUJI_VERSION)||(RELEASE_VERSION == RTK_VEISHENG_VERSION))
						settings->adv_interval/0.625, settings->adv_interval/0.625,  
						#else
                        96, 96,                  // Primary_Advertising_Interval_Min, Primary_Advertising_Interval_Max
                        #endif
						//settings->adv_interval, settings->adv_interval,                  // Primary_Advertising_Interval_Min, Primary_Advertising_Interval_Max
                        0x7,                       // Primary_Advertising_Channel_Map
                        BD_ADDR_TYPE_LE_RANDOM,    // Own_Address_Type
                        BD_ADDR_TYPE_LE_PUBLIC,    // Peer_Address_Type (ignore)
                        NULL,                      // Peer_Address      (ignore)
                        ADV_FILTER_ALLOW_ALL,      // Advertising_Filter_Policy
                        tx_power_level_mapping[settings->tx_power], // Advertising_Tx_Power
                        PHY_1M,                    // Primary_Advertising_PHY
                        0,                         // Secondary_Advertising_Max_Skip
                        PHY_1M,                    // Secondary_Advertising_PHY
                        0x00,                      // Advertising_SID
                        0x00);                     // Scan_Request_Notification_Enable
    //printf_adv_data(len, adv_data);
    gap_set_ext_adv_data(0, len, adv_data);
    gap_set_ext_scan_response_data(0, g_scan_data_len, (uint8_t*)g_scan_data);
    local_adv_ctrl(1);
}

void peripheral_role_update(const settings_t *settings)
{
    int i;

    local_adv_ctrl(0);

    if (is_ble_off == 1) {
        return;
    }
	
	log_printf("peripheral role update\n");

    if (settings->local.addr_changed != 0) {
        for (i = 0; i < MASTER_MAX_NUM; i++)
        {
            if (peripheral_cfgs[i].conn_handle != INVALID_HANDLE) {
                gap_disconnect(peripheral_cfgs[i].conn_handle);
                log_printf("peripheral_role_update dis p\n");
            }
        }
        peripheral_role_enable(settings);
        return;
    } else {
        for (i = 0; i < MASTER_MAX_NUM; i++)
        {
        	//if(i!=0)	continue;
            if (peripheral_cfgs[i].conn_handle == INVALID_HANDLE) {
                peripheral_role_enable(settings);
                log_printf("peripheral_role_update dis r\n");
                return;
            }
        }
    }
}

uint8_t v_to_ch(uint64_t v)
{
    return v < 10 ? v - 0 + '0' : v - 10 + 'A';
}

void set_local_name(const uint8_t *name, int len)
{
    // TODO: we need to confirm the name length
    // for lagecy adv, the total length is limited to 31 bytes
    memcpy(g_adv_data +  ADVERTISING_ITEM_OFFSET_COMPLETE_LOCAL_NAME, name, 3);
}

const uint8_t *get_local_name(int *len)
{
    *len = 3;
    return g_adv_data +  ADVERTISING_ITEM_OFFSET_COMPLETE_LOCAL_NAME;
}

void local_mac_updated(const settings_t *settings)
{
#if(RELEASE_VERSION == RTK_VEISHENG_VERSION)
uint64_t biao_addr = 0;
uint8_t bt_mac[6];
uint8_t i;
reverse_bd_addr(settings->local.addr, bt_mac);
PRINT("bt_addr:");
PRINT("%02x-%02x-%02x-%02x-%02x-%02x\r\n",
    bt_mac[0],bt_mac[1],bt_mac[2],
    bt_mac[3],bt_mac[4],bt_mac[5]
    );

//for( i=0;i<6;i++)
//PRINT("0x%02x",bt_mac[i]);

memcpy((uint8_t *)&biao_addr, bt_mac, BD_ADDR_LEN-1 );
uint8_t *p = (uint8_t *)&biao_addr;
for( i=0;i<6;i++)
    PRINT("0x%02x--",*p++);


#else
    uint16_t biao_addr = settings->local.addr[5] | (settings->local.addr[4] << 8);
#endif

    // update adv_data
//    g_adv_data[ADVERTISING_ITEM_OFFSET_COMPLETE_LOCAL_NAME + 2] = v_to_ch(settings->local.addr[5] & 0xf);
//    g_adv_data[ADVERTISING_ITEM_OFFSET_COMPLETE_LOCAL_NAME + 1] = v_to_ch(settings->local.addr[5] >> 4);
//    g_adv_data[ADVERTISING_ITEM_OFFSET_COMPLETE_LOCAL_NAME + 0] = v_to_ch(settings->local.addr[4] & 0xf);
    g_adv_data[ADVERTISING_ITEM_OFFSET_COMPLETE_LOCAL_NAME + 2] = v_to_ch(biao_addr % 10);
    biao_addr = biao_addr / 10;
    g_adv_data[ADVERTISING_ITEM_OFFSET_COMPLETE_LOCAL_NAME + 1] = v_to_ch(biao_addr % 10);
    biao_addr = biao_addr / 10;
    g_adv_data[ADVERTISING_ITEM_OFFSET_COMPLETE_LOCAL_NAME + 0] = v_to_ch(biao_addr % 10);

    memcpy(g_scan_data + g_scan_data_len - 6, settings->local.addr, BD_ADDR_LEN);

    update_local_addr(settings);

    peripheral_role_update(settings);
}

void adv_data_updated(const settings_t *settings)
{
    peripheral_role_update(settings);
}

void send_encapsulated_dt698_645_frame(dt698_645_frame_t *frame, const uint8_t *addr)
{
    uint8_t opcode = 0;
#if(RELEASE_VERSION == RTK_VEISHENG_VERSION)
    opcode = CTRL_CODE_MODULE2METER;
#else
    opcode = CTRL_CODE_METER2MODULE;
#endif
    send_encapsulated_frame(opcode, &(frame->header), frame->frame_len, addr);

}

void slave_rx_frame(slave_info_t *slave, dt698_645_frame_t *frame)
{
    const settings_t *settings = get_settings();
    bd_addr_t addr;

#if (RELEASE_VERSION != RTK_VEISHENG_VERSION)
    reverse_bd_addr(slave->addr, addr);
    send_encapsulated_dt698_645_frame(frame, addr);
#else	
    send_encapsulated_dt698_645_frame(frame, slave->addr);
#endif
    dt698_645_frame_free(frame);
}

void peripheral_rx_frame(peripheral_cfg_t *peripheral, dt698_645_frame_t *frame)
{
    send_encapsulated_dt698_645_frame(frame, peripheral->addr);
    dt698_645_frame_free(frame);
}

io_capability_t smp_level_to_io_cap(uint8_t level)
{
    switch (level)
    {
    case SMP_MIMT_KEY:
        return IO_CAPABILITY_DISPLAY_ONLY;
    default:
        return IO_CAPABILITY_NO_INPUT_NO_OUTPUT;
    }
}

void i_am_master_connected_slave(const settings_t *settings, int slave_id, const le_meta_event_enh_create_conn_complete_t *conn_complete)
{
    const dev_settings_t *targets = get_targets();
    slave_info_t *slave = &slave_lst[slave_id];

    slave->conn_handle = conn_complete->handle;
    reverse_bd_addr(conn_complete->peer_addr, slave->addr);
    slave->meter_service.start_group_handle = INVALID_HANDLE;
    slave->noti_char.value_handle = INVALID_HANDLE;
    slave->noti_desc.handle = INVALID_HANDLE;

    adv_report_size = 0;

    dt698_645_frame_create(&slave->comm_fsm, slave, (f_rx_dt698_645_frame)slave_rx_frame);

    if (targets[slave_id].smp_level == SMP_MIMT_KEY) {
        log_printf("sm con\n");
        sm_config_conn(conn_complete->handle,
                      //smp_level_to_io_cap(targets[slave_id].smp_level),
                      IO_CAPABILITY_KEYBOARD_ONLY,
                      SM_AUTHREQ_MITM_PROTECTION);
        //sm_request_pairing(conn_complete->handle);
    }

	else{
	
	sm_config_conn(conn_complete->handle,
					IO_CAPABILITY_NO_INPUT_NO_OUTPUT,
				  	SM_AUTHREQ_MITM_PROTECTION);
	}

    if (!gatt_client_is_ready(conn_complete->handle))
    {
        platform_raise_assertion("profile.c", __LINE__);
        return;
    }
    //gap_update_connection_parameters(conn_complete->handle, 120, 120, 0, 600, 10, 10);
#if (RELEASE_VERSION != RTK_VEISHENG_VERSION)
    if (slave->addr[0] == 0xC4) {
        gap_update_connection_parameters(conn_complete->handle, 32, 64, 0, I_AM_MASTER_TIMEOUT, I_AM_MASTER_CE_LEN, I_AM_MASTER_CE_LEN);
    } else {
        gap_update_connection_parameters(conn_complete->handle, I_AM_MASTER_CON_MIN_INTERVAL, I_AM_MASTER_CON_MAX_INTERVAL, 0, I_AM_MASTER_TIMEOUT, I_AM_MASTER_CE_LEN, I_AM_MASTER_CE_LEN);
    }
#endif
    gatt_client_discover_primary_services_by_uuid128(service_discovery_callback,
                                                         conn_complete->handle, UUID_METER);
    log_printf("i am master conn slv %d hld %d\n", slave->id, conn_complete->handle);
}

#define USER_MSG_RX_FRAME           1
#define USER_MSG_REPORT_RESET       2
#define USER_MSG_REPORT_ADV         3
#define USER_MSG_INITIATE_TIMOUT    5
#define USER_MSG_BLE_OFF            6
#define USER_MSG_DISCONNECT_DEV     7
#define USER_MSG_UPDATA_CON_PARAM	8

static void conn_timer_callback(UINT32 xTimer)
{
    btstack_push_user_msg(USER_MSG_INITIATE_TIMOUT, NULL, 0);
}

static void reset_timer_callback(UINT32 xTimer)
{
    printf("reset_timer_callback\r\n");
    btstack_push_user_msg(USER_MSG_REPORT_RESET, NULL, 0);
}

static void ble_off_timer_callback(UINT32 xTimer)
{
    btstack_push_user_msg(USER_MSG_BLE_OFF, NULL, 0);
}

static void adv_report_callback(UINT32 xTimer)
{
    btstack_push_user_msg(USER_MSG_REPORT_ADV, NULL, 0);
}

void app_rx_frame(void *user_data, simple_frame_t *frame)
{
    btstack_push_user_msg(USER_MSG_RX_FRAME, frame, 0);
}

static int slave_con_state[SLAVE_MAX_NUM] = {0};
static int master_con_state[MASTER_MAX_NUM] = {0};
void peripheral_on_disc(const settings_t *settings, const hci_con_handle_t conn_handle)
{
    int i;
    for (i = 0; i < MASTER_MAX_NUM; i++)
    {
        if (conn_handle == peripheral_cfgs[i].conn_handle)
        {	
        	
            if (peripheral_cfgs[i].bonding_clear) {
                log_printf("del bond info\n");
                del_bonding_info(peripheral_cfgs[i].addr_type, peripheral_cfgs[i].addr);
            }
            peripheral_cfgs[i].conn_handle = INVALID_HANDLE;
            peripheral_cfgs[i].notify_enable = 0;
			
			#ifdef LOG_FLASH_ENABLE
            log_flash_write(LOG_FLASH_TLV_DISCCONNECT_EVENT, 6, peripheral_cfgs[i].addr);
			#endif
	#if (RELEASE_VERSION != RTK_VEISHENG_VERSION)
	master_con_state[i] = 0;
    platform_printf("[%d%d:%d%d%d]", master_con_state[0], master_con_state[1], slave_con_state[0], slave_con_state[1], slave_con_state[2]);
   	#endif
	         break;
        }
    }
    peripheral_role_update(settings);
}

void ble_off(void)
{
    is_ble_off = 1;
    local_adv_ctrl(0);
    disconnect_all();
}

void ble_on(void)
{
    if (is_ble_off == 0) return;
	log_printf("**ble on**\n");
    is_ble_off = 0;
    local_adv_ctrl(1);
    auto_connect(get_settings());
}

void ble_off_ctrl(uint8_t ctrl)
{
    LOS_SwtmrStop(ble_off_timer);
	log_printf("ble ctrl: %d\n", ctrl);
    if (ctrl == 0)
    {
        ble_off();
    }
    else if (ctrl == 0xff)
    {
        ble_on();
    }
    else
    {
        ble_on();
        //xTimerChangePeriod(ble_off_timer, pdMS_TO_TICKS(ctrl * 60 * 1000), portMAX_DELAY);
        LOS_SwtmrStart(ble_off_timer);
    }
}
#if (RELEASE_VERSION != RTK_VEISHENG_VERSION)
static int global_master_comm_timeout_en = 0;
void master_comm_time_out_stop(void)
{
    if (global_master_comm_timeout_en != 0) {
        xTimerStop(master_comm_timeout_timer, portMAX_DELAY);
    }
    global_master_comm_timeout_en = 0;
    return;
}

void master_comm_time_out_start(uint16_t time_out)
{
    int i;

    if (global_master_comm_timeout_en == 0) {
        xTimerStart(master_comm_timeout_timer, portMAX_DELAY);
    }
    global_master_comm_timeout_en = 1;

    for (i = 0; i < MASTER_MAX_NUM; i++) {
        peripheral_cfgs[i].no_comm_timer_cnt = 0;
        peripheral_cfgs[i].comm_time_out = time_out;
    }
    return;
}

#define MASTER_COMM_TIMEOUT_TIMER_PERIOD 5 // second
static void master_comm_timeout_timer_callback(TimerHandle_t xTimer)
{
    int i;

    for (i = 0; i < MASTER_MAX_NUM; i++) {
        if (peripheral_cfgs[i].conn_handle == INVALID_HANDLE) {
            continue;
        } else if (peripheral_cfgs[i].comm_time_out_en == 0) {
            continue;
        } else {
            peripheral_cfgs[i].no_comm_timer_cnt++;
            if ((peripheral_cfgs[i].no_comm_timer_cnt * MASTER_COMM_TIMEOUT_TIMER_PERIOD) >= peripheral_cfgs[i].comm_time_out) {
                btstack_push_user_msg(USER_MSG_DISCONNECT_DEV, &(peripheral_cfgs[i].conn_handle), sizeof(peripheral_cfgs[i].conn_handle));
            }
        }
    }
}
#endif
static void report_conn_status(const settings_t *settings)
{
    extern conn_dev_info_t g_conn_dev_info;
    int cnt = get_connected_all_mac(settings, &g_conn_dev_info);
    send_response_id_frame(CTRL_CODE_PROACTIVE_REPORT, CMD_BLE_CONN_INFO, 1 + cnt * 6, &g_conn_dev_info);
}

static void update_conn_status(const settings_t *settings)
{
    int i;
    int flag = 0;
    report_conn_status(settings);

    for (i = 0; i < MASTER_MAX_NUM; i++)
    {
        if (INVALID_HANDLE != peripheral_cfgs[i].conn_handle)
        {
            flag = 1;
            break;
        }
    }

    if (flag)
        goto show_status;

    for (i = 0; i < SLAVE_MAX_NUM; i++)
    {
        if (INVALID_HANDLE != slave_lst[i].conn_handle)
        {
            flag = 1;
            break;
        }
    }

show_status:
    connection_state(flag);
}

uint8_t i_am_slave_connected_master(const le_meta_event_enh_create_conn_complete_t *conn_complete)
{
    int i;
    for (i = 0; i < MASTER_MAX_NUM; i++)
    {
        if (INVALID_HANDLE == peripheral_cfgs[i].conn_handle)
        {
            settings_t *settings = get_settings();
            peripheral_cfgs[i].conn_handle = conn_complete->handle;
            peripheral_cfgs[i].notify_enable = 0;
            peripheral_cfgs[i].addr_type = conn_complete->peer_addr_type;
	#if (RELEASE_VERSION != RTK_VEISHENG_VERSION)			
            peripheral_cfgs[i].no_comm_timer_cnt = 0;
            peripheral_cfgs[i].comm_time_out_en = global_master_comm_timeout_en;
	#endif
            memcpy(peripheral_cfgs[i].addr, conn_complete->peer_addr, BD_ADDR_LEN);
            dt698_645_frame_create(&peripheral_cfgs[i].comm_fsm, &peripheral_cfgs[i], (f_rx_dt698_645_frame)peripheral_rx_frame);

			#if ((RELEASE_VERSION == RTK_XUJI_VERSION)||(RELEASE_VERSION == RTK_VEISHENG_VERSION))
			 printf("==sm==:%x\r\n",settings->local.smp_level);
			if (settings->local.smp_level == SMP_MIMT_KEY)
            {
                sm_config_conn(conn_complete->handle,
                               //smp_level_to_io_cap(settings->local.smp_level),
                               IO_CAPABILITY_DISPLAY_ONLY,
                               SM_AUTHREQ_NO_BONDING | SM_AUTHREQ_MITM_PROTECTION);
                sm_request_pairing(conn_complete->handle);
            }

			else
            	{

                sm_config_conn(conn_complete->handle,
                   IO_CAPABILITY_NO_INPUT_NO_OUTPUT,
                   SM_AUTHREQ_NO_BONDING | SM_AUTHREQ_MITM_PROTECTION);
			}

			#else
            if (settings->local.smp_level == SMP_MIMT_KEY)
            {
                sm_config_conn(conn_complete->handle,
                               //smp_level_to_io_cap(settings->local.smp_level),
                               IO_CAPABILITY_DISPLAY_ONLY,
                               SM_AUTHREQ_BONDING | SM_AUTHREQ_MITM_PROTECTION);
                //sm_request_pairing(conn_complete->handle);
            }
            //gap_update_connection_parameters(conn_complete->handle, I_AM_MASTER_CON_MIN_INTERVAL, I_AM_MASTER_CON_MAX_INTERVAL, 0, I_AM_MASTER_TIMEOUT, I_AM_MASTER_CE_LEN, I_AM_MASTER_CE_LEN);

            master_con_state[i] = 1;
            platform_printf("[%d%d:%d%d%d]", master_con_state[0], master_con_state[1], slave_con_state[0], slave_con_state[1], slave_con_state[2]);
			#endif

            return i;
        }
    }

    gap_disconnect(conn_complete->handle);
    log_printf("i_am_slave_connected_master dis %d\n", conn_complete->handle);
    return 0xff;
}

static void user_msg_handler(uint32_t msg_id, void *data, uint16_t size)
{
    switch (msg_id) {
    case USER_MSG_RX_FRAME:
        handle_frame((simple_frame_t *)data);
        comm_frame_free((simple_frame_t *)data);
        break;
    case USER_MSG_REPORT_RESET:
        {

            uint8_t len = 1;
            uint8_t dar[27]={0x01,};
        #if (RELEASE_VERSION == RTK_VEISHENG_VERSION)
            memcpy(&dar[1], module_sn, sizeof(module_sn));
            memcpy(&dar[21],module_ver,sizeof(module_ver));
            len = sizeof(dar);
		#endif
            send_response_id_frame(CTRL_CODE_PROACTIVE_REPORT, CMD_MODULE_INFO, len, dar);
        }
        break;
    case USER_MSG_BLE_OFF:
        ble_off();
        break;
    case USER_MSG_INITIATE_TIMOUT:
        log_printf("initiate timeout 0x%x\n", initiating_id);
        if (initiating_id != INITIATING_OFF) {
            cancel_initiating();
        }
        break;
	case USER_MSG_UPDATA_CON_PARAM:
		log_printf("host: updata con param handle:%d\n", size);
		l2cap_request_connection_parameter_update(size, 24, 24, 0, 400);
	break;
	#if (RELEASE_VERSION == RTK_VEISHENG_VERSION)
    case USER_MSG_REPORT_ADV:
        send_response_id_frame(CTRL_CODE_PROACTIVE_REPORT, CMD_GET_FILTER_INFO, adv_report_size, adv_report_buffer);
        break;
	#endif
	
	#if ((RELEASE_VERSION != RTK_XUJI_VERSION)&&(RELEASE_VERSION != RTK_VEISHENG_VERSION))
    case USER_MSG_DISCONNECT_DEV: {
        hci_con_handle_t handle = INVALID_HANDLE;
        if (size != sizeof(hci_con_handle_t)) {
            break;
        }
        handle = *(hci_con_handle_t *)data;
        if (handle != INVALID_HANDLE) {
            log_printf("disconnect %d\n", handle);
            gap_disconnect(handle);
        }
    }
    break;
	#endif
    default:
    break;
    }
}

void reset_responded(void)
{
    LOS_SwtmrStop(reset_timer);
}

void report_reset_status(void)
{
    extern const char module_ver[6];
    uint8_t len = 1;
    LOS_SwtmrStart(reset_timer);	
    uint8_t dar[27]={0x01,};
#if (RELEASE_VERSION == RTK_VEISHENG_VERSION)
	{
    memcpy(&dar[1], module_sn, sizeof(module_sn));
    memcpy(&dar[21],module_ver,sizeof(module_ver));
    len = sizeof(dar);
    }
#endif
    send_response_id_frame(CTRL_CODE_PROACTIVE_REPORT, CMD_MODULE_INFO, len, dar);
}

uint16_t updata_con_param_handle = 0xffff;
void updata_con_param(void)
{
	log_printf("updata handle=%d\n", updata_con_param_handle);
	btstack_push_user_msg(USER_MSG_UPDATA_CON_PARAM, NULL, updata_con_param_handle);
	
}

static void user_packet_handler(uint8_t packet_type, uint16_t channel, const uint8_t *packet, uint16_t size)
{
    const le_meta_event_enh_create_conn_complete_t *conn_complete;
    const event_disconn_complete_t *disconn_event;
    slave_info_t *slave;
    uint8_t event = hci_event_packet_get_type(packet);
    const btstack_user_msg_t *p_user_msg;
    const settings_t *settings;
    if (packet_type != HCI_EVENT_PACKET) return;

    settings = get_settings();

    switch (event)
    {
    case BTSTACK_EVENT_STATE:
        if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING)
            break;
	    log_printf("BTSTACK_EVENT_STATE ll_heap size=%d\r\n",ll_get_heap_free_size());
        ll_set_max_conn_number(SLAVE_MAX_NUM + MASTER_MAX_NUM);
#if (RELEASE_VERSION == RTK_VEISHENG_VERSION)
        local_mac_updated(settings);
#endif
        break;

    case HCI_EVENT_LE_META:
        switch (hci_event_le_meta_get_subevent_code(packet))
        {
        case HCI_SUBEVENT_LE_ENHANCED_CONNECTION_COMPLETE:
            
            conn_complete = decode_hci_le_meta_event(packet, le_meta_event_enh_create_conn_complete_t);
		log_printf("\n\nCONNECT! status:%d handle:%d interval:%d latency:%d sup_timeout:%d\n\n", 
				conn_complete->status, conn_complete->handle, conn_complete->interval, conn_complete->latency, conn_complete->sup_timeout);
            if (0 != conn_complete->status) {
                gap_is_connection_cancel = 0;
                initiating_id = INITIATING_OFF;
                auto_connect(settings);
                break;
            }
			
			#ifdef LOG_FLASH_ENABLE
            log_flash_write(LOG_FLASH_TLV_CONNECT_EVENT, 6, conn_complete->peer_addr);
			#endif
            gatt_client_is_ready(conn_complete->handle);

            ll_set_conn_tx_power(conn_complete->handle, tx_power_level_mapping[settings->tx_power]);

            if (HCI_ROLE_SLAVE == conn_complete->role) { // i am slave
                if (0 == conn_complete->status) {
                    att_set_db(conn_complete->handle, profile_data);
                    ll_hint_on_ce_len(conn_complete->handle, 10, 15);
                    i_am_slave_connected_master(conn_complete);
                }
            } else {
                if (INITIATING_AUTO == initiating_id) {
                    initiating_id = get_target_index(settings, conn_complete->peer_addr);
                }

                if (initiating_id < SLAVE_MAX_NUM) {
                    slave_con_state[initiating_id] = 1;
                    platform_printf("[%d%d:%d%d%d]", master_con_state[0], master_con_state[1], slave_con_state[0], slave_con_state[1], slave_con_state[2]);
                } else {
                    gap_disconnect(conn_complete->handle);
                    log_printf("HCI_EVENT_LE_META dis %d\n", conn_complete->handle);
                    break;
                }

                print_addr(conn_complete->peer_addr);
                i_am_master_connected_slave(settings, initiating_id, conn_complete);
                log_printf("con slv %d %d\r\n", initiating_id, slave_lst[initiating_id].conn_handle);
                //gap_set_phy(conn_complete->handle, 0, PHY_2M_BIT, PHY_2M_BIT, HOST_NO_PREFERRED_CODING);
                initiating_id = INITIATING_OFF;
                LOS_SwtmrStop(create_conn_timer);
                auto_connect(settings);
            }

            report_conn_status(settings);
            connection_state(1);
            log_printf("role = %d, handle = %d interval %.2f ms\n", conn_complete->role, conn_complete->handle, conn_complete->interval * 1.25);
#if DEBUG_DISCONNECT
			updata_con_param_handle = conn_complete->handle;
			platform_set_timer(updata_con_param, 3000/0.625);
#endif
			break;

        case HCI_SUBEVENT_LE_ADVERTISING_SET_TERMINATED:
			log_printf("ADVE_SET_TERMINATED\n");
            peripheral_role_update(settings);
            break;

        case HCI_SUBEVENT_LE_EXTENDED_ADVERTISING_REPORT:
            {
                const le_ext_adv_report_t *report = decode_hci_le_meta_event(packet, le_meta_event_ext_adv_report_t)->reports;

                rx_adv_data(settings, report);

            }
            break;
        case HCI_SUBEVENT_LE_CONNECTION_UPDATE_COMPLETE:
            {
                const le_meta_event_conn_update_complete_t *up_con = decode_hci_le_meta_event(packet, le_meta_event_conn_update_complete_t);
				log_printf("***UP CON PARAM: %d-%d-%d%d\n***", 
					up_con->handle ,up_con->interval, up_con->latency, up_con->sup_timeout);
            }

        default:
            break;
        }

        break;

    case HCI_EVENT_COMMAND_COMPLETE:
        {
            if (hci_event_command_complete_get_command_opcode(packet) == OPCODE_READ_RSSI)
            {
                const read_rssi_complete_t *cmpl =
                    (const read_rssi_complete_t *)hci_event_command_complete_get_return_parameters(packet);
                slave = get_slave_by_conn(cmpl->handle);
                if (slave)
                {
                    slave->rssi = cmpl->rssi;
                    slave->rssi_req = 0;
                }
                else
                {
                    peripheral_cfg_t *p = peripheral_from_conn_handle(cmpl->handle);
                    if (p)
                    {
                        p->rssi = cmpl->rssi;
                        p->rssi_req = 0;
                    }
                }

                check_rssi_report();


            }
        }
        break;

    case HCI_EVENT_DISCONNECTION_COMPLETE:
        
        disconn_event = decode_hci_event_disconn_complete(packet);
		log_printf("DISCON! reason=0x%x, handle=%d\n", disconn_event->reason, disconn_event->conn_handle);
        slave = get_slave_by_conn(disconn_event->conn_handle);
		
		#ifdef LOG_FLASH_ENABLE
        log_flash_write(LOG_FLASH_TLV_DISCCONNECT_EVENT, sizeof(*disconn_event), (const uint8_t *)disconn_event); // #define ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION       0x13
        #endif
		if (slave) { // i am master
            slave_con_state[slave->id] = 0;
            platform_printf("[%d%d:%d%d%d]", master_con_state[0], master_con_state[1], slave_con_state[0], slave_con_state[1], slave_con_state[2]);
			#ifdef LOG_FLASH_ENABLE
            log_flash_write(LOG_FLASH_TLV_DISCCONNECT_EVENT, 6, slave->addr);
			#endif
            if (slave->bonding_clear)
            {
                log_printf("del bond info\n");
                del_bonding_info(BD_ADDR_TYPE_LE_RANDOM, slave->addr);
            }

            slave->conn_handle = INVALID_HANDLE;
            auto_connect(settings);
        } else {
            peripheral_on_disc(settings, disconn_event->conn_handle);
        }
        update_conn_status(settings);
        break;

    case L2CAP_EVENT_CAN_SEND_NOW:
        // add your code
        break;

    case BTSTACK_EVENT_USER_MSG:
        p_user_msg = hci_event_packet_get_user_msg(packet);
        user_msg_handler(p_user_msg->msg_id, p_user_msg->data, p_user_msg->len);
        break;

    default:
        break;
    }
}

uint32_t get_passkey(dev_settings_t *dev)
{
    if (dev->pair_len >= sizeof(dev->paring_code))
        return 0;
    dev->paring_code[dev->pair_len] = '\0';
    return atoi(dev->paring_code);
}

uint32_t get_stored_passkey(hci_con_handle_t handle)
{
    slave_info_t *slave = get_slave_by_conn(handle);
    if (slave)
    {
        dev_settings_t * targets = (dev_settings_t *)get_targets();
        return get_passkey(&targets[slave->id]);
    }
    else
        return get_passkey(&get_settings()->local);
}

static void sm_packet_handler(uint8_t packet_type, uint16_t channel, const uint8_t *packet, uint16_t size)
{
    uint8_t event = hci_event_packet_get_type(packet);

    if (packet_type != HCI_EVENT_PACKET) return;

    log_printf("sm 0x%02x\n", event);
    switch (event)
    {
    case SM_EVENT_JUST_WORKS_REQUEST:
        PRINT("JUSTWORK");
        sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
        break;
    case SM_EVENT_PASSKEY_INPUT_NUMBER:
        {
            hci_con_handle_t handle = sm_event_passkey_input_number_get_handle(packet);
            uint32_t key2 = get_stored_passkey(handle);
            log_printf("PASSKEY_INPUT_NUMBER: %6d\n", key2);
            sm_passkey_input(handle, key2);
        }
        break;
    case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
        {
            hci_con_handle_t handle = sm_event_passkey_display_number_get_handle(packet);
            uint32_t key1 = sm_event_passkey_display_number_get_passkey(packet);
            uint32_t key2 = get_stored_passkey(handle);
            log_printf("PASSKEY_DISPLAY: override %6d -> %6d\n", key1, key2);
            sm_passkey_input(handle, key2);
        }
        break;
    case SM_EVENT_PASSKEY_DISPLAY_CANCEL:
        log_printf("DISPLAY_CANCEL\n");
        break;
    case SM_EVENT_STATE_CHANGED:
        {
            const sm_event_state_changed_t *state_changed = decode_hci_event(packet, sm_event_state_changed_t);
            switch (state_changed->reason)
            {
                case SM_STARTED:
                    log_printf("SM: STARTED\n");
                    break;
                case SM_FINAL_PAIRED:
                    log_printf("SM: PAIRED\n");
                    break;
                case SM_FINAL_REESTABLISHED:
                    log_printf("SM: REESTABLISHED");
//                    if (0 == att_handle_notify)
//                    {
//                        log_printf(" BUT LOCAL INFO DELETED");
//                    }
                    log_printf("\n");
                    break;
                case SM_FINAL_FAIL_TIMEOUT:
                    log_error("SM: SM_FINAL_FAIL_TIMEOUT\n");
                    break;
                default:
                    gap_disconnect(state_changed->conn_handle);
                    log_error("SM: FINAL ERROR: %d %d\n", state_changed->conn_handle, state_changed->reason);
                    break;
            }
        }

        break;
    default:
        break;
    }
}

extern void config_uart(void);
comm_fsm_t uart_comm;
static btstack_packet_callback_registration_t sm_event_callback_registration  = {.callback = &sm_packet_handler};

uint32_t setup_profile(void *data, void *user_data)
{
	
	
	printf("ll_heap size=%d\r\n",ll_get_heap_free_size());
	//printf("ll_malloc(25*1024)=%p\r\n",ll_malloc(25*1024));
    if (ll_malloc(21*1024) != (void *)0x400a84bc)
       platform_raise_assertion(__FILE_NAME__, __LINE__);
	printf("ll_heap size=%d\r\n",ll_get_heap_free_size());
	
    int i;
	#if(RELEASE_VERSION != RTK_VEISHENG_VERSION)
    platform_32k_rc_auto_tune();
	#endif
    comm_frame_init();
    comm_frame_create(&uart_comm, NULL, app_rx_frame);
    uart_driver_init(COMM_PORT, &uart_comm, (f_uart_rx_byte)comm_frame_rx_byte);
    dt698_645_frame_init();

    LOS_SwtmrCreate (LOS_MS2Tick(5000), LOS_SWTMR_MODE_ONCE, conn_timer_callback, &create_conn_timer, 1);
    LOS_SwtmrCreate (LOS_MS2Tick(10000), LOS_SWTMR_MODE_ONCE, reset_timer_callback, &reset_timer, 1);
    LOS_SwtmrCreate (LOS_MS2Tick(1000), LOS_SWTMR_MODE_ONCE, adv_report_callback, &adv_report_timer, 1);
    LOS_SwtmrCreate (LOS_MS2Tick(1000), LOS_SWTMR_MODE_ONCE, ble_off_timer_callback, &ble_off_timer, 1);
    LOS_SwtmrStart(reset_timer);
#if (RELEASE_VERSION != RTK_VEISHENG_VERSION)   
    master_comm_timeout_timer = xTimerCreate("comm_timeout",
                            pdMS_TO_TICKS(MASTER_COMM_TIMEOUT_TIMER_PERIOD * 1000),
                            pdTRUE,
                            NULL,
                            master_comm_timeout_timer_callback);
#endif

    install_on_demand(config_uart);
    //platform_config(PLATFORM_CFG_LOG_HCI, PLATFORM_CFG_ENABLE);
    att_server_init(att_read_callback, att_write_callback);
    hci_event_callback_registration.callback = user_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);
    att_server_register_packet_handler(user_packet_handler);
    gatt_client_register_handler(user_packet_handler);

   // ota_init_service();
    ota_init_handles(HANDLE_FOTA_VERSION, HANDLE_FOTA_CONTROL, HANDLE_FOTA_DATA);

    sm_add_event_handler(&sm_event_callback_registration);

    for (i = 0; i < sizeof(sm_persistent.er); i++)
    {
        int t = platform_rand();
        sm_persistent.er[i] = t & 0xff;
        sm_persistent.ir[i] = t >> 8;
    }
#if 0// (RELEASE_VERSION == RTK_VEISHENG_VERSION)
    uint32_t flag = FLASH_PARAM_FLAG;
    extern persistent_settings_t persistentSettings_flash;

    if(memcmp((void *)&persistentSettings_flash.flag,&flag,4)==0)
        memcpy(&sm_persistent,&persistentSettings_flash.sm_persistent, sizeof(sm_persistent));
#endif
    sm_config(1,
              IO_CAPABILITY_KEYBOARD_DISPLAY,
              0,
              &sm_persistent);
#ifdef LOG_FLASH_ENABLE
    log_flash_write(LOG_FLASH_TLV_INFO, 3, (uint8_t *)"STR");
#endif
    platform_set_irq_callback(COMM_ISR_ID, uart_driver_isr, NULL);
	
	const platform_ver_t *version = platform_get_version();
	log_printf("pltform version:%d.%d.%d\n\n", version->major, version->minor, version->patch);
    return 0;
}

prog_ver_t app_ver =
{
    .major = 2,
    .minor = 2,
    .patch = 4
};
