
/*
** COPYRIGHT (c) 2022 by INGCHIPS
*/
#ifdef LOG_FLASH_ENABLE

#include "string.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "platform_api.h"
#include "log_flash.h"
#include "main.h"

#if defined __cplusplus
    extern "C" {
#endif
//#define dbg_flash_printf(...) platform_printf(__VA_ARGS__)
#define dbg_flash_printf(...)

static uint32_t log_flash_write_page_addr = 0;
static uint32_t log_flash_write_addr = 0;
static uint8_t log_flash_page_flag[LOG_FLASH_PAGE_NUM] = {0};
static uint8_t log_flash_write_page_flag = 0;

static void init_new_page(uint32_t page_addr, uint8_t page_flag)
{
    uint8_t page_flag_data[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    
    page_flag_data[0] = page_flag;
    
    program_flash(page_addr, page_flag_data, sizeof(page_flag_data));
    dbg_flash_printf("init pageflag %02x 0x%08x\r\n", page_flag, page_addr);
    
    return;
}

static void init_page_flag_array(void)
{
    int i;
    uint32_t p_log_flash_page_flag = 0;
    
    for (i = 0; i < LOG_FLASH_PAGE_NUM; i++) {
        p_log_flash_page_flag = (LOG_FLASH_START_ADDRESS + i * LOG_FLASH_PAGE_SIZE);
        log_flash_page_flag[i] = *(uint8_t *)p_log_flash_page_flag;
        dbg_flash_printf(" %02x", log_flash_page_flag[i]);
    }
    dbg_flash_printf("\r\n");
    return;
}

static uint32_t find_start_page_frome_flash(void)
{
// 0 X X X X X ----mode a
// 0 1 X X X X ----mode b
// 0 1 2 3 4 5 ----mode c

// 6 1 2 3 4 5 ----mode d
// 6 7 2 3 4 5 ----mode e
// 6 7 8 9 A B ----mode f
    int i;
    uint32_t page_index;
    
    init_page_flag_array();
    
    for (i = 1; i < LOG_FLASH_PAGE_NUM; i++) {
        if (log_flash_page_flag[i] == ((log_flash_page_flag[i - 1] + 1) & 0xFF)) {
            continue;
        } else {
            break;
        }
    }
    
    if (i == LOG_FLASH_PAGE_NUM) {
        page_index = 0;
    } else {
        page_index = i;
    }
    

    return page_index;
}

static SemaphoreHandle_t sem_log_flash_read = NULL;
static SemaphoreHandle_t sem_log_flash_write = NULL;
static int m_log_last = 0;

static void log_flash_read_all(void)
{
    if (sem_log_flash_read == NULL) return;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(sem_log_flash_read, &xHigherPriorityTaskWoken);
}

static void log_flash_read_last(void)
{
    if (sem_log_flash_read == NULL) return;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    m_log_last = 1;
    xSemaphoreGiveFromISR(sem_log_flash_read, &xHigherPriorityTaskWoken);
}

uint32_t cb_putc(char *c, void *dummy);
static void print_log_flash_str(uint8_t len, uint8_t *p_data)
{
    int i;
    uint32_t ms;
    uint32_t s;
    uint32_t m;
    uint32_t h;
    
    ms = p_data[0];
    ms = (ms << 8) | p_data[1];
    ms = (ms << 8) | p_data[2];
    
    s = ms / 1000;
    ms = ms % 1000;
    
    m = s / 60;
    s = s % 60;
    
    h = m / 60;
    m = m % 60;
    platform_printf("[%02u:%02u:%02u.%03u] [%06u] ", h, m, s, ms, p_data);
    
    for (i = 3; i < len; i++) {
        cb_putc((char *)(p_data + i), 0);
    }
    platform_printf("\r\n");
    return;
}

static void print_log_flash_hex(uint8_t len, uint8_t *p_data)
{
    int i;
    uint32_t ms;
    uint32_t s;
    uint32_t m;
    uint32_t h;
    
    ms = p_data[0];
    ms = (ms << 8) | p_data[1];
    ms = (ms << 8) | p_data[2];
    
    s = ms / 1000;
    ms = ms % 1000;
    
    m = s / 60;
    s = s % 60;
    
    h = m / 60;
    m = m % 60;
    platform_printf("[%02u:%02u:%02u.%03u] [%06u] ", h, m, s, ms, p_data);
    
    for (i = 3; i < len; i++) {
        platform_printf("%02x ", p_data[i]);
    }
    platform_printf("\r\n");
    return;
}

static void print_log_flash(uint32_t addr)
{
    int i;
    uint8_t *p_data = (uint8_t *)addr;
    uint8_t data_len;
    
    if ((p_data[4] & 0xF0) != 0xA0) {
        return;
    }
    
    i = 4;
    while (i < LOG_FLASH_PAGE_SIZE) {
        switch (p_data[i]) {
            
        case LOG_FLASH_TLV_RX_EVENT:
            platform_printf("[revt]: ");
            data_len = p_data[i + 1];
            print_log_flash_hex(data_len, &p_data[i + 2]);
            i += (data_len + 2);
            break;
            
        case LOG_FLASH_TLV_TX_EVENT:
            platform_printf("[tevt]: ");
            data_len = p_data[i + 1];
            print_log_flash_hex(data_len, &p_data[i + 2]);
            i += (data_len + 2);
            break;
            
        case LOG_FLASH_TLV_CONNECT_EVENT:
            platform_printf("[conn]: ");
            data_len = p_data[i + 1];
            print_log_flash_hex(data_len, &p_data[i + 2]);
            i += (data_len + 2);
            break;
            
        case LOG_FLASH_TLV_DISCCONNECT_EVENT:
            platform_printf("[disc]: ");
            data_len = p_data[i + 1];
            print_log_flash_hex(data_len, &p_data[i + 2]);
            i += (data_len + 2);
            break;
        
        case LOG_FLASH_TLV_INFO:
            platform_printf("[info]: ");
            data_len = p_data[i + 1];
            print_log_flash_str(data_len, &p_data[i + 2]);
            i += (data_len + 2);
            break;
            
        case LOG_FLASH_TLV_WARN:
            platform_printf("[warn]: ");
            data_len = p_data[i + 1];
            print_log_flash_str(data_len, &p_data[i + 2]);
            i += (data_len + 2);
            break;
            
        case LOG_FLASH_TLV_ERROR:
            platform_printf("[eror]: ");
            data_len = p_data[i + 1];
            print_log_flash_str(data_len, &p_data[i + 2]);
            i += (data_len + 2);
            break;
        
        default:
            i++;
            break;
        }
    }
    
    return;
}

#warning "flag_log_flash_en"
static int flag_log_flash_en = 0;
static void log_flash_read_all_task(void *pdata)
{
    uint32_t page_index;
    int i;
    
    for (;;)
    {
        BaseType_t r = xSemaphoreTake(sem_log_flash_read,  portMAX_DELAY);
        if (pdTRUE != r) continue;
        
        // Record log-enable-status and disable writing log to flash.
        int log_flash_en_record = flag_log_flash_en;
        flag_log_flash_en = 0;

        platform_printf("log:\r\n");
        
        page_index = find_start_page_frome_flash();
        i = 0;
        
        if (m_log_last == 1) {
            m_log_last = 0;
            page_index = page_index + LOG_FLASH_PAGE_NUM - 2;
            i = LOG_FLASH_PAGE_NUM - 2;
        }
        
        for (; i < LOG_FLASH_PAGE_NUM; i++) {
            if (page_index >= LOG_FLASH_PAGE_NUM) {
                page_index = page_index - LOG_FLASH_PAGE_NUM;
            }
            print_log_flash(page_index * LOG_FLASH_PAGE_SIZE + LOG_FLASH_START_ADDRESS);
            page_index++;
        }

        platform_printf("end:\r\n");
        
        // Recovery log enable status.
        flag_log_flash_en = log_flash_en_record;
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

#define ADDR_PACK4 0x3
#define ADDR_PACK4_MASK (~0x3)
#define RTC_VALUE_LEN 3
static void log_write_to_flash(uint8_t type, uint8_t length, const uint8_t *data)
{
    uint32_t write_flash_size;
    uint32_t write_end_data_size;
    uint8_t write_start[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t write_end[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint64_t rtc;
    int i;
    
    rtc = RTC_Current();
    rtc = rtc * 1000 / 32768;
    if (length == 1) {
        write_start[0] = type;
        write_start[1] = length + RTC_VALUE_LEN;
        write_start[2] = rtc >> 16;
        write_start[3] = rtc >> 8;
        write_start[4] = rtc;
        write_start[5] = data[0];
        write_flash(log_flash_write_page_addr + log_flash_write_addr, write_start, 8);
        log_flash_write_addr += 8;
    } else if (length == 2) {
        write_start[0] = type;
        write_start[1] = length + RTC_VALUE_LEN;
        write_start[2] = rtc >> 16;
        write_start[3] = rtc >> 8;
        write_start[4] = rtc;
        write_start[5] = data[0];
        write_start[6] = data[1];
        write_flash(log_flash_write_page_addr + log_flash_write_addr, write_start, 8);
        log_flash_write_addr += 8;
    } else {
        write_start[0] = type;
        write_start[1] = length + RTC_VALUE_LEN;
        write_start[2] = rtc >> 16;
        write_start[3] = rtc >> 8;
        write_start[4] = rtc;
        write_start[5] = data[0];
        write_start[6] = data[1];
        write_start[7] = data[2];
        write_flash(log_flash_write_page_addr + log_flash_write_addr, write_start, 8);
        log_flash_write_addr += 8;

        write_flash_size = length - 3;
        write_flash_size &= ADDR_PACK4_MASK;
        write_flash(log_flash_write_page_addr + log_flash_write_addr, &data[3] , write_flash_size);
        log_flash_write_addr += write_flash_size;
        
        write_end_data_size = (length - 3) & ADDR_PACK4;
        for (i = 0; i < write_end_data_size; i++) {
            write_end[i] = data[length + i - write_end_data_size];
        }
        write_flash(log_flash_write_page_addr + log_flash_write_addr, write_end, 4);
        log_flash_write_addr += 4;
    }
    return;
}

#define LOG_BUF_DEEP 4
static uint8_t log_type_buf[LOG_BUF_DEEP];
static uint8_t log_length_buf[LOG_BUF_DEEP];
static uint8_t log_data_buf[LOG_BUF_DEEP][256];
static int log_data_buf_index = 0;
static int log_data_buf_write_index = 0;
static void log_flash_write_task(void *pdata)
{
    uint32_t page_left_size;
    uint8_t segment_size;
    uint8_t *log_data = 0;
    uint8_t log_type;
    uint8_t log_length;
    
    for (;;)
    {
        BaseType_t r = xSemaphoreTake(sem_log_flash_write,  portMAX_DELAY);
        if (pdTRUE != r) continue;
        
        while (log_data_buf_write_index < log_data_buf_index) {
            log_data = &log_data_buf[log_data_buf_write_index % LOG_BUF_DEEP][0];
            log_type = log_type_buf[log_data_buf_write_index % LOG_BUF_DEEP];
            log_length = log_length_buf[log_data_buf_write_index % LOG_BUF_DEEP];
            
            dbg_flash_printf("logaddr %06u %u\r\n", log_flash_write_page_addr + log_flash_write_addr, log_length);
            page_left_size = LOG_FLASH_PAGE_SIZE - log_flash_write_addr;
            if (page_left_size < log_length + 7) {
                if (page_left_size > 7) {
                    segment_size = page_left_size - 7;
                    log_write_to_flash(log_type, segment_size, log_data);
                } else {
                    segment_size = 0;
                }
                
                log_flash_write_page_addr += LOG_FLASH_PAGE_SIZE;
                if (log_flash_write_page_addr >= LOG_FLASH_END_ADDRESS) {
                    log_flash_write_page_addr = LOG_FLASH_START_ADDRESS;
                }
                log_flash_write_page_flag += 1;
                init_new_page(log_flash_write_page_addr, log_flash_write_page_flag);
                log_flash_write_addr = 4;
                log_write_to_flash(log_type, log_length - segment_size, &log_data[segment_size]);
            } else {
                log_write_to_flash(log_type, log_length, log_data);
            }
            
            log_data_buf_write_index++;
        }
    }
}

#define CMD_LEN 8
static uint8_t cmd_buf[CMD_LEN] = {0};

static void recv_log_uart_data(uint8_t data)
{
    static int state_machine = 0;
    static int cmd_index = 0;
    
    switch (state_machine) {
    case 0:
        if (data == '#') {
            state_machine++;
        }
        break;
    case 1:
        if (data == '$') {
            state_machine++;
        } else {
            state_machine = 0;
        }
        break;
    case 2:
        cmd_buf[cmd_index] = data;
        cmd_index++;
        if (cmd_index >= CMD_LEN) {
            state_machine = 0;
            cmd_index = 0;
            
            if (memcmp("printlog", cmd_buf, CMD_LEN) == 0) {
                log_flash_read_all();
            } else if (memcmp("printlst", cmd_buf, CMD_LEN) == 0) {
                log_flash_read_last();
            } else if (memcmp("stopflog", cmd_buf, CMD_LEN) == 0) {
                flag_log_flash_en = 0;
            } else if (memcmp("startlog", cmd_buf, CMD_LEN) == 0) {
                flag_log_flash_en = 1;
            } else {
            }
        } 
        break;
    default:
        state_machine = 0;
        break;
    }
    
    return;
}

static uint32_t log_uart_isr(void *user_data)
{
    uint32_t status;

    while(1)
    {
        status = apUART_Get_all_raw_int_stat(APB_UART0);
        if (status == 0)
            break;

        APB_UART0->IntClear = status;

        // rx int
        if (status & (1 << bsUART_RECEIVE_INTENAB))
        {
            while (apUART_Check_RXFIFO_EMPTY(APB_UART0) != 1)
            {
                uint8_t b = APB_UART0->DataRead;
                recv_log_uart_data(b);
            }
        }
    }
    return 0;
}

void log_uart_irq_init(void)
{
    platform_set_irq_callback(PLATFORM_CB_IRQ_UART0, log_uart_isr, NULL);
}

static int find_and_init_space_in_before_page(uint32_t page_index)
{
    int i;
    int null_cnt;
    int data_len;
    uint8_t *p_data = (uint8_t *)(page_index * LOG_FLASH_PAGE_SIZE + LOG_FLASH_START_ADDRESS);
    
    if ((p_data[1] != 0xFF) ||
        (p_data[2] != 0xFF) ||
        (p_data[3] != 0xFF) ||
        ((p_data[4] & 0xF0) != 0xA0)) {
            log_flash_write_page_flag = log_flash_page_flag[page_index];
            log_flash_write_page_addr = page_index * LOG_FLASH_PAGE_SIZE + LOG_FLASH_START_ADDRESS;
            init_new_page(log_flash_write_page_addr, log_flash_write_page_flag);
            log_flash_write_addr = 4;
            return 0;
    }
        
    i = 4;
    null_cnt = 0;
    while (i < (LOG_FLASH_PAGE_SIZE - 16)) {
        if (p_data[i] == 0xFF) {
            null_cnt++;
            i++;
        } else if ((p_data[i] & 0xF0) == 0xA0) {
            null_cnt = 0;
            data_len = p_data[i + 1];
            i += (data_len + 2);
        } else {
            log_flash_write_page_flag = log_flash_page_flag[page_index];
            log_flash_write_page_addr = page_index * LOG_FLASH_PAGE_SIZE + LOG_FLASH_START_ADDRESS;
            dbg_flash_printf("find_and_init_space_in_before_page\r\n");
            init_new_page(log_flash_write_page_addr, log_flash_write_page_flag);
            log_flash_write_addr = 4;
            return 0;
        }
        
        if (null_cnt >= 8) {
            log_flash_write_page_flag = log_flash_page_flag[page_index];
            log_flash_write_page_addr = page_index * LOG_FLASH_PAGE_SIZE + LOG_FLASH_START_ADDRESS;
            log_flash_write_addr = ((i - 4) & ADDR_PACK4_MASK);
            return 0;
        }
    }
    
    return -1;
}

static int log_flash_init(void)
{
    static uint8_t init_flag = 0;
    uint32_t page_index;
    uint32_t before_page_index;
    
    if (init_flag == 1) {
        return 0;
    }
    
    page_index = find_start_page_frome_flash();
    
    if (page_index == 0) {
        before_page_index = LOG_FLASH_PAGE_NUM - 1;
    } else {
        before_page_index = page_index - 1;
    }
    
    if (find_and_init_space_in_before_page(before_page_index) != 0) {
        log_flash_write_page_flag = log_flash_page_flag[before_page_index] + 1;
        log_flash_write_page_addr = page_index * LOG_FLASH_PAGE_SIZE + LOG_FLASH_START_ADDRESS;
        init_new_page(log_flash_write_page_addr, log_flash_write_page_flag);
        log_flash_write_addr = 4;
    }
    
    sem_log_flash_read = xSemaphoreCreateBinary();
    xTaskCreate(log_flash_read_all_task, "r", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    // log_uart_irq_init();
    
    sem_log_flash_write = xSemaphoreCreateBinary();
    xTaskCreate(log_flash_write_task, "w", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    
    init_flag = 1;
    return 0;
}

void log_flash_write(uint8_t type, uint8_t length, const uint8_t *data)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint8_t *log_data = 0;
    
    if (flag_log_flash_en == 0) {
        return;
    }
    
    if (log_flash_init() != 0) {
        return;
    }
    
    if ((length == 0) || (data == 0)) {
        return;
    }
    
    if (log_data_buf_index == log_data_buf_write_index) {
        log_data_buf_index = 0;
        log_data_buf_write_index = 0;
    }
    log_data = &log_data_buf[log_data_buf_index % LOG_BUF_DEEP][0];
    log_type_buf[log_data_buf_index % LOG_BUF_DEEP] = type;
    log_length_buf[log_data_buf_index % LOG_BUF_DEEP] = length;
    memcpy(log_data, data, length);
    log_data_buf_index++;
    
    xSemaphoreGiveFromISR(sem_log_flash_write, &xHigherPriorityTaskWoken);

    return;
}

void log_flash_enable(int en)
{
    flag_log_flash_en = en;
    return;
}
#if defined __cplusplus
    }
#endif
#endif
