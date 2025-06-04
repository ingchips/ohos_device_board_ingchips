#include <stdio.h>
#include <string.h>
#include "profile.h"
#include "ingsoc.h"
#include "platform_api.h"
#include "eflash.h"
#include "app_cfg.h"
#include "zt_comm.h"
#include "uart_driver.h"
#include "dt_698_645.h"
#ifdef LOG_FLASH_ENABLE
#include "log_flash.h"
#endif
#include "trace.h"
#include "main.h"
#include "rf_util.h"

void platform_wdg_reset(void)
{
    SYSCTRL_ClearClkGateMulti(1 << SYSCTRL_ClkGate_APB_TMR0);
    TMR0_UNLOCK();
    TMR_WatchDogEnable(0);
    TMR0_LOCK();
    return;
}


uint32_t cb_putc(char *c, void *dummy)
{
    while (apUART_Check_TXFIFO_FULL(PRINT_PORT) == 1);
    UART_SendData(PRINT_PORT, (uint8_t)*c);
    return 0;
}

int fputc(int ch, FILE *f)
{
    cb_putc((char *)&ch, NULL);
    return ch;
}

UART_ePARITY cmd_parity_to_enum(uint8_t parity_bits)
{
    switch (parity_bits)
    {
    case 2:
        return UART_PARITY_EVEN_PARITY;
    case 1:
        return UART_PARITY_ODD_PARITY;
    default:
        return UART_PARITY_NOT_CHECK;
    }
}

void config_print_uart(void)
{
    UART_sStateStruct config;

    PINCTRL_SetPadMux(PIN_PRINT_RX, IO_SOURCE_GENERAL);
    PINCTRL_SelUartRxdIn(UART_PORT_0, PIN_PRINT_RX);
    PINCTRL_SetPadMux(PIN_PRINT_TX, IO_SOURCE_UART0_TXD);

    config.word_length       = UART_WLEN_8_BITS;
    config.parity            = UART_PARITY_NOT_CHECK;
    config.fifo_enable       = 1;
    config.two_stop_bits     = 0;
    config.receive_en        = 1;
    config.transmit_en       = 1;
    config.UART_en           = 1;
    config.cts_en            = 0;
    config.rts_en            = 0;
    config.rxfifo_waterlevel = 1;
    config.txfifo_waterlevel = 1;
    config.ClockFrequency    = OSC_CLK_FREQ;
    config.BaudRate          = 115200;

    if(0 != apUART_Initialize(PRINT_PORT, &config, (1 << bsUART_RECEIVE_INTENAB))){
        platform_wdg_reset();
        for (;;);
    }
#ifdef LOG_FLASH_ENABLE
    log_uart_irq_init(); // If it is not initialized, the log system cannot be started using serial command configuration. 
#endif
}

void config_uart(void)
{
    UART_sStateStruct config;
    const uart_param_t *uart_param = &get_persistent_settings()->uart_param;

    PINCTRL_SetPadMux(PIN_COMM_RX, IO_SOURCE_GENERAL);
    PINCTRL_SelUartRxdIn(UART_PORT_1, PIN_COMM_RX);
    PINCTRL_SetPadMux(PIN_COMM_TX, IO_SOURCE_UART1_TXD);
    printf("baudrate:%d\r\n",uart_param->baud);
    config.word_length       = (UART_eWLEN)(UART_WLEN_5_BITS + uart_param->data_bits - 5);
    config.parity            = cmd_parity_to_enum(uart_param->parity_bits);
    config.fifo_enable       = 1;
    config.two_stop_bits     = uart_param->stop_bits ? 1 : 0;
    config.receive_en        = 1;
    config.transmit_en       = 1;
    config.UART_en           = 1;
    config.cts_en            = 0;
    config.rts_en            = 0;
    config.rxfifo_waterlevel = 1;
    config.txfifo_waterlevel = 1;
    config.ClockFrequency    = OSC_CLK_FREQ;
    config.BaudRate          = uart_param->baud;
    if(0 != apUART_Initialize(COMM_PORT, &config, (1 << bsUART_RECEIVE_INTENAB) | (1 << bsUART_TRANSMIT_INTENAB))){
        log_error("comm uart error!!!\n");
        platform_wdg_reset();
        for (;;);
    }
}

