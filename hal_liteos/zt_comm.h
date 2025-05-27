#ifndef zt_comm_h
#define zt_comm_h

#include "bluetooth.h"

#include "app_cfg.h"

#include "zt_frame.h"
#include "sm.h"
#if (RELEASE_VERSION == RTK_XUJI_VERSION)
#define AVOID_TENG_HE_BUG       1 
#else
#define AVOID_TENG_HE_BUG       0
#endif

#define STA_BIT_LINK            0
#define STA_BIT_JUSTWORKS       1
#define STA_BIT_PASSKEY         2
#define STA_BIT_OOB             3

#define COMM_ERR_SUCC               (0)
#define COMM_ERR_UNDEF_CMD          (1)
#define COMM_ERR_BAD_TARGET_ADDR    (2)
#define COMM_ERR_ACCESS_DENIED      (3)
#define COMM_ERR_BLE_TRANS          (4)
#define COMM_ERR_TOO_MANY_BLE_CONNS (5)
#define COMM_ERR_DATAGRAM_PARAM     (6)
#define COMM_ERR_NO_BONDING_INFO    (16)
#define COMM_ERR_OUT_OF_MEM         (17)
#define COMM_ERR_OTHER              (255)

#define CTRL_CODE_METER2MODULE          0x01
#define CTRL_CODE_SET_PARAM             0x02
#define CTRL_CODE_GET_PARAM             0x03
#define CTRL_CODE_PROACTIVE_REPORT      0x04

#define CTRL_CODE_FLAG_RESPONSE         0x80
#define CTRL_CODE_MODULE2METER          0x81

#define CTRL_CODE_FLAG_STATUS           0x40

#define CTRL_CODE_RSP_GET_PARAM         (CTRL_CODE_FLAG_RESPONSE | CTRL_CODE_GET_PARAM)
#if(RELEASE_VERSION == RTK_VEISHENG_VERSION)
#define CTRL_CODE_RSP_SET_PARAM         (CTRL_CODE_FLAG_RESPONSE | CTRL_CODE_SET_PARAM)
#else
#define CTRL_CODE_RSP_SET_PARAM         (CTRL_CODE_FLAG_RESPONSE | CTRL_CODE_FLAG_STATUS | CTRL_CODE_SET_PARAM) 
#endif
#define SMP_NULL                            0
#define SMP_JUST_WORKS                      1
#define SMP_MIMT_KEY                        2
#define SMP_OOB                             3

enum
{
    PROTOCOL_645            = 1,
    PROTOCOL_698            = 2,
    PROTOCOL_MODBUS         = 3,
};

enum
{
    MODE_DUAL                   = 2,
    MODE_TEST_MASTER            = 0x99,
};

typedef enum ble_data_mode
{
    DATA_MODE_UNDEFINED         = 0,
    DATA_MODE_CACHED            = 1,
    DATA_MODE_DIRECT            = 2,
} ble_data_mode_t;

#pragma pack (push, 1)

typedef struct
{
    bd_addr_t addr;
    uint8_t smp_level;
    uint8_t pair_len;
    char paring_code[16];
    uint8_t addr_changed;
} dev_settings_t;

typedef struct
{
    uint16_t    interval;  // in ms
    uint8_t     latency;
    uint16_t    sup_timer;      // in ms
} conn_param_t;

typedef struct
{
    uint32_t baud;
    uint8_t data_bits;
    uint8_t parity_bits;
    uint8_t stop_bits;
} uart_param_t;

typedef struct
{
    uint8_t cmd;
    bd_addr_t addr;
} conn_ctrl_t;

typedef struct
{
#if (RELEASE_VERSION == RTK_XUJI_VERSION)
    uint8_t cmd[4];
#endif

    uint8_t pulse_type;
    uint8_t interval;
    uint8_t pow_level;

#if (RELEASE_VERSION == RTK_XUJI_VERSION)
    uint8_t comm_addr_len;
#endif

    uint8_t comm_addr[6];

#if (RELEASE_VERSION != RTK_VEISHENG_VERSION)
    uint8_t chan_no;
#endif

    uint16_t chan_freqs[5];
} test_mode_t;

