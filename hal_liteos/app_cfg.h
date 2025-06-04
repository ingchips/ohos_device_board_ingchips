#ifndef app_cfg_h
#define app_cfg_h

#define DEBUG_DISCONNECT	1

#define SUPPORT_UART_OTA	1

#warning "define release com version"
#define RTK_VERSION             10
#define RTK_XUJI_VERSION        11
#define RTK_ZHONG_CHEN_VERSION  12
#define RTK_VEISHENG_VERSION    13 //BST91880C //SW9188

#define RELEASE_VERSION         RTK_VEISHENG_VERSION
//#define RELEASE_VERSION         RTK_XUJI_VERSION

#define SLAVE_MAX_NUM       3

#define MASTER_MAX_NUM      2

#define SCAN_FILTER_MAX_NUM 20

#define FLASH_WRITE_NO_DELAY

#define CONN_MAX_NUM    (SLAVE_MAX_NUM + MASTER_MAX_NUM)

#define PRINT   platform_printf

#ifndef SEC_FOTA_APP
#define SEC_FOTA_APP    0x44000
#endif

#define CONN_STAT_OUT       18
#define MODE_CTRL           19

#if 1 //�ĳ����Ͱ��
#if(RELEASE_VERSION == RTK_VEISHENG_VERSION)
#define PIN_PRINT_TX        2
#define PIN_PRINT_RX        3

#define PIN_COMM_TX         7
#define PIN_COMM_RX         17
#else
#define PIN_PRINT_TX        2
#define PIN_PRINT_RX        3

#define PIN_COMM_TX         17
#define PIN_COMM_RX         9
#endif
//                            ������ �������� ʱ��Ͷ�� ��г�� ��г�� �޹� �й�
#if (RELEASE_VERSION == RTK_ZHONG_CHEN_VERSION)
#define TEST_PINS           {  8,     8,       8,       8,     8,     8,  10 }
#elif(RELEASE_VERSION == RTK_VEISHENG_VERSION)
#define TEST_PINS           { 0,    0,      0,      0,    0,    1,  9 }
#else
#define TEST_PINS           { 10,    10,      10,      10,    10,    10,  8 }
#endif

#else
#if(RELEASE_VERSION == RTK_VEISHENG_VERSION)
#define PIN_PRINT_TX        15
#define PIN_PRINT_RX        16

#define PIN_COMM_TX         8
#define PIN_COMM_RX         7
#else
#define PIN_PRINT_TX        2
#define PIN_PRINT_RX        3

#define PIN_COMM_TX         17
#define PIN_COMM_RX         9
#endif
//                            ������ �������� ʱ��Ͷ�� ��г�� ��г�� �޹� �й�
#if (RELEASE_VERSION == RTK_ZHONG_CHEN_VERSION)
#define TEST_PINS           {  8,     8,       8,       8,     8,     8,  10 }
#elif(RELEASE_VERSION == RTK_VEISHENG_VERSION)
#define TEST_PINS           { 2,    2,      2,      2,    2,    3,  1 }
#else
#define TEST_PINS           { 10,    10,      10,      10,    10,    10,  8 }
#endif

#endif

#define DT698_MAX_LEN            (1500)
#define DT645_MAX_LEN            (200)

#define DATA_MAX_LEN             (1500)


#define DT_698_645_FRAMES_CNT 5//8
#define FRAMES_CNT           5//16

#endif