void pulse_status_updated(uint16_t status, uint64_t timer_cnt, void *user_data);

void connection_state(int flag)
{
    GIO_WriteValue((GIO_Index_t)CONN_STAT_OUT, flag ? 0 : 1);
}

int is_cfg_allowed(void)
{
    return 1; // GIO_ReadValue(MODE_CTRL) == 0;
}

void setup_peripherals(void)
{
    SYSCTRL_ClearClkGateMulti(  (1 << SYSCTRL_ClkGate_APB_UART0)
                              | (1 << SYSCTRL_ClkGate_APB_UART1)
                              | (1 << SYSCTRL_ClkGate_APB_PinCtrl)
                              | (1 << SYSCTRL_ClkGate_APB_GPIO)
    );

    config_print_uart();

    PINCTRL_SetPadMux(MODE_CTRL, IO_SOURCE_GENERAL);
    GIO_SetDirection((GIO_Index_t)MODE_CTRL, GIO_DIR_INPUT);
    PINCTRL_Pull(MODE_CTRL, PINCTRL_PULL_UP);

    PINCTRL_SetPadMux(CONN_STAT_OUT, IO_SOURCE_GENERAL);
    GIO_SetDirection((GIO_Index_t)CONN_STAT_OUT, GIO_DIR_OUTPUT);
    GIO_WriteValue((GIO_Index_t)CONN_STAT_OUT, 1);
}

uint32_t on_deep_sleep_wakeup(void *dummy, void *user_data)
{
    (void)(dummy);
    (void)(user_data);
    setup_peripherals();
    return 0;
}

uint32_t query_deep_sleep_allowed(void *dummy, void *user_data)
{
    (void)(dummy);
    (void)(user_data);
    return 0;
}

#define DB_FLASH_ADDRESS  0x48000

#define USE_CYCLE_STORAGE   (1)

#if(USE_CYCLE_STORAGE)

#define FLASH_EMPTY_FLAG    0xFFFFFFFF
#define FLASH_INIT_FLAG     0xA5A5A5A5
#define FLASH_VERSION_1     0x00000001

#pragma pack (push, 4)
typedef struct
{
    uint32_t init_flag;
    uint32_t version;
} flash_header_t;
#pragma pack (pop)


#define BLOCK_SIZE                  512     // Each block max size is 512 bytes.
#define PAGE_SIZE                   0x2000  // For ing918, every page has 8K bytes.
#define MAX_BLOCK_NUM               (PAGE_SIZE / BLOCK_SIZE) // 16 BLOCKS
#define MAX_BLOCK_DATA_NUM          (BLOCK_SIZE - sizeof(flash_header_t)) // max valid data number(bytes).

static int check_flash_block_empty(uint32_t block_addr, int size)
{
    uint8_t *pData = (uint8_t *)block_addr;
    for(int i=0; i<size; i++){
        if(pData[i] != 0xFF){
            return 0; // not empty
        }
    }
    return 1; //empty
}

static void write_block(uint32_t addr, void *data, int size, int erase)
{
    flash_header_t header = {
        .init_flag = FLASH_INIT_FLAG,
        .version   = FLASH_VERSION_1,
    };
    if(erase)
    {
        erase_flash_page(DB_FLASH_ADDRESS);
    }
    write_flash((const uint32_t)addr, (const void *)&header, sizeof(flash_header_t));
    write_flash((const uint32_t)(addr + sizeof(flash_header_t)), (const void *)data, (size + 3) & ~3);
    log_printf("write data to addr:%#x .\r\n", (addr + sizeof(flash_header_t)));
}
#endif

