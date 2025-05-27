#include "zt_comm.h"

#include <stdlib.h>
#include <string.h>
#include "platform_api.h"
#include "btstack_util.h"
#include "cross_param.h"
#include "peripheral_uart.h"
#include "profile.h"
#include "main.h"

const int8_t tx_power_level_mapping[] = {4, 0, -4, -8, -20};

extern bd_addr_t temp_taiti_mac;
extern int read_db_flash(void *data, int size);
extern int write_db_flash(void *data, int size);
extern int is_invalid_mac(const uint8_t *mac);
extern void local_adv_ctrl(uint8_t enable);
extern uint8_t is_scanning; 		
extern uint8_t is_ble_off;
extern uint8_t ota_enable;
void switch_to_pulse_app(void);

int is_cfg_allowed(void);

#if(RELEASE_VERSION == RTK_VEISHENG_VERSION)
const uint32_t fw_version = 0x02020004;
const char module_ver[6] = {0x02,0x00,0x00,0x02,0x00,0x04};
const char module_sn[20] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
#else
const uint32_t fw_version = 0x01040006;
const char module_ver[6] = {0x41,0x42,0x43,0x44,0x45,0x46};
const char module_sn[20] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
#endif

static const char build_time[24] = __DATE__ " " __TIME__;

ble_data_mode_t data_mode = DATA_MODE_CACHED;

void reset_targets(dev_settings_t *targets, int num)
{
    int i;
    for (i = 0; i < num; i++)
    {
        memset(targets[i].addr, 0xff, BD_ADDR_LEN);
    }
}

settings_t g_settings = {
    .filter_onoff = 0x06,//default enable
    .tx_power = 01,
    .adv_interval = 0x0040*0.625,
    .scan_interval = 0x00A0*0.625,
    .local =
    {
        #if(RELEASE_VERSION == RTK_VEISHENG_VERSION)
        .addr = {0xc1, 0x00, 0x00, 0x00, 0x00, 0x7b, }        
        #else
        .addr = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, }
        #endif
    },

    .filter_type_cnt = 1,
    .filter_types = {0xC4}
//    .master_conn_param =
//    {
//        .interval = 410,
//        .latency = 0,
//        .sup_timer = 6000
//    },
};

#define FLASH_PARAM_FLAG    0x55AA55AA

const persistent_settings_t persistent_settings_default = {
    .uart_param = {
        .baud = 115200,
        .data_bits = 8,
        .parity_bits = 0, // 0:none, 1:odd, 2:even. ref: cmd_parity_to_enum 
        .stop_bits = 0 // 0: one stop bit, 1:two stop bits.
    }
};

persistent_settings_t g_persistent_settings;
// persistent_settings_t persistentSettings_flash;

static dev_settings_t g_dev_settings[SLAVE_MAX_NUM];

extern uint8_t g_adv_data[31];
extern int g_adv_data_len;
extern uint8_t g_scan_data[31];
extern int g_scan_data_len;

conn_dev_info_t g_conn_dev_info;

const uint8_t *get_connected_masters_mac(const settings_t *settings, int *master_num);
extern const uint32_t fw_version;


#if(USB_BCC_CHECK)
static uint8_t Cal_BCC(uint8_t *data, uint32_t len){
    uint8_t BCC = data[0];
    for(uint32_t i=1; i<len; i++){
        BCC ^= data[i];
    }
    return BCC;
}
#endif

void install_factory_data(void)
{
    memset(g_dev_settings, 0, sizeof(dev_settings_t) * SLAVE_MAX_NUM);//~{Ge3}4SIh18PEO"~}
    reset_targets(g_dev_settings, SLAVE_MAX_NUM);//~{0Q4SIh185XV7IhVCN*H+2?~}FF

#if (RELEASE_VERSION == RTK_VEISHENG_VERSION) 
    uint32_t flag = FLASH_PARAM_FLAG;
    read_db_flash(&g_persistent_settings, sizeof(persistent_settings_t));//~{4S~}flash~{;qH!~}uart~{2NJ}~}
    #if(USB_BCC_CHECK)
    if( memcmp((void *)&g_persistent_settings.flag, &flag, 4) == 0 && 
        Cal_BCC((uint8_t *)&g_persistent_settings, sizeof(persistent_settings_t)) == 0x00 )
    #else
    if( memcmp((void *)&g_persistent_settings.flag, &flag, 4) == 0 )
    #endif        
    {
        // set flash value.
        //memcpy(&g_persistent_settings, &persistentSettings_flash, sizeof(persistent_settings_t));
        PRINT("use flash value\r\n");

        memcpy(&g_settings.local.addr[0], &g_persistent_settings.settings.local.addr[0], 6);
        memcpy(g_adv_data, &g_persistent_settings.adv_data, sizeof(t_frame_adv_data));
        memcpy(g_dev_settings, &g_persistent_settings.dev_set, sizeof(dev_settings_t));
        PRINT("sm==:%x\r\n",g_persistent_settings.dev_set.smp_level);
        g_dev_settings->addr_changed = 1;

    }
    else
    {
        // set default value.
        memcpy(&g_persistent_settings, &persistent_settings_default, sizeof(persistent_settings_t));
        PRINT("use default value\r\n");
    }
#else
    read_db_flash(&g_persistent_settings, sizeof(persistent_settings_t));
    PRINT("use default value");
#endif

    PRINT("FW:%08x\r\n",fw_version);
}

static void load_from_global_param()
{
    int i;
    global_param_t *p = GLOBAL_PARAM;
    memcpy(g_settings.local.addr, p->local, BD_ADDR_LEN);

    for (i = 0; i < SLAVE_MAX_NUM; i++)
    {
        memcpy(g_dev_settings[i].addr, p->targets[i], BD_ADDR_LEN);
    }
}

static void save_to_global_param(const test_mode_t *test)
{
    extern UART_ePARITY cmd_parity_to_enum(uint8_t parity_bits);

    int i;
    global_param_t *p = GLOBAL_PARAM;
    const uint8_t *mac = get_connected_masters_mac(get_settings(), &i);
    memcpy(p->remote, temp_taiti_mac, BD_ADDR_LEN);
#if (RELEASE_VERSION == RTK_VEISHENG_VERSION)
    reverse_bd_addr(test->comm_addr,p->comm_addr);
#else
    memcpy(p->comm_addr, test->comm_addr, BD_ADDR_LEN);
#endif
    memcpy(p->local, g_settings.local.addr, BD_ADDR_LEN);

    for (i = 0; i < SLAVE_MAX_NUM; i++)
    {
        memcpy(p->targets[i], g_dev_settings[i].addr, BD_ADDR_LEN);
    }

    const uart_param_t *uart_param = &get_persistent_settings()->uart_param;
    p->uart_baud = uart_param->baud;
    p->uart_parity = cmd_parity_to_enum(uart_param->parity_bits);
    p->uart_two_stop_bits = uart_param->stop_bits ? 1 : 0;

    if (NULL == test) return;

#define COPY_F(a)   p->a = test->a

    p->meter_no = 1;
    COPY_F(pulse_type);
    COPY_F(pow_level);
#if (RELEASE_VERSION != RTK_VEISHENG_VERSION)
    COPY_F(chan_no);
#else
    p->chan_no = 5;
#endif

    p->interval = test->interval * 1000;
    //memcpy(p->comm_addr, test->comm_addr, BD_ADDR_LEN);

    memcpy(p->chan_freqs, test->chan_freqs, sizeof(test->chan_freqs));
}

void targets_updated(void);
void local_mac_updated(const settings_t *settings);

void install_on_demand(void (*extra_init)(void))
{
    void init_for_pulse_app(int init);

    install_factory_data();
    extra_init();//����UART
    uint8_t status = platform_read_persistent_reg();
    dbg_printf("====status===%d\r\n",status);
    init_for_pulse_app(platform_read_persistent_reg() == 0);

    if (platform_read_persistent_reg() == 0)//��ʱ���渴λ״̬
    {
        extern void report_reset_status(void);
        printf("report_reset_status\r\n");
        report_reset_status();
    }
    else
    {//ʹ���ϴα���Ĳ������ظ���˹�Զ����Ӻ͹㲥
        settings_t *p = get_settings();
        uint8_t dar = 0;
        GLOBAL_PARAM->uart_append_tx_data = NULL;
        platform_write_persistent_reg(0);

        send_response_frame(CTRL_CODE_RSP_SET_PARAM, CMD_PD, 1, &dar);

        load_from_global_param();
#if(RELEASE_VERSION != RTK_VEISHENG_VERSION)
        local_mac_updated(p);
#endif
        targets_updated();
    }
}