typedef struct
{
    uint8_t active;
    bd_addr_t addr[SLAVE_MAX_NUM + MASTER_MAX_NUM];
} conn_dev_info_t;

typedef struct {
    uint8_t adv_data_ltv[3];

    uint8_t adv_manu_data_lt[2];
    uint8_t adv_manu_data_v_dev_type;
    uint8_t adv_manu_data_v_dev_id[2];
    uint8_t adv_manu_data_v_pair_check[2];
    uint8_t adv_manu_data_v_con_PIN[16];

    uint8_t adv_dev_name_lt[2];
    uint8_t adv_dev_name_v[3];
} t_frame_adv_data;

typedef struct {
    uint8_t dev_pair_mode;
    uint8_t pair_len;
    uint8_t pair_data[];
} t_pair_info;

typedef struct {
    uint8_t opcode;
    uint8_t addr[6];
} t_dev_addr;

#pragma pack (pop)

typedef struct
{
    uint8_t tx_power;
	uint8_t filter_onoff;
    uint8_t filter_mac_cnt;
    uint8_t filter_type_cnt;
    uint8_t filter_types[SCAN_FILTER_MAX_NUM];
    uint16_t adv_interval;
    uint16_t scan_interval;
    bd_addr_t filter_macs[SCAN_FILTER_MAX_NUM];
    dev_settings_t local;
    //conn_param_t   master_conn_param;
} settings_t;

#define USB_BCC_CHECK   (1)

typedef struct
{
    uint8_t        license[20];
    uart_param_t   uart_param;   
#if 1//(RELEASE_VERSION == RTK_VEISHENG_VERSION)
    t_frame_adv_data adv_data;
    dev_settings_t  dev_set;
    settings_t      settings;    
    sm_persistent_t sm_persistent;
    uint32_t        flag;
    #if(USB_BCC_CHECK)
	uint8_t resever_data[3];
    uint8_t         bcc;
    #endif
#endif
} persistent_settings_t;


void handle_frame(simple_frame_t *frame);

void install_factory_data(void);
void install_on_demand(void (*extra_init)(void));

settings_t *get_settings(void);
const persistent_settings_t *get_persistent_settings(void);
dev_settings_t *get_targets(void);
uint8_t *get_adv_data(int *len);
extern const int8_t tx_power_level_mapping[];
extern ble_data_mode_t data_mode;

#define ID_TRANSFORM(x) (x)
#if (RELEASE_VERSION != RTK_VEISHENG_VERSION)
#define    CMD_ALL_DEV_PAIR_MODE        ID_TRANSFORM(0xF20B0000)
#define    CMD_ALL_DEV_MAC_ADDR         ID_TRANSFORM(0xF20B0001)
#define    CMD_BLE_PARAMS               ID_TRANSFORM(0xF20B0002)
#define    CMD_BLE_CONN_INFO            ID_TRANSFORM(0xF20B0003)
#define    CMD_BLE_DISCONN_MASTER   ID_TRANSFORM(0xF20B0004)

#define    CMD_PD                   ID_TRANSFORM(0xF20B0201)

#define    CMD_BLE_TRANS_MODE       ID_TRANSFORM(0x00000006)
#define    CMD_MODULE_INFO          ID_TRANSFORM(0x00000007)
#define    CMD_FIRMWARE_VERSION     ID_TRANSFORM(0x00000008)
#define    CMD_SCAN                 ID_TRANSFORM(0x00000009)
#define    CMD_FILTER_ADDR          ID_TRANSFORM(0x0000000A)
#define    CMD_FILTER_TYPE          ID_TRANSFORM(0x0000000B)
#define    CMD_GET_FILTER_INFO      ID_TRANSFORM(0x0000000C)

//#define    CMD_UART_PARAMS          ID_TRANSFORM(0x0000000D)
#define    CMD_ADV_DATA             ID_TRANSFORM(0x0000000E)
#define    CMD_RESP_DATA            ID_TRANSFORM(0x0000000F)
#define    CMD_ADV_INFO_REPORT      ID_TRANSFORM(0x00000010)

//#define    CMD_CLEAR_BOND_INFO      ID_TRANSFORM(0x00000011)
//#define    CMD_CLEAR_ALL_BOND_INFO  ID_TRANSFORM(0x00000012)