int read_db_flash(void *data, int size)
{
#if(USE_CYCLE_STORAGE)
    if(size > MAX_BLOCK_DATA_NUM)
    {
        log_error("Too many data to READ: %d(max_block_size=%d)\r\n", size, MAX_BLOCK_DATA_NUM);
        return -1; // too many data to read.
    }
    
    uint32_t index, block_addr, data_addr;
    flash_header_t *pFlashHeader = NULL;

    for(index=0; index<MAX_BLOCK_NUM; index++)
    {
        block_addr = (DB_FLASH_ADDRESS + (MAX_BLOCK_NUM - 1 - index) * BLOCK_SIZE); // Look from back to front
        pFlashHeader = (flash_header_t *)block_addr;
        data_addr  = block_addr + sizeof(flash_header_t);

        if(pFlashHeader->init_flag == FLASH_INIT_FLAG)
        {
            // copy data.
            log_printf("copy addr:%#x data to read buf.\r\n", data_addr);
            memcpy(data, (void *)data_addr, size);
            return 0;
        }
        else if(pFlashHeader->init_flag == FLASH_EMPTY_FLAG)
        {
            if (index == (MAX_BLOCK_NUM-1)) // last one
            {
                // Not found valid param.
                log_printf("param not exist, set 0xFF!\r\n");
                memset(data, 0xFF, size);
                return 0;
            }
            else
            {
                // Block is already occupied, search for the next one.
                log_printf("rd,next:%d;", (MAX_BLOCK_NUM - 1 - index));
                continue;
            }
        }
        else
        {
            // Block data is invalid.
            log_error("block data is invalid, set 0xFF!\r\n");
            memset(data, 0xFF, size);
            return -2;
        }
    }
    log_printf("\r\n");
#else
    memcpy(data, (void *)DB_FLASH_ADDRESS, size);
#endif    
    return 0;
}

int write_db_flash(void *data, int size)
{
#if(USE_CYCLE_STORAGE)
    if(size > MAX_BLOCK_DATA_NUM)
    {
        log_error("Too many data to WRITE: %d(max_block_size=%d)\r\n", size, MAX_BLOCK_DATA_NUM);
        return -1; // too many data to storage.
    }
    
    uint32_t index, block_addr;
    flash_header_t *pFlashHeader = NULL;

    for(index=0; index<MAX_BLOCK_NUM; index++)
    {
        block_addr = (DB_FLASH_ADDRESS + index * BLOCK_SIZE);
        pFlashHeader = (flash_header_t *)block_addr;

        if(pFlashHeader->init_flag == FLASH_INIT_FLAG)
        {
            if (index == (MAX_BLOCK_NUM-1)) // last one
            {
                // Not found empty position, erase the entire page and rewrite from scratch.
                log_printf("page is full, restart.\r\n");
                write_block(DB_FLASH_ADDRESS, data, size, 1); // erase
                return 0;
            }
            else
            {
                // Block is already occupied, search for the next one.
                log_printf("wr,next:%d;", index);
                continue;
            }
        }
        else if(pFlashHeader->init_flag == FLASH_EMPTY_FLAG)
        {
            if(!check_flash_block_empty(block_addr, BLOCK_SIZE))
            {
                // This block data is not empty, erase the entire page and rewrite from scratch.
                log_error("block not empty, restart.\r\n");
                write_block(DB_FLASH_ADDRESS, data, size, 1); // erase
                return 0;
            }
            else
            {
                // Find an empty block, store the data.
                log_printf("normal write flash.\r\n");
                write_block(block_addr, data, size, 0);
                return 0;
            }
        }
        else
        {
            // Block data is invalid, erase the entire page and rewrite from scratch.
            log_error("block data is invalid, restart.\r\n");
            write_block(DB_FLASH_ADDRESS, data, size, 1); // erase
            return 0;
        }
    }
    log_printf("\r\n");
#else
    if(memcmp((void *)data, (void *)DB_FLASH_ADDRESS, size) == 0)
        return 0; // If the same data, then quit.
    program_flash(DB_FLASH_ADDRESS, (const uint8_t *)data, (size + 3) & ~3);
#endif
    return 0;
}
//extern void app_rx_frame(void *user_data, simple_frame_t *frame);
extern void comm_frame_rx_byte(comm_fsm_t *fsm, uint8_t b);