dev_settings_t *get_targets(void)
{
    return g_dev_settings;
}

settings_t *get_settings()
{
    return &g_settings;
}

const persistent_settings_t *get_persistent_settings(void)
{
    return &g_persistent_settings;
}

uint8_t *get_adv_data(int *len)
{
    if (len)
        *len = g_adv_data_len;
    return g_adv_data;
}

extern void driver_flush_tx(void);
uint8_t bonding_ctrl(const settings_t *settings, uint8_t cmd, const uint8_t *addr);
void targets_updated(void);
void peripheral_role_update(settings_t *settings);
const uint8_t *get_connected_slaves_mac(const settings_t *settings, int *slave_num);
void local_mac_updated(const settings_t *settings);
int get_connected_all_mac(const settings_t *settings, conn_dev_info_t *info);
void adv_data_updated(const settings_t *settings);
void adv_info_report_control(uint8_t status, uint8_t interval);
void local_pair_mode_updated(void);


static uint8_t check_sum(const uint8_t *buf, uint16_t len)
{
    uint16_t i = 0;
    uint8_t sum = 0;
    for(; i < len; i++)
        sum += buf[i];
    return sum;
}

static const uint8_t *load_all_dev_mac(const uint8_t *param, settings_t *setting)
{
    uint8_t i;
    uint16_t pos = 0;
    dev_settings_t *targets = (dev_settings_t *)get_targets();
    t_frame_adv_data *p_adv = (t_frame_adv_data *)g_adv_data;

    if (memcmp(setting->local.addr, param, DEV_ADDR_LEN) == 0) {
        setting->local.addr_changed = 0;
    } else {
        memcpy(setting->local.addr, param, DEV_ADDR_LEN);
        setting->local.addr_changed = 1;
    }
    pos += 6;

    for (i = 0; i < SLAVE_MAX_NUM; i++) {
        if (memcmp(targets[i].addr, param + (i + 1) * DEV_ADDR_LEN, DEV_ADDR_LEN) == 0) {
            targets[i].addr_changed = 0;
        } else {
            memcpy(targets[i].addr, param + (i + 1) * DEV_ADDR_LEN, DEV_ADDR_LEN);
            targets[i].addr_changed = 1;
        }
    }
    pos = (i + 1) * DEV_ADDR_LEN;
    memcpy(p_adv->adv_manu_data_v_dev_id, param + pos, 2);
    pos += 2;

    p_adv->adv_manu_data_v_dev_type = param[pos];
    pos += 1;

    memcpy(p_adv->adv_manu_data_v_pair_check, param + pos, 2);
//    uint8_t check_code[2] = {0xff,0xff};
//	
//    if(memcmp(p_adv->adv_manu_data_v_pair_check, check_code, 2) == 0)
//    {
//        setting->local.smp_level = 0;//boson
//    }
//    else
//    {
//        setting->local.smp_level = 0;//SMP_MIMT_KEY;//boson
//    }

	
	
    PRINT("smp_level=%d",setting->local.smp_level);
        
    pos += 2;

    memcpy(p_adv->adv_manu_data_v_con_PIN,param + pos, 16);
    pos += 16;

	setting->local.pair_len = 6;
    memcpy(setting->local.paring_code, param + pos, 6);
	uint8_t invalid_pin[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
	PRINT("pin: ");
	for (uint8_t j = 0; j < 6; j++)
		PRINT("pin:%c\r\n",*(param+pos+j));
	PRINT("\r\n");
	if (memcmp(invalid_pin, param+pos, 6) == 0)
		setting->local.smp_level = SMP_JUST_WORKS;
	else
		setting->local.smp_level = SMP_MIMT_KEY;
    pos += 6;

    for(i = 0; i < SLAVE_MAX_NUM; i++)
    {
        memcpy(targets[i].paring_code,param + pos, 6);		
		if(memcmp(targets[i].paring_code, invalid_pin, 6) == 0) {
			targets[i].smp_level = SMP_JUST_WORKS;
			targets[i].pair_len = 6;
		}
		else {
			targets[i].smp_level = SMP_MIMT_KEY;			
			targets[i].pair_len = 6;
		}
        pos += 6;
    }

    //TO DO CHECKSUM    
#if(RELEASE_VERSION == RTK_VEISHENG_VERSION)
    persistent_settings_t *pers_setting = &g_persistent_settings;
    uint32_t flag = FLASH_PARAM_FLAG;
    memcpy((void *)&(pers_setting->flag), &flag, 4);
    memcpy((void *)&(pers_setting->adv_data), p_adv, sizeof(t_frame_adv_data));
    memcpy((void *)&pers_setting->dev_set, targets, sizeof(dev_settings_t));
    memcpy((void *)&pers_setting->settings, setting, sizeof(settings_t));
    #if(USB_BCC_CHECK)    
    pers_setting->bcc = Cal_BCC((uint8_t *)&g_persistent_settings, sizeof(persistent_settings_t)-1);
    #endif
    write_db_flash(&g_persistent_settings, sizeof(g_persistent_settings));
    PRINT("save flash\r\n");
#endif

    return 0;

}

//f20b0008
static const uint8_t *load_local_dev_info(const uint8_t *param, settings_t *setting)
{
    uint16_t pos = 0;
    dev_settings_t *targets = (dev_settings_t *)get_targets();
    t_frame_adv_data *p_adv = (t_frame_adv_data *)g_adv_data;

    if (memcmp(setting->local.addr, param, DEV_ADDR_LEN) == 0) {
        PRINT("same mac");
        setting->local.addr_changed = 0;
    } else {
        PRINT("diff mac");
        memcpy(setting->local.addr, param, DEV_ADDR_LEN);
        setting->local.addr_changed = 1;
    }
    pos += 6;
    PRINT("smp_level=%d\r\n",param[pos]);
    if(param[pos] == 0)
       setting->local.smp_level = SMP_JUST_WORKS; 
    else
        setting->local.smp_level = SMP_MIMT_KEY;
    
    setting->local.pair_len = param[pos];
    pos += 1;

    memcpy(setting->local.paring_code, param+pos, setting->local.pair_len);
    pos += setting->local.pair_len;

    pos += 1; // pin len
    //����PIN�����ڹ㲥���Զ��յ��㲥���ݺ��PIN���ܳ���
    //��ͨ��1��2һ�������ݣ�����һ�鼴��
    memcpy(p_adv->adv_manu_data_v_con_PIN,param + pos, 16);
    return 0;

}


static const uint8_t *load_pair_mode(int active, const uint8_t *param, dev_settings_t *dev)
{
    if (active == 0) return param;
    const uint8_t *p = param;
    dev->smp_level = p[0];
    dev->pair_len = p[1];
    memcpy(dev->paring_code, p + 2, dev->pair_len);
    return p + 2 + dev->pair_len;
}

const uint8_t *load_pair_param(int active, const uint8_t *param, dev_settings_t *dev)
{
    if (active == 0) return param;
    const uint8_t *p = param;
    dev->pair_len = p[0];
    memcpy(dev->paring_code, p + 1, dev->pair_len);
    return p + 1 + dev->pair_len;
}

uint8_t *write_pair_mode(uint8_t *param, const dev_settings_t *dev)
{
    param[0] = dev->smp_level;
    param[1] = dev->pair_len;
    memcpy(param + 2, dev->paring_code, dev->pair_len);
    return param + 2 + dev->pair_len;
}

uint8_t *write_pair_param(uint8_t *param, const dev_settings_t *dev)
{
    param[0] = dev->pair_len;
    memcpy(param + 1, dev->paring_code, dev->pair_len);
    return param + 1 + dev->pair_len;
}

#define CHECK_CMD_PARAM_ERROR (-1)
#define CHECK_CMD_PARAM_OK (0)

static int check_CMD_ALL_DEV_PAIR_MODE_param(const int len, const uint8_t *param)
{
    int param_len;
    int i;
    int pair_num;
    uint8_t info_mask = param[0];
    t_pair_info *p_info = NULL;

    pair_num = 0;
    for (i = 0; i <= SLAVE_MAX_NUM; i++) {
        if ((info_mask & (1 << i)) == 0) {
            continue;
        }
        pair_num++;
    }

    param_len = 1;
    for (i = 0; i < pair_num; i++) {
        p_info = (t_pair_info *)(param + param_len);
        if ((p_info->dev_pair_mode != STA_BIT_LINK) &&
            (p_info->dev_pair_mode != STA_BIT_JUSTWORKS) &&
            (p_info->dev_pair_mode != STA_BIT_PASSKEY) &&
            (p_info->dev_pair_mode != STA_BIT_OOB)) {
            return CHECK_CMD_PARAM_ERROR;
        }

        if (p_info->dev_pair_mode == STA_BIT_JUSTWORKS) {
            if (p_info->pair_len != 0) {
                return CHECK_CMD_PARAM_ERROR;
            }
        }

        if (p_info->dev_pair_mode == STA_BIT_PASSKEY) {
            int pair_index;
            if (p_info->pair_len != 6) {
                return CHECK_CMD_PARAM_ERROR;
            }
            for (pair_index = 0; pair_index < p_info->pair_len; pair_index++) {
                if ((p_info->pair_data[pair_index] < '0') ||
                    (p_info->pair_data[pair_index] > '9')) {
                    return CHECK_CMD_PARAM_ERROR;
                }
            }
        }

        param_len = param_len + p_info->pair_len + 2;
    }

    if (param_len != len) {
        return CHECK_CMD_PARAM_ERROR;
    }

    return CHECK_CMD_PARAM_OK;
}

static int check_CMD_ALL_DEV_MAC_ADDR_param(const int len, const uint8_t *param)
{
    if (len != (SLAVE_MAX_NUM + 1) * DEV_ADDR_LEN) {
        return CHECK_CMD_PARAM_ERROR;
    }

    return CHECK_CMD_PARAM_OK;
}

static int check_CMD_BLE_PARAMS_param(const int len, const uint8_t *param)
{
    // FE FE FE FE 68 05 00 02 02 00 0B F2 FF FF FF FF FF FF 00 00 00 00 68 01 00 40 00 A0 B1 16
    uint8_t tx_power;
    uint16_t adv_interval;
    uint16_t scan_interval;

    if (len != 5) {
        return CHECK_CMD_PARAM_ERROR;
    }

    tx_power = param[0];
    if (tx_power >= sizeof(tx_power_level_mapping)) {
        return CHECK_CMD_PARAM_ERROR;
    }
    
#if (RELEASE_VERSION == RTK_VEISHENG_VERSION)
    adv_interval  = (param[2] << 8) | param[1];
#else
    adv_interval  = (param[1] << 8) | param[2];
#endif
    if ((adv_interval < 0x28) ||
        (adv_interval > 0x03e8)) {
        return CHECK_CMD_PARAM_ERROR;
    }

#if (RELEASE_VERSION == RTK_VEISHENG_VERSION)
    scan_interval = (param[4] << 8) | param[3];
#else
    scan_interval = (param[3] << 8) | param[4];
#endif
    if ((scan_interval < 0x4) ||
        (scan_interval > 0x4000)) {
        return CHECK_CMD_PARAM_ERROR;
    }

    return CHECK_CMD_PARAM_OK;
}

static int check_CMD_BLE_DISCONN_MASTER_param(const int len, const uint8_t *param)
{
    //FE FE FE FE 68 0D 00 02 04 00 0B F2 FF FF FF FF FF FF 00 00 00 00 68 02 C0 00 00 00 00 01 C0 00 00 00 00 02 5F 16
    uint8_t dev_num;

    dev_num = param[0];

    if ((dev_num != 1) && (dev_num != 2)) {
        return CHECK_CMD_PARAM_ERROR;
    }

    if (len != dev_num * 6 + 1) {
        return CHECK_CMD_PARAM_ERROR;
    }

    return CHECK_CMD_PARAM_OK;
}

int check_CMD_PD_param(const int len, const uint8_t *param)
{
    const test_mode_t *p_pd_param = (test_mode_t *)param;

    if ((p_pd_param->pulse_type > 0x06) &&
        (p_pd_param->pulse_type != 0xFF)) {
        return CHECK_CMD_PARAM_ERROR;
    }

    if ((p_pd_param->pow_level >= sizeof(tx_power_level_mapping))) {
        return CHECK_CMD_PARAM_ERROR;
    }
	
#if (RELEASE_VERSION != RTK_VEISHENG_VERSION)
    if ((p_pd_param->chan_no < 1) &&
        (p_pd_param->chan_no > 5)) {
        return CHECK_CMD_PARAM_ERROR;
    }

    if (len != p_pd_param->chan_no * 2 + sizeof(test_mode_t) - 10) {
        return CHECK_CMD_PARAM_ERROR;
    }
#endif

    return CHECK_CMD_PARAM_OK;
}


int check_CMD_SCAN_FILTER_param(const int len, const uint8_t *param)
{
    PRINT("SCAN_FILTER%daaaaaaaaaaaaaaa\r\n",len);
    if(len != 25)       
        return CHECK_CMD_PARAM_ERROR;
    else
        return CHECK_CMD_PARAM_OK;
}

static int check_CMD_BLE_TRANS_MODE_param(const int len, const uint8_t *param)
{
    uint8_t mode;

    if (len != 1) {
        return CHECK_CMD_PARAM_ERROR;
    }

    mode = param[0];
    if (mode > 0x02) {
        return CHECK_CMD_PARAM_ERROR;
    }

    return CHECK_CMD_PARAM_OK;
}

static int check_CMD_MODULE_INFO_param(const int len, const uint8_t *param)
{
    uint8_t info_type;

    if (len != 1) {
        return CHECK_CMD_PARAM_ERROR;
    }

    info_type = param[0];
    if (info_type > 0x04) {
        return CHECK_CMD_PARAM_ERROR;
    }

    return CHECK_CMD_PARAM_OK;
}

static int check_CMD_SCAN_param(const int len, const uint8_t *param)
{
    uint8_t scan_status;

    if (len != 1) {
        return CHECK_CMD_PARAM_ERROR;
    }

    scan_status = param[0];
    if (scan_status > 0x01) {
        return CHECK_CMD_PARAM_ERROR;
    }

    return CHECK_CMD_PARAM_OK;
}

static int check_CMD_FILTER_ADDR_param(const int len, const uint8_t *param)
{
    int param_len;
    uint8_t dev_num;

    dev_num = param[0];
    if (dev_num > 0x14) {
        return CHECK_CMD_PARAM_ERROR;
    }

    param_len = (dev_num * 6) + 1;
    if (len != param_len) {
        return CHECK_CMD_PARAM_ERROR;
    }

    return CHECK_CMD_PARAM_OK;
}

static int check_CMD_FILTER_TYPE_param(const int len, const uint8_t *param)
{
    int param_len;
    uint8_t signature_num;

    signature_num = param[0];
    if (signature_num > 0x14) {
        return CHECK_CMD_PARAM_ERROR;
    }

    param_len = signature_num + 1;
    if (len != param_len) {
        return CHECK_CMD_PARAM_ERROR;
    }

    return CHECK_CMD_PARAM_OK;
}

static uint8_t adv_check_msk[] = {
    0x02, 0x01, 0x06,
#if (AVOID_TENG_HE_BUG == 1)
    0x16, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0x4, 0, 0, 0, 0
#else
    0x16, 0xFF, 0xC1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0x4, 0x09, 0, 0, 0
#endif
};
static int check_CMD_ADV_DATA_param(const int len, const uint8_t *param)
{
    int i;

    if (len > sizeof(adv_check_msk)) {
        //log_printf("adv data len err\n");
        return CHECK_CMD_PARAM_ERROR;
    }

    for (i = 0; i < len; i++) {
        if (adv_check_msk[i] == 0) {
            continue;
        } else if (adv_check_msk[i] == param[i]) {
            continue;
        } else {
            //log_printf("adv data %d err\n", i);
            return CHECK_CMD_PARAM_ERROR;
        }
    }

    return CHECK_CMD_PARAM_OK;
}

static uint8_t resp_check_msk[] = {
    17, 0x07, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    9, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0
};
static int check_CMD_RESP_DATA_param(const int len, const uint8_t *param)
{
    int i;

    if (len > sizeof(resp_check_msk)) {
        return CHECK_CMD_PARAM_ERROR;
    }

    for (i = 0; i < len; i++) {
        if (resp_check_msk[i] == 0) {
            continue;
        } else if (resp_check_msk[i] == param[i]) {
            continue;
        } else {
            return CHECK_CMD_PARAM_ERROR;
        }
    }

    return CHECK_CMD_PARAM_OK;
}

static int check_CMD_ADV_INFO_REPORT_param(const int len, const uint8_t *param)
{
    uint8_t report_status;

    if (len != 2) {
        return CHECK_CMD_PARAM_ERROR;
    }

    report_status = param[0];
    if (report_status > 0x1) {
        return CHECK_CMD_PARAM_ERROR;
    }

    return CHECK_CMD_PARAM_OK;
}

static int check_CMD_ADV_CONFIG_INFO_param(const int len, const uint8_t *param)
{
    uint8_t info_mask;
    int param_len;

    info_mask = param[0];
    if ((info_mask < 0x01) ||
        (info_mask > 0x0f)) {
        return CHECK_CMD_PARAM_ERROR;
    }

    param_len = 1;
    if ((info_mask & 0x01) != 0) {
        param_len += 2;
    }
    if ((info_mask & 0x02) != 0) {
        param_len += 16;
    }
    if ((info_mask & 0x04) != 0) {
        param_len += 3;
    }
    if ((info_mask & 0x08) != 0) {
        param_len += 2;
    }

    if (param_len != len) {
        return CHECK_CMD_PARAM_ERROR;
    }

    return CHECK_CMD_PARAM_OK;
}

//static int check_CMD_LINK_CMD_param(const int len, const uint8_t *param)
//{
//    uint8_t info_mask;
//    uint8_t opcode;
//    int dev_num;
//    int param_len;
//    int i;

//    info_mask = param[0];
//    if (info_mask > 0x1F) {
//        return CHECK_CMD_PARAM_ERROR;
//    }

//    dev_num = 0;
//    for (i = 0; i < MASTER_MAX_NUM + SLAVE_MAX_NUM; i++) {
//        if ((info_mask & (1 << i)) == 0) {
//            continue;
//        }
//        dev_num++;
//    }

//    param_len = dev_num * sizeof(t_dev_addr) + 1;
//    if (param_len != len) {
//        return CHECK_CMD_PARAM_ERROR;
//    }

//    for (i = 0; i < dev_num; i++) {
//        opcode = param[(i * sizeof(t_dev_addr)) + 1];
//        if (opcode > 0x01) {
//            return CHECK_CMD_PARAM_ERROR;
//        }
//    }

//    return CHECK_CMD_PARAM_OK;
//}

//static int check_CMD_ADV_param(const int len, const uint8_t *param)
//{
//    uint8_t opcode;

//    if (len != 1) {
//        return CHECK_CMD_PARAM_ERROR;
//    }

//    opcode = param[0];
//    if (opcode > 0x01) {
//        return CHECK_CMD_PARAM_ERROR;
//    }

//    return CHECK_CMD_PARAM_OK;
//}

static int check_CMD_BLE_AUTH_ON_OFF_param(const int len, const uint8_t *param)
{
    if (len != 1) {
        return CHECK_CMD_PARAM_ERROR;
    }

    return CHECK_CMD_PARAM_OK;
}

static uint16_t master_comm_time_out = 0;

static int check_CMD_MASTER_COMM_TIMEOUT_param(const int len, const uint8_t *param)
{
    // FE FE FE FE 68 02 00 02 1F 00 00 00 FF FF FF FF FF FF 00 00 00 00 68 2C 01 1A 16
    if (len != 2) {
        return CHECK_CMD_PARAM_ERROR;
    }

    return CHECK_CMD_PARAM_OK;
}


static int check_CMD_ALL_WORK_PARAM_param(const int len, const uint8_t *param)
{
    uint8_t dev_type;
	uint16_t factory_id,feature_id;
    int i = 0, offset = 0;
    int pair_num;
    uint8_t info_mask = param[0];
    t_pair_info *p_info = NULL;
	
	log_printf("len=%d\r\n ",len);
	if (len != 70)
		return CHECK_CMD_PARAM_ERROR;
	offset = 4*BD_ADDR_LEN;
	factory_id = *(uint16_t *)&param[offset];
	
	log_printf("offset = %d,factory_id=%04x\r\n ",offset,factory_id);
	offset += 2;
	dev_type =  param[offset];
	offset += 1;
	feature_id = *(uint16_t *)&param[offset];
	
	log_printf("feature_id=%04x\r\n",feature_id);
//	if (feature_id > 1)
//		return CHECK_CMD_PARAM_ERROR;

	uint8_t cs = check_sum(param, len - 1);
	log_printf("cs=%02x\r",cs);
//	if (cs != param[69])	
//		return CHECK_CMD_PARAM_ERROR;
	
    return CHECK_CMD_PARAM_OK;
}


static int check_param_ok(uint32_t id, const int len, const uint8_t *param)
{
    int ret = CHECK_CMD_PARAM_OK;

    if (len == 0) {
        return CHECK_CMD_PARAM_ERROR;
    }

    switch (id)
    {
#if (RELEASE_VERSION != RTK_VEISHENG_VERSION)   
    case CMD_ALL_DEV_PAIR_MODE:
        ret = check_CMD_ALL_DEV_PAIR_MODE_param(len, param);
        break;
#endif
#if (RELEASE_VERSION == RTK_VEISHENG_VERSION)   
    case CMD_SCAN_FILTER:
        ret = check_CMD_SCAN_FILTER_param(len, param);
        break;
    case CMD_ALL_WORK_PARAM:
	    ret = check_CMD_ALL_WORK_PARAM_param(len, param);
    	break;
#endif

    case CMD_ALL_DEV_MAC_ADDR:
        ret = check_CMD_ALL_DEV_MAC_ADDR_param(len, param);
        break;

    case CMD_BLE_PARAMS:
        ret = check_CMD_BLE_PARAMS_param(len, param);
        break;
#if (RELEASE_VERSION != RTK_VEISHENG_VERSION)	   
    case CMD_BLE_DISCONN_MASTER:
        ret = check_CMD_BLE_DISCONN_MASTER_param(len, param);
        break;
#endif
    case CMD_PD:
        ret = check_CMD_PD_param(len, param);
        break;
    case CMD_BLE_TRANS_MODE:
        ret = check_CMD_BLE_TRANS_MODE_param(len, param);
        break;
    case CMD_MODULE_INFO:
        ret = check_CMD_MODULE_INFO_param(len, param);
        break;
    case CMD_SCAN:
        ret = check_CMD_SCAN_param(len, param);
        break;
    case CMD_FILTER_ADDR:
        ret = check_CMD_FILTER_ADDR_param(len, param);
        break;
    case CMD_FILTER_TYPE:
        ret = check_CMD_FILTER_TYPE_param(len, param);
        break;
    case CMD_ADV_DATA:
        ret = check_CMD_ADV_DATA_param(len, param);
        break;
    case CMD_RESP_DATA:
        ret = check_CMD_RESP_DATA_param(len, param);
        break;
    case CMD_ADV_INFO_REPORT:
        ret = check_CMD_ADV_INFO_REPORT_param(len, param);
        break;
    case CMD_ADV_CONFIG_INFO:
        ret = check_CMD_ADV_CONFIG_INFO_param(len, param);
        break;
    //case CMD_LINK_CMD:
    //    ret = check_CMD_LINK_CMD_param(len, param);
    //    break;
//    case CMD_ADV:
//        ret = check_CMD_ADV_param(len, param);
//        break;
    case CMD_BLE_AUTH_ON_OFF:
        ret = check_CMD_BLE_AUTH_ON_OFF_param(len, param);
        break;
    
#if (RELEASE_VERSION != RTK_VEISHENG_VERSION)
    case CMD_MASTER_COMM_TIMEOUT:
        ret = check_CMD_MASTER_COMM_TIMEOUT_param(len, param);
        break;
#endif
    default:
        break;
    }
    return ret;
}

static void handle_set_param(uint32_t id, const int len, const uint8_t *param)
{

	#if (SUPPORT_UART_OTA == 1)
	uint8_t uart_ota = 0;
	#endif

    uint8_t dar = COMM_ERR_DATAGRAM_PARAM;
    settings_t *p = get_settings();
    int switch_to_pulse = 0;

    if (is_ble_off && (id != CMD_BLE_AUTH_ON_OFF))
    {
        send_response_frame(CTRL_CODE_RSP_SET_PARAM, id, 1, &dar);
        return;
    }

    //log_printf("set len %d\n", len);

    if (check_param_ok(id, len, param) != CHECK_CMD_PARAM_OK) {
        send_response_frame(CTRL_CODE_RSP_SET_PARAM, id, 1, &dar);
        log_error("param err 0x%x\n", id);
        return;
    }

    switch (id)
    {
    #if (RELEASE_VERSION == RTK_VEISHENG_VERSION)
    case CMD_ALL_WORK_PARAM:
    {
        const uint8_t *t = param;
        load_all_dev_mac(t,p);
		local_pair_mode_updated();

		if (p->local.addr_changed ==1)  {
			PRINT("local mac changed\r\n");
			local_adv_ctrl(0);	
			is_scanning = 0;
			gap_set_ext_scan_enable(0, 0, 0, 0);
			gap_disconnect_all();
    	}
		
        local_mac_updated(p);		
        targets_updated();

        dar = COMM_ERR_SUCC;
        p->local.addr_changed = 0;

    }
        break;

    case CMD_MASTER_INFO:
    {
        const uint8_t *t = param;

        load_local_dev_info(t,p);

        local_mac_updated(p);
        targets_updated();
		
        dar = COMM_ERR_SUCC;
        p->local.addr_changed = 0;
    }

        break;

    case CMD_SLAVE_INFO:	
    {
        int i = 0;
        uint16_t pos = 0;
        dev_settings_t *targets = (dev_settings_t *)get_targets();
        for (i = 0; i < SLAVE_MAX_NUM; i++) {
            if (memcmp(targets[i].addr, param + (i + 1) * DEV_ADDR_LEN, DEV_ADDR_LEN) == 0) {
                targets[i].addr_changed = 0;
            } else {
                memcpy(targets[i].addr, param + (i + 1) * DEV_ADDR_LEN, DEV_ADDR_LEN);
                targets[i].addr_changed = 1;
            }
        }
        pos += SLAVE_MAX_NUM * DEV_ADDR_LEN;

        for(i = 0; i < SLAVE_MAX_NUM; i++){
            targets[i].pair_len = param[pos];
            pos += 1;
        }

        for(i = 0; i < SLAVE_MAX_NUM; i++){
            memcpy(targets[i].paring_code, param+pos, targets[i].pair_len);
            pos += targets[i].pair_len;
        }


        targets_updated();
        dar = COMM_ERR_SUCC;			
    }
        break;

    case CMD_SCAN_FILTER:
    {
        uint8_t pos = 0;
        uint16_t i;
        p->filter_onoff = param[pos++];
        p->filter_mac_cnt = param[pos++];
        for( i = 0; i < p->filter_mac_cnt; i++){
            memcpy(&(p->filter_macs[i]), param + pos, DEV_ADDR_LEN);
            pos += DEV_ADDR_LEN;
        }

        p->filter_type_cnt = param[pos++];
        for( i = 0; i < p->filter_type_cnt; i++){
            memcpy(&(p->filter_types[i]), param + pos, 1);
            pos += 1;
        }
       
        dar = COMM_ERR_SUCC;
    }

        break;

    case CMD_OTA_PERMIT:
        ota_enable = 1;
        //ota flag
        break;
#endif
#if (RELEASE_VERSION != RTK_VEISHENG_VERSION)
    case CMD_ALL_DEV_PAIR_MODE:
    {
        uint8_t info_mask = param[0];
        const uint8_t *t = param + 1;
        int i;
        t = load_pair_mode(info_mask & 1, t, &p->local);
        local_pair_mode_updated();
        dev_settings_t *targets = (dev_settings_t *)get_targets();
        for (i = 1; i <= SLAVE_MAX_NUM; i++)
            t = load_pair_mode(info_mask & (1 << i), t, targets + i - 1);
        dar = COMM_ERR_SUCC;
    }
    break;


    case CMD_ALL_DEV_MAC_ADDR:
        if (len == (SLAVE_MAX_NUM + 1) * DEV_ADDR_LEN) {
            int i;
            dev_settings_t *targets = (dev_settings_t *)get_targets();

            if (memcmp(p->local.addr, param, DEV_ADDR_LEN) == 0) {
                p->local.addr_changed = 0;
            } else {
                memcpy(p->local.addr, param, DEV_ADDR_LEN);
                p->local.addr_changed = 1;
            }

            for (i = 0; i < SLAVE_MAX_NUM; i++) {
                if (memcmp(targets[i].addr, param + (i + 1) * DEV_ADDR_LEN, DEV_ADDR_LEN) == 0) {
                    targets[i].addr_changed = 0;
                } else {
                    memcpy(targets[i].addr, param + (i + 1) * DEV_ADDR_LEN, DEV_ADDR_LEN);
                    targets[i].addr_changed = 1;
                }
            }

            local_mac_updated(p);
            targets_updated();
            dar = COMM_ERR_SUCC;
            p->local.addr_changed = 0;
        }
        break;
#endif
    case CMD_BLE_PARAMS:
        if (len == 5){
            p->tx_power = param[0];
        #if(RELEASE_VERSION == RTK_VEISHENG_VERSION)
            p->adv_interval  = (param[2] << 8) | param[1];
            p->scan_interval = (param[4] << 8) | param[3];
        #else
            p->adv_interval  = (param[1] << 8) | param[2];
            p->scan_interval = (param[3] << 8) | param[4];
        #endif
		    local_mac_updated(p);
            targets_updated();
            dar = COMM_ERR_SUCC;
        }
        break;

#if (RELEASE_VERSION != RTK_VEISHENG_VERSION)   
    case CMD_BLE_DISCONN_MASTER: {
        uint8_t dev_num = param[0];
        bd_addr_t peer_addr;
        int i;
        int ret = 0;

        for (i = 0; i < dev_num; i++) {
            reverse_bd_addr(&param[i * 6 + 1], peer_addr);
            ret |= i_am_slave_disconnect_master(peer_addr);
        }
        if (ret != 0) {
            dar = COMM_ERR_BAD_TARGET_ADDR;
        } else {
            dar = COMM_ERR_SUCC;
        }
    }
    break;
#endif
    case CMD_MODULE_INFO:
        if (len == 1) {
            switch (param[0])
            {
            case 2:
                dar = COMM_ERR_SUCC;
                install_factory_data();
            case 1:
                dar = COMM_ERR_SUCC;
                send_response_frame(CTRL_CODE_RSP_SET_PARAM, CMD_MODULE_INFO, 1, &dar);
                driver_flush_tx();
                platform_reset();
                break;
            case 3:
                dar = COMM_ERR_SUCC;
                send_response_frame(CTRL_CODE_RSP_SET_PARAM, CMD_MODULE_INFO, 1, &dar);
                driver_flush_tx();
                platform_shutdown(0xffffffff, NULL, 0);
                break;
            case 4:
                dar = COMM_ERR_SUCC;
                send_response_frame(CTRL_CODE_RSP_SET_PARAM, CMD_MODULE_INFO, 1, &dar);
                driver_flush_tx();
                platform_switch_app(SEC_FOTA_APP);
                break;
            default:
                break;
            }
        }
        break;
#if (RELEASE_VERSION != RTK_VEISHENG_VERSION)   
    case CMD_SCAN:
        if (len == 1)
        {
            dar = scan_control(p, param[0]);
        }
        break;
    case CMD_FILTER_ADDR:
        if (len)
        {
            p->filter_mac_cnt = param[0];
            if (p->filter_mac_cnt && (p->filter_mac_cnt <= SCAN_FILTER_MAX_NUM))
            {
                int i;
                for (i = 0; i < p->filter_mac_cnt; i++) {
                    reverse_bd_addr(param + 1 + i * DEV_ADDR_LEN, p->filter_macs[i]);
                } // filter_macs A B C D E F
            }
            dar = COMM_ERR_SUCC;
        }
        break;

    case CMD_FILTER_TYPE:
        if (len)
        {
            p->filter_type_cnt = param[0];
            if (p->filter_type_cnt && (p->filter_type_cnt <= SCAN_FILTER_MAX_NUM))
            {
                memcpy(p->filter_types, param + 1, p->filter_type_cnt);
            }
            dar = COMM_ERR_SUCC;
        }
        break;


//    case CMD_UART_PARAMS:
//        if (len == sizeof(uart_param_t))
//        {
//            g_persistent_settings.uart_param = *(const uart_param_t *)param;
//            write_db_flash(&g_persistent_settings, sizeof(g_persistent_settings));
//            dar = COMM_ERR_SUCC;
//        }
//        break;

    case CMD_ADV_DATA:
        if (len <= 31)
        {
            //g_adv_data_len = len;
            memcpy(g_adv_data, param, len);
            adv_data_updated(p);
            dar = COMM_ERR_SUCC;
        }
        break;

    case CMD_RESP_DATA:
        if (len <= 31)
        {
            g_scan_data_len = len;
            memcpy(g_scan_data, param, len);
            adv_data_updated(p);
            dar = COMM_ERR_SUCC;
        }
        break;

    case CMD_ADV_INFO_REPORT:
        if (2 == len)
        {
            adv_info_report_control(param[0], param[1]);
            dar = COMM_ERR_SUCC;
        }
        break;

//    case CMD_CLEAR_BOND_INFO:
//        if (BD_ADDR_LEN == len)
//            dar = bonding_ctrl(p, 0, param + 1);
//        break;

//    case CMD_CLEAR_ALL_BOND_INFO:
//        if (0 == len)
//        {
//            dar = COMM_ERR_SUCC;
//            send_response_frame(CTRL_CODE_RSP_SET_PARAM, CMD_MODULE_INFO, 1, &dar);
//            driver_flush_tx();
//            platform_reset();
//        }
//        break;
#endif
    case CMD_PD:
        {
            const test_mode_t *test = (const test_mode_t *)param;
            log_printf("pulse_type: %d\n", test->pulse_type);
            dar = COMM_ERR_SUCC;
            if (test->pulse_type != 0xff)
            {
				uint8_t conn_info = 0;
                send_response_frame(CTRL_CODE_RSP_SET_PARAM, CMD_PD, 1, &dar);
                driver_flush_tx();
				int cnt = get_connected_all_mac(p, &g_conn_dev_info);
				g_conn_dev_info.active |= 0x01;
				memcpy(g_conn_dev_info.addr[0],  temp_taiti_mac,  BD_ADDR_LEN);
				send_response_id_frame(CTRL_CODE_PROACTIVE_REPORT, CMD_BLE_CONN_INFO, 1 + cnt * 6, &g_conn_dev_info);
                driver_flush_tx();
				
                save_to_global_param(test);
                platform_write_persistent_reg(1);
                switch_to_pulse = 1;
            }
        }
        break;

    case CMD_BLE_TRANS_MODE:
        if (len == 1)
        {
            data_mode = (ble_data_mode_t)param[0];
            dar = COMM_ERR_SUCC;
        }
        break;

    case CMD_ADV_CONFIG_INFO:
        {
            t_frame_adv_data *p_adv = (t_frame_adv_data *)g_adv_data;
            uint8_t info_mask = param[0];
            const uint8_t *p_data = &param[1];

            if ((info_mask & 0x01) != 0) {
                memcpy(p_adv->adv_manu_data_v_pair_check, p_data, 2);
                p_data += 2;
            }
            if ((info_mask & 0x02) != 0) {
                memcpy(p_adv->adv_manu_data_v_con_PIN, p_data, 16);
                p_data += 16;
            }
            if ((info_mask & 0x04) != 0) {
                memcpy(p_adv->adv_dev_name_v, p_data, 3);
                p_data += 3;
            }
            if ((info_mask & 0x08) != 0) {
                memcpy(p_adv->adv_manu_data_v_dev_id, p_data, 2);
                p_data += 2;
            }
            dar = COMM_ERR_SUCC;
        }
        break;
//    case CMD_TX_POWER:
//        if (len == 1)
//        {
//            p->tx_power = param[0];
//            dar = COMM_ERR_SUCC;
//        }
//        break;
//    case CMD_ADV_INTERVAL:
//        if (len == 2)
//        {
//            p->adv_interval  = param[1] | (param[0] << 8);
//            dar = COMM_ERR_SUCC;
//        }
//        break;
//    case CMD_DEV_ADDR:
//        {
//            uint8_t info_mask = param[0];
//            const uint8_t *t = param + 1;
//            int i;

//            if (info_mask & 1)
//            {
//                reverse_bd_addr(t, p->local.addr);
//                t += DEV_ADDR_LEN;
//                local_mac_updated(p);
//            }

//            if (info_mask & 0xE)
//            {
//                dev_settings_t *targets = (dev_settings_t *)get_targets();
//                for (i = 1; i <= SLAVE_MAX_NUM; i++)
//                {
//                    if (info_mask & (1 << i))
//                    {
//                        reverse_bd_addr(param + (i - 1) * DEV_ADDR_LEN, targets[i].addr);
//                        t += DEV_ADDR_LEN;
//                    }
//                }

//                targets_updated();
//            }

//            dar = COMM_ERR_SUCC;
//        }
//        break;
//    case CMD_DEV_PAIR_MODE:
//        {
//            uint8_t info_mask = param[0];
//            const uint8_t *t = param + 1;
//            int i;
//            if (info_mask & 1)
//            {
//                p->local.smp_level = *t;
//                t++;
//                local_pair_mode_updated();
//            }

//            dev_settings_t *targets = (dev_settings_t *)get_targets();
//            for (i = 1; i <= SLAVE_MAX_NUM; i++)
//            {
//                targets[i].smp_level = *t;
//                t++;
//            }
//            dar = COMM_ERR_SUCC;
//        }
//        break;
//    case CMD_DEV_PAIR_PARAM:
//        {
//            uint8_t info_mask = param[0];
//            const uint8_t *t = param + 1;
//            int i;

//            if (info_mask & 1)
//            {
//                t = load_pair_param(info_mask & 1, t, &p->local);
//                local_pair_mode_updated();
//            }

//            dev_settings_t *targets = (dev_settings_t *)get_targets();
//            for (i = 1; i <= SLAVE_MAX_NUM; i++)
//                t = load_pair_param(info_mask & (i << 1), t, targets + i - 1);

//            dar = COMM_ERR_SUCC;
//        }
//        break;

//    case CMD_LINK_CMD:
//        {
//            uint8_t info_mask = param[0];
//            const uint8_t *t = NULL;
//            int i;
//            int ret;

//            extern int disconnect_device(const settings_t *settings, const uint8_t *addr);
//            extern int connect_slave_device(int slave_no, const settings_t *settings, const uint8_t *addr);
//            bd_addr_t addr;

//            t = param + 1;
//            ret = 0;
//            for (i = 0; i < SLAVE_MAX_NUM + MASTER_MAX_NUM; i++)
//            {
//                if ((info_mask & (1 << i)) == 0) {
//                    continue;
//                }

//                memcpy(addr, t + 1, sizeof(addr));
//                if (t[0] == 0) {
//                    disconnect_device(p, addr);
//                } else if (t[0] == 1) {
//                    if (i < MASTER_MAX_NUM) {
//                    } else {
//                        ret |= connect_slave_device(i - MASTER_MAX_NUM + 1, p, addr);
//                    }
//                } else {
//                }
//                t += 1 + DEV_ADDR_LEN;

//            }
//            if (ret == 0) {
//                dar = COMM_ERR_SUCC;
//            } else {
//                dar = COMM_ERR_BAD_TARGET_ADDR;
//            }
//        }
//        break;
//    case CMD_ADV:
//        if (len == 1)
//        {
//            extern void local_adv_ctrl(uint8_t enable);
//            local_adv_ctrl(param[0]);
//            dar = COMM_ERR_SUCC;
//        }
//        break;




    case CMD_BLE_AUTH_ON_OFF:
        if (len == 1)
        {
            extern void ble_off_ctrl(uint8_t ctrl);
            ble_off_ctrl(param[0]);
            dar = COMM_ERR_SUCC;
        }
        break;
 #if (RELEASE_VERSION != RTK_VEISHENG_VERSION)   
    case CMD_MASTER_COMM_TIMEOUT: {
        master_comm_time_out = param[0] | param[1] << 8;
        if ((master_comm_time_out == 0x0000) || (master_comm_time_out == 0xFFFF)) {
            master_comm_time_out_stop();
        } else {
            master_comm_time_out_start(master_comm_time_out);
        }
        dar = COMM_ERR_SUCC;
    }
    break;
#endif

#if (SUPPORT_UART_OTA == 1)
		
		case CMD_UART_OTA:
			{
				uart_ota = 1;
				extern int uart_ota_write_and_resp(const uint8_t *data, int len);
				uart_ota_write_and_resp(param,len);
			}
		break;			
#endif	

    default:
        dar = COMM_ERR_UNDEF_CMD;
        break;
    }

    if (switch_to_pulse) {
        volatile int i;
        //send_response_frame(CTRL_CODE_RSP_SET_PARAM, id, 1, &dar);
        //driver_flush_tx();
        for (i = 0; i < 48000; i++) {
        };
        switch_to_pulse_app();
    } else {
		#if (SUPPORT_UART_OTA == 1)
		if (uart_ota != 1)
		#endif
        send_response_frame(CTRL_CODE_RSP_SET_PARAM, id, 1, &dar);
    }
}

static uint8_t adv_config_info_data[] = {
    0x0F,
    0x12, 0x34,
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
    0x44, 0x44, 0x44,
    0xEE, 0xFF
};

static void handle_get_param(uint32_t id, const int len, const uint8_t *param)
{
    const settings_t *p = get_settings();	
	t_frame_adv_data *p_adv = (t_frame_adv_data *)g_adv_data;
    static uint8_t buffer[200];
    switch (id)
    {
 #if (RELEASE_VERSION == RTK_VEISHENG_VERSION)   
    case CMD_ALL_WORK_PARAM:
    {
        int i;
        uint8_t *t = buffer;
        PRINT("t:%p,buffer:%p \r\n",t,buffer);
        const dev_settings_t *targets = get_targets();
        memcpy(t, p->local.addr,DEV_ADDR_LEN);
        t += DEV_ADDR_LEN;
        for(i = 0; i < SLAVE_MAX_NUM; i++){
            memcpy(t, targets[i].addr,DEV_ADDR_LEN);
            t += DEV_ADDR_LEN;
        }

        memcpy(t, p_adv->adv_manu_data_v_dev_id, 2);
        t += 2;

        memcpy(t, &p_adv->adv_manu_data_v_dev_type, 1);
        t += 1;

        memcpy(t, p_adv->adv_manu_data_v_pair_check, 2);
        t += 2;

        memcpy(t, p_adv->adv_manu_data_v_con_PIN, 16);
        t += 16;

        memcpy(t, p->local.paring_code, 6);
        t += 6;

        for(i = 0; i < SLAVE_MAX_NUM; i++){
            memcpy(t, targets[i].paring_code, 6);
            t += 6;
        }
        uint8_t cs = check_sum(buffer, t-buffer);
        memcpy(t, &cs, 1);
        t += 1;

        send_response_id_frame(CTRL_CODE_RSP_GET_PARAM, CMD_ALL_WORK_PARAM, t - buffer, buffer);


    }
    break;

    case CMD_MASTER_INFO:
    {
        uint8_t *t = buffer;
        //PRINT("t:%p,buffer:%p \r\n",t,buffer);
        const dev_settings_t *targets = get_targets();
        memcpy(t, p->local.addr,DEV_ADDR_LEN);
        t += DEV_ADDR_LEN;
        
        *(t++) = 6;
        memcpy(t, p->local.paring_code, 6);
        t += 6;     

        *(t++) = 16;
        memcpy(t, p_adv->adv_manu_data_v_con_PIN, 16);
        t += 16;
        
        *(t++) = 6;
        memcpy(t, p->local.paring_code, 6);
        t += 6;     

        *(t++) = 16;
        memcpy(t, p_adv->adv_manu_data_v_con_PIN, 16);
        t += 16;

        send_response_id_frame(CTRL_CODE_RSP_GET_PARAM, CMD_MASTER_INFO, t - buffer, buffer);
    }
        break;
        
    case CMD_SLAVE_INFO://F20B0009
    {
        int i;
        uint16_t pos = 0;
        const dev_settings_t *targets = get_targets();
        for(i = 0; i < SLAVE_MAX_NUM; i++){
            memcpy(buffer+pos, targets[i].addr, DEV_ADDR_LEN);
            pos += DEV_ADDR_LEN;
        }

        for(i = 0; i < SLAVE_MAX_NUM; i++){
            buffer[pos++] = targets[i].pair_len;
        }

        for(i = 0; i < SLAVE_MAX_NUM; i++){
            memcpy(buffer+pos, targets[i].paring_code, targets[i].pair_len);
            pos += targets[i].pair_len;
        }

        send_response_id_frame(CTRL_CODE_RSP_GET_PARAM, CMD_SLAVE_INFO, pos, buffer);


    }
    break;

    case CMD_MODULE_SERIAL_NUMBER:
    {
        send_response_id_frame(CTRL_CODE_RSP_GET_PARAM, CMD_MODULE_SERIAL_NUMBER, sizeof(module_sn), module_sn);
    }
    break;
#endif
#if (RELEASE_VERSION != RTK_VEISHENG_VERSION)
    case CMD_ALL_DEV_PAIR_MODE:
        {
            int i;
            uint8_t *t = buffer + 1;
            const dev_settings_t *targets = get_targets();
            buffer[0] = 0xf;
            t = write_pair_mode(t, &p->local);
            for (i = 0; i < SLAVE_MAX_NUM; i++)
                t = write_pair_mode(t, targets + i);
            send_response_id_frame(CTRL_CODE_RSP_GET_PARAM, CMD_ALL_DEV_PAIR_MODE, t - buffer, buffer);
        }
        break;
#endif	
#if (RELEASE_VERSION == RTK_VEISHENG_VERSION)
    case CMD_SCAN_FILTER:
    {
        uint8_t pos = 0;
        uint16_t i;
        memcpy(buffer, &p->filter_onoff, 1);
        pos++;

        memcpy(buffer+pos, &p->filter_mac_cnt, 1);
        pos++;
        PRINT("filter_mac_cnt:%d,filter_type_cnt:%d\r\n",p->filter_mac_cnt,p->filter_type_cnt);
        for(i = 0; i < p->filter_mac_cnt; i++){
            memcpy(buffer +  pos, &p->filter_macs[i], DEV_ADDR_LEN);
            pos += DEV_ADDR_LEN;
        }

        memcpy(buffer + pos, &p->filter_type_cnt, 1);
        pos++;

        for(i = 0; i < p->filter_type_cnt; i++){
            memcpy(buffer + pos, &p->filter_types[i], 1);
            pos += 1;
        }

        memset(buffer+pos, 0x00, 2);
        pos += 2;
        PRINT("get scan filter len=%d\r\n",pos);
        send_response_id_frame(CTRL_CODE_RSP_GET_PARAM, CMD_SCAN_FILTER, pos, buffer);

    }
        break;
    #endif
    case CMD_ALL_DEV_MAC_ADDR:
        {
            int i;
            const dev_settings_t *targets = get_targets();
            memcpy(buffer, p->local.addr, DEV_ADDR_LEN);
            for (i = 0; i < SLAVE_MAX_NUM; i++)
                memcpy(buffer + (i + 1) * DEV_ADDR_LEN, targets[i].addr, DEV_ADDR_LEN);
            send_response_id_frame(CTRL_CODE_RSP_GET_PARAM, CMD_ALL_DEV_MAC_ADDR, (SLAVE_MAX_NUM + 1) * DEV_ADDR_LEN, buffer);
        }
        break;

    case CMD_BLE_PARAMS:
        {
               buffer[0] = p->tx_power;         
#if (RELEASE_VERSION == RTK_VEISHENG_VERSION)           
			buffer[2] = p->adv_interval >> 8;
            buffer[1] = p->adv_interval & 0xff;
            buffer[4] = p->scan_interval >> 8;
            buffer[3] = p->scan_interval & 0xff;
#else
            buffer[1] = p->adv_interval >> 8;
            buffer[2] = p->adv_interval & 0xff;
            buffer[3] = p->scan_interval >> 8;
            buffer[4] = p->scan_interval & 0xff;
#endif
            send_response_id_frame(CTRL_CODE_RSP_GET_PARAM, CMD_BLE_PARAMS, 5, buffer);
        }
        break;

    case CMD_BLE_CONN_INFO:
        {
            int cnt = get_connected_all_mac(p, &g_conn_dev_info);
            send_response_id_frame(CTRL_CODE_RSP_GET_PARAM, CMD_BLE_CONN_INFO, 1 + cnt * 6, &g_conn_dev_info);
        }
        break;

    case CMD_FIRMWARE_VERSION:
        send_response_id_frame(CTRL_CODE_RSP_GET_PARAM, CMD_FIRMWARE_VERSION, sizeof(fw_version), &fw_version);
        break;

    case CMD_GET_FILTER_INFO:
        {
            extern int adv_report_size;
            extern uint8_t adv_report_buffer[1];
            send_response_id_frame(CTRL_CODE_RSP_GET_PARAM, CMD_GET_FILTER_INFO, adv_report_size, adv_report_buffer);
        }
        break;

//    case CMD_UART_PARAMS:
//        send_response_id_frame(CTRL_CODE_RSP_GET_PARAM, CMD_UART_PARAMS, sizeof(uart_param_t),
//                               &get_persistent_settings()->uart_param);
//        break;

    case CMD_ADV_DATA:
        send_response_id_frame(CTRL_CODE_RSP_GET_PARAM, CMD_ADV_DATA, g_adv_data_len,
                                g_adv_data);
        break;
    case CMD_RESP_DATA:
        send_response_id_frame(CTRL_CODE_RSP_GET_PARAM, CMD_RESP_DATA, g_scan_data_len,
                                g_scan_data);
        break;

//    case CMD_GET_LICENSE:
//        send_response_id_frame(CTRL_CODE_RSP_GET_PARAM, CMD_GET_LICENSE, sizeof(g_persistent_settings.license),
//                                g_persistent_settings.license);
//        break;

    case CMD_BLE_TRANS_MODE:
        send_response_id_frame(CTRL_CODE_RSP_GET_PARAM, CMD_BLE_TRANS_MODE, sizeof(data_mode),
                                &data_mode);
        break;
    case CMD_ADV_CONFIG_INFO:
        {
            int data_len;
            uint8_t info_mask = param[0];
            t_frame_adv_data *p_adv = (t_frame_adv_data *)g_adv_data;

            data_len = 0;
            adv_config_info_data[data_len] = info_mask;
            data_len = 1;

            if ((info_mask & 0x01) != 0) {
                memcpy(adv_config_info_data + data_len, p_adv->adv_manu_data_v_pair_check, 2);
                data_len += 2;
            }
            if ((info_mask & 0x02) != 0) {
                memcpy(adv_config_info_data + data_len, p_adv->adv_manu_data_v_con_PIN, 16);
                data_len += 16;
            }
            if ((info_mask & 0x04) != 0) {
                memcpy(adv_config_info_data + data_len, p_adv->adv_dev_name_v, 3);
                data_len += 3;
            }
            if ((info_mask & 0x08) != 0) {
                memcpy(adv_config_info_data + data_len, p_adv->adv_manu_data_v_dev_id, 2);
                data_len += 2;
            }
            send_response_id_frame(CTRL_CODE_RSP_GET_PARAM, CMD_ADV_CONFIG_INFO, data_len,
                                adv_config_info_data);
        }
        break;
//    case CMD_TX_POWER:
//        send_response_id_frame(CTRL_CODE_RSP_GET_PARAM, CMD_TX_POWER, sizeof(p->tx_power),
//                                &p->tx_power);
//    case CMD_ADV_INTERVAL:
//        {
//            uint8_t big[2];
//            big[0] = p->adv_interval >> 8;
//            big[1] = p->adv_interval & 0xff;
//            send_response_id_frame(CTRL_CODE_RSP_GET_PARAM, CMD_ADV_INTERVAL, sizeof(big),
//                                big);
//        }
//        break;
//    case CMD_DEV_ADDR:
//        {
//            int i;
//            uint8_t *t = buffer + 1;
//            const dev_settings_t *targets = get_targets();
//            buffer[0] = 0x1;
//            reverse_bd_addr(p->local.addr, t); t += BD_ADDR_LEN;
//            for (i = 0; i < SLAVE_MAX_NUM; i++)
//            {
//                if (is_invalid_mac(targets[i].addr))
//                {
//                    buffer[0] |= 1 << (i + 1);
//                    reverse_bd_addr(targets[i].addr, t); t += BD_ADDR_LEN;
//                }
//            }
//            send_response_id_frame(CTRL_CODE_RSP_GET_PARAM, CMD_DEV_ADDR, t - buffer, buffer);
//        }
//        break;
//    case CMD_DEV_PAIR_MODE:
//        {
//            int i;
//            uint8_t *t = buffer + 1;
//            const dev_settings_t *targets = get_targets();
//            buffer[0] = 0x1;
//            t[0] = p->local.smp_level; t++;
//            for (i = 0; i < SLAVE_MAX_NUM; i++)
//            {
//                if (is_invalid_mac(targets[i].addr))
//                {
//                    buffer[0] |= 1 << (i + 1);
//                    t[0] = targets[i].smp_level; t++;
//                }
//            }
//            send_response_id_frame(CTRL_CODE_RSP_GET_PARAM, CMD_DEV_PAIR_MODE, t - buffer, buffer);
//        }
//        break;

//    case CMD_DEV_PAIR_PARAM:
//        {
//            int i;
//            uint8_t *t = buffer + 1;
//            const dev_settings_t *targets = get_targets();
//            buffer[0] = 0x1;
//            t = write_pair_param(t, &p->local);
//            for (i = 0; i < SLAVE_MAX_NUM; i++)
//            {
//                if (is_invalid_mac(targets[i].addr))
//                {
//                    buffer[0] |= 1 << (i + 1);
//                    t = write_pair_param(t, targets + i);
//                }
//            }
//            send_response_id_frame(CTRL_CODE_RSP_GET_PARAM, CMD_DEV_PAIR_PARAM, t - buffer, buffer);
//        }
//        break;
    case CMD_FIRMWARE_BUILD_TIME:
        send_response_id_frame(CTRL_CODE_RSP_GET_PARAM, CMD_FIRMWARE_BUILD_TIME, sizeof(build_time), build_time);
        break;
    case CMD_LINK_RSSI:
        {
            extern void ble_collect_rssi(void);
            ble_collect_rssi();
        }
        break;
    case CMD_MODULE_SN:
        send_response_id_frame(CTRL_CODE_RSP_GET_PARAM, CMD_MODULE_SN, sizeof(module_sn), module_sn);
        break;
#if (RELEASE_VERSION != RTK_VEISHENG_VERSION)
    case CMD_MASTER_COMM_TIMEOUT: {
        uint8_t time_out[2];
        time_out[0] = master_comm_time_out & 0xFF;
        time_out[1] = (master_comm_time_out >> 8) & 0xFF;
        send_response_id_frame(CTRL_CODE_RSP_GET_PARAM, CMD_MASTER_COMM_TIMEOUT, sizeof(time_out), time_out);
    }
    break;
#endif
    default:
        {
            buffer[0] = COMM_ERR_UNDEF_CMD;
            send_response_frame(CTRL_CODE_RSP_GET_PARAM, id, 1, buffer);
        }
        break;
    }
}

static void handle_proactive_response(uint32_t id, const uint8_t *param)
{
    extern void reset_responded(void);
    t_frame_adv_data *p_adv = (t_frame_adv_data *)g_adv_data;
    uint8_t dev_id_offset = 2;
    settings_t *settings = get_settings();
    PRINT("handle_proactive_response");
    switch (id)
    {
    case CMD_MODULE_INFO:
#if(RELEASE_VERSION == RTK_VEISHENG_VERSION)
        memcpy(p_adv->adv_manu_data_v_dev_id, param + 2, 2);
        peripheral_role_update(settings);
#endif
        reset_responded();
        break;
    }
}

void send_to_dev(const uint8_t *addr, const uint8_t *data, int len);

void handle_frame(simple_frame_t *frame)
{
    switch (frame->header.ctrl_code & 0x7)
    {
    case CTRL_CODE_METER2MODULE:{
		memcpy(temp_taiti_mac, frame->header.m, BD_ADDR_LEN);			
        send_to_dev(frame->header.m, frame->data, frame->header.len);
    	}
        break;

    case CTRL_CODE_SET_PARAM:
        handle_set_param(frame->header.cmd_code, frame->header.len, frame->data);
        break;

    case CTRL_CODE_GET_PARAM:
        handle_get_param(frame->header.cmd_code, frame->header.len, frame->data);
        break;

    case CTRL_CODE_PROACTIVE_REPORT:
        handle_proactive_response(frame->header.cmd_code, frame->data);
        break;

    default:
        platform_reset();
        break;
    }
}