//#define    CMD_ADV_CONFIG_INFO      ID_TRANSFORM(0x00000013)
#define    CMD_ADV_CONFIG_INFO      ID_TRANSFORM(0x00000013)
//#define    CMD_TX_POWER             ID_TRANSFORM(0x00000014)
//#define    CMD_ADV_INTERVAL         ID_TRANSFORM(0x00000015)
//#define    CMD_DEV_ADDR             ID_TRANSFORM(0x00000016)
//#define    CMD_DEV_PAIR_MODE        ID_TRANSFORM(0x00000017)
//#define    CMD_DEV_PAIR_PARAM       ID_TRANSFORM(0x00000018)
//#define    CMD_LINK_CMD             ID_TRANSFORM(0x00000019)
//#define    CMD_ADV                  ID_TRANSFORM(0x0000001A)
#define    CMD_FIRMWARE_BUILD_TIME  ID_TRANSFORM(0x0000001B)
#define    CMD_LINK_RSSI            ID_TRANSFORM(0x0000001C)
#define    CMD_MODULE_SN            ID_TRANSFORM(0x0000001D)
#define    CMD_BLE_AUTH_ON_OFF      ID_TRANSFORM(0x0000001E)
#define    CMD_MASTER_COMM_TIMEOUT  ID_TRANSFORM(0x0000001F)
//#define    CMD_GET_LICENSE          ID_TRANSFORM(0x00000020)
#else
//FOR WEISHENG
#define     CMD_ALL_WORK_PARAM          ID_TRANSFORM(0xF20B0000)
#define     CMD_SCAN_FILTER             ID_TRANSFORM(0xF20B0006)
#define     CMD_MASTER_INFO             ID_TRANSFORM(0xF20B0008)
#define     CMD_SLAVE_INFO              ID_TRANSFORM(0xF20B0009)
#define     CMD_MODULE_SERIAL_NUMBER    ID_TRANSFORM(0xF20B000A)

#define     CMD_PD                      ID_TRANSFORM(0xFFFF0005)
#define     CMD_OTA_PERMIT              ID_TRANSFORM(0xFFFFFEFE)

#define     CMD_ALL_DEV_MAC_ADDR        ID_TRANSFORM(0xF20B0001)
#define     CMD_BLE_PARAMS              ID_TRANSFORM(0xF20B0002)
#define     CMD_BLE_CONN_INFO           ID_TRANSFORM(0xF20B0003)


#define     CMD_BLE_TRANS_MODE          ID_TRANSFORM(0x00000006)
#define     CMD_MODULE_INFO             ID_TRANSFORM(0x00000007)
#define     CMD_FIRMWARE_VERSION        ID_TRANSFORM(0x00000008)
#define     CMD_SCAN                    ID_TRANSFORM(0x00000009)
#define     CMD_FILTER_ADDR             ID_TRANSFORM(0x0000000A)
#define     CMD_FILTER_TYPE             ID_TRANSFORM(0x0000000B)
#define     CMD_GET_FILTER_INFO         ID_TRANSFORM(0x0000000C)

#define     CMD_ADV_DATA                ID_TRANSFORM(0x0000000E)
#define     CMD_RESP_DATA               ID_TRANSFORM(0x0000000F)
#define     CMD_ADV_INFO_REPORT         ID_TRANSFORM(0x00000010)
#define     CMD_ADV_CONFIG_INFO         ID_TRANSFORM(0x00000013)
//#define     CMD_LINK_CMD                ID_TRANSFORM(0x00000019)
//#define     CMD_ADV                     ID_TRANSFORM(0x0000001A)
#define     CMD_FIRMWARE_BUILD_TIME     ID_TRANSFORM(0x0000001B)
#define     CMD_LINK_RSSI               ID_TRANSFORM(0x0000001C)
#define     CMD_MODULE_SN               ID_TRANSFORM(0x0000001D)
#define     CMD_BLE_AUTH_ON_OFF         ID_TRANSFORM(0x0000001E)
#if (SUPPORT_UART_OTA == 1)
#define    CMD_UART_OTA					ID_TRANSFORM(0x0000002E)
#endif
#endif
#endif