trace_rtt_t trace_ctx = {0};

void jump_to_pulse_app(void);
extern void handle_certi_app_frame(simple_frame_t *frame);

extern const gen_os_driver_t *os_impl_get_driver(void);
int _write(int fd, char *ptr, int len)
{
    int i;
    for (i = 0; i < len; i++)
        cb_putc(ptr + i, NULL);

    return len;
}


uintptr_t app_main()
{
    #if 1
    setup_peripherals();
#if DEBUG_DISCONNECT
	platform_config(PLATFORM_CFG_OSC32K_EN, PLATFORM_CFG_DISABLE);
	platform_config(PLATFORM_CFG_32K_CLK_ACC, 800);
#endif
	
 	#if (RELEASE_VERSION != RTK_ZHONG_CHEN_VERSION)
   // rf_enable_powerboost();
	#endif
	
    SYSCTRL_ClearClkGateMulti(1 << SYSCTRL_ClkGate_APB_TMR0);
    TMR0_UNLOCK();	
    //TMR_WatchDogEnable(TMR_CLK_FREQ * 10);
    TMR0_LOCK();	

    platform_set_evt_callback(PLATFORM_CB_EVT_PUTC, (f_platform_evt_cb)cb_putc, NULL);

#if DEBUG_DISCONNECT
	platform_config(PLATFORM_CFG_LL_DBG_FLAGS, 0x10);
#endif

    // setup handlers
    //platform_set_evt_callback(PLATFORM_CB_EVT_HARD_FAULT, (f_platform_evt_cb)cb_hard_fault, NULL);
    //platform_set_evt_callback(PLATFORM_CB_EVT_ASSERTION, (f_platform_evt_cb)cb_assertion, NULL);
   // platform_set_evt_callback(PLATFORM_CB_EVT_HEAP_OOM, (f_platform_evt_cb)cb_heap_oom, NULL);
    platform_set_evt_callback(PLATFORM_CB_EVT_ON_DEEP_SLEEP_WAKEUP, on_deep_sleep_wakeup, NULL);
    platform_set_evt_callback(PLATFORM_CB_EVT_QUERY_DEEP_SLEEP_ALLOWED, query_deep_sleep_allowed, NULL);



    platform_set_evt_callback(PLATFORM_CB_EVT_PROFILE_INIT, setup_profile, NULL);
    #endif
    //setup_peripherals();
    //while(1)
    //printf("hello\r\n");
//    trace_rtt_init(&trace_ctx);
//    platform_set_evt_callback(PLATFORM_CB_EVT_TRACE, (f_platform_evt_cb)cb_trace_rtt, &trace_ctx);
//    platform_config(PLATFORM_CFG_TRACE_MASK, 0x1ff);
    return (uintptr_t)os_impl_get_driver();
}

static uint32_t ClkFreq;

#include "eflash.inc"

void cache_enable(int enable)
{
    if (enable)
        EflashCacheEna();
    else
        EflashCacheBypass();
}

void switch_to_pulse_app(void)
{
    jump_to_pulse_app();
}

void jump_to_pulse_app(void)
{
    typedef  void (*pFunction)(void);
    __disable_irq();
    #define JumpAddr 0x4A000
    {
        int i;
        uint32_t jump =  *(uint32_t *) (JumpAddr + 4);
        SysTick->CTRL = 0;

        *(uint32_t *)0x40070008 = 0ul;
        *(uint32_t *)0x40070008 = 0x3ffffful;

        // restore to default
        PINCTRL_SetPadMux(3, IO_SOURCE_GENERAL);
        PINCTRL_SelUartRxdIn(UART_PORT_0, 3);
        PINCTRL_SetPadMux(2, IO_SOURCE_UART0_TXD);

        for (i = 0; i < 32; i++)
        {
            NVIC_DisableIRQ((IRQn_Type)i);
        }

        __set_CONTROL(0);

        __set_MSP(*(uint32_t *)(JumpAddr));

        ((pFunction)(jump))();
    }
}
