#include <string.h>
#include "uart_driver.h"
#include "los_task.h"
#include "peripheral_uart.h"
#include "platform_api.h"
#include "ohos_init.h"
#include "cmsis_os2.h"
#include  "los_mux.h"
#include "los_sem.h"

#ifndef UART_BUFF_SIZE
#define UART_BUFF_SIZE         (4096)  // must be 2^n
#endif

#define UART_BUFF_SIZE_MASK    (UART_BUFF_SIZE - 1)

typedef struct
{
    uint8_t           buffer[UART_BUFF_SIZE];
    uint16_t          write_next;
    uint16_t          read_next;
    //TaskHandle_t      handle;
    uint32_t         tx_sem;
    uint32_t          mutex;
    UART_TypeDef     *port;
    f_uart_rx_byte    rx_byte_cb;
    void             *user_data;
} uart_driver_ctx_t;

static uart_driver_ctx_t ctx = {0};

#warning "simulate slave"
//#define TEST_MODE_SLAVE_DEV
#define TEST_MODE_SLAVE_DEV_PIN
#define TEST_MODE_SLAVE_DEV_1

#ifdef TEST_MODE_SLAVE_DEV
uint8_t init_slave_power_on[] =  {0xFE, 0xFE, 0xFE, 0xFE,  
                                  0x68, 0x02, 0x00, 0xC4, 0x07, 0x00, 0x00, 0x00, 
                                  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x68, 0x01, 0x00, 
                                  0x98, 0x16};

#ifdef TEST_MODE_SLAVE_DEV_1
#ifdef TEST_MODE_SLAVE_DEV_PIN
uint8_t init_slave_C401_pin[] =  {0xFE, 0xFE, 0xFE, 0xFE, 
                                  0x68, 0x09, 0x00, 0x02, 0x00, 0x00, 0x0B, 0xF2, 
                                  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 
                                  0x68, 0x01, 0x02, 0x06, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 
                                  0x10, 0x16};
#else
uint8_t init_slave_C401_pin[] =  {};
#endif
uint8_t init_slave_C401_addr[] = {0xFE, 0xFE, 0xFE, 0xFE, 
                                  0x68, 0x18, 0x00, 0x02, 0x01, 0x00, 0x0B, 0xF2, 
                                  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 
                                  0x68, 0xC3, 0x20, 0x20, 0x20, 0x20, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
                                  0x14, 0x16};
uint8_t init_slave_C401_addt[] = {0xFE, 0xFE, 0xFE, 0xFE, 
                                  0x68, 0x1F, 0x00, 0x02, 0x0E, 0x00, 0x00, 0x00, 
                                  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 
								  0x68, 0x02, 0x01, 0x06, 0x16, 0xff, 0xc3, 0xff, 0xff, 0x00, 0x00, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x04, 0x08, 0x50, 0x65, 0x72,
                                  0x36, 0x16};
#define INIT_SLAVE_PIN        init_slave_C401_pin
#define INIT_SLAVE_ADDR       init_slave_C401_addr
#define INIT_SLAVE_ADV_DATA   init_slave_C401_addt
#endif

#ifdef TEST_MODE_SLAVE_DEV_2
#ifdef TEST_MODE_SLAVE_DEV_PIN
uint8_t init_slave_C402_pin[] =  {0xFE, 0xFE, 0xFE, 0xFE, 
                                  0x68, 0x09, 0x00, 0x02, 0x00, 0x00, 0x0B, 0xF2, 
                                  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 
                                  0x68, 0x01, 0x02, 0x06, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 
                                  0x10, 0x16};
#else
uint8_t init_slave_C402_pin[] =  {};
#endif
uint8_t init_slave_C402_addr[] = {0xFE, 0xFE, 0xFE, 0xFE, 
                                  0x68, 0x18, 0x00, 0x02, 0x01, 0x00, 0x0B, 0xF2, 
                                  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 
                                  0x68, 0xC3, 0x20, 0x20, 0x20, 0x20, 0x02, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
                                  0x15, 0x16};
uint8_t init_slave_C402_addt[] = {0xFE, 0xFE, 0xFE, 0xFE, 
                                  0x68, 0x1F, 0x00, 0x02, 0x0E, 0x00, 0x00, 0x00, 
                                  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 
								  0x68, 0x02, 0x01, 0x06, 0x16, 0xff, 0xc3, 0xff, 0xff, 0x00, 0x00, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x04, 0x08, 0x50, 0x65, 0x72,
                                  0x36, 0x16};
#define INIT_SLAVE_PIN        init_slave_C402_pin
#define INIT_SLAVE_ADDR       init_slave_C402_addr
#define INIT_SLAVE_ADV_DATA   init_slave_C402_addt
#endif

#ifdef TEST_MODE_SLAVE_DEV_3
#ifdef TEST_MODE_SLAVE_DEV_PIN
uint8_t init_slave_C403_pin[] =  {0xFE, 0xFE, 0xFE, 0xFE, 
                                  0x68, 0x09, 0x00, 0x02, 0x00, 0x00, 0x0B, 0xF2, 
                                  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 
                                  0x68, 0x01, 0x02, 0x06, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 
                                  0x10, 0x16};
#else
uint8_t init_slave_C403_pin[] =  {};
#endif
uint8_t init_slave_C403_addr[] = {0xFE, 0xFE, 0xFE, 0xFE, 
                                  0x68, 0x18, 0x00, 0x02, 0x01, 0x00, 0x0B, 0xF2, 
                                  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 
                                  0x68, 0xC3, 0x20, 0x20, 0x20, 0x20, 0x03, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
                                  0x16, 0x16};
uint8_t init_slave_C403_addt[] = {0xFE, 0xFE, 0xFE, 0xFE, 
                                  0x68, 0x1F, 0x00, 0x02, 0x0E, 0x00, 0x00, 0x00, 
                                  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 
								  0x68, 0x02, 0x01, 0x06, 0x16, 0xff, 0xc3, 0xff, 0xff, 0x00, 0x00, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x04, 0x08, 0x50, 0x65, 0x72,
                                  0x36, 0x16};
#define INIT_SLAVE_PIN        init_slave_C403_pin
#define INIT_SLAVE_ADDR       init_slave_C403_addr
#define INIT_SLAVE_ADV_DATA   init_slave_C403_addt
#endif

#endif

void uart_driver_task(uart_driver_ctx_t *ctx)
{
#ifdef TEST_MODE_SLAVE_DEV
    int i;
    
    vTaskDelay(pdMS_TO_TICKS(2000));
    for (i = 0; i < sizeof(init_slave_power_on); i++) {
        ctx->rx_byte_cb(ctx->user_data, init_slave_power_on[i]);
    }
    vTaskDelay(pdMS_TO_TICKS(500));
    for (i = 0; i < sizeof(INIT_SLAVE_PIN); i++) {
        ctx->rx_byte_cb(ctx->user_data, INIT_SLAVE_PIN[i]);
    }
    vTaskDelay(pdMS_TO_TICKS(500));
    for (i = 0; i < sizeof(INIT_SLAVE_ADDR); i++) {
        ctx->rx_byte_cb(ctx->user_data, INIT_SLAVE_ADDR[i]);
    }
    vTaskDelay(pdMS_TO_TICKS(500));
    for (i = 0; i < sizeof(INIT_SLAVE_ADV_DATA); i++) {
        ctx->rx_byte_cb(ctx->user_data, INIT_SLAVE_ADV_DATA[i]);
    }
#endif
    
    for (;;)
    {
wait:
        LOS_SemPend(ctx->tx_sem, LOS_WAIT_FOREVER);
        while (ctx->read_next != ctx->write_next)
        {
            if (!apUART_Check_TXFIFO_FULL(ctx->port))
            {
                UART_SendData(ctx->port, ctx->buffer[ctx->read_next]);
                //UART_SendData(APB_UART0, ctx->buffer[ctx->read_next]);
                ctx->read_next = (ctx->read_next + 1) & UART_BUFF_SIZE_MASK;
            }
            else
                goto wait;
        }
    }
}

void driver_trigger_output(uart_driver_ctx_t *ctx)
{

        //printf("driver_trigger_output\r\n");
        LOS_SemPost(ctx->tx_sem);
}

uint32_t uart_driver_isr(void *user_data)
{
    uint32_t status;

    while(1)
    {
        status = apUART_Get_all_raw_int_stat(ctx.port);
        if (status == 0)
            break;

        ctx.port->IntClear = status;

        // rx int
        if (status & (1 << bsUART_RECEIVE_INTENAB))
        {
            while (apUART_Check_RXFIFO_EMPTY(ctx.port) != 1)
            {
                uint8_t b = ctx.port->DataRead;
                //UART_SendData(APB_UART0, b);
                //printf("%02x-",b);
                ctx.rx_byte_cb(ctx.user_data, b);
            }
        }

        // tx int
        if (status & (1 << bsUART_TRANSMIT_INTENAB))
            driver_trigger_output(&ctx);
    }
    return 0;
}

void driver_flush_tx()
{
    while (ctx.read_next != ctx.write_next)
    {
        while (!apUART_Check_TXFIFO_FULL(ctx.port))
        {
            UART_SendData(ctx.port, ctx.buffer[ctx.read_next]);
            //UART_SendData(APB_UART0, ctx.buffer[ctx.read_next]);
            ctx.read_next = (ctx.read_next + 1) & UART_BUFF_SIZE_MASK;
        }
    }
    while (!apUART_Check_TXFIFO_EMPTY(APB_UART0)) ;
    while (!apUART_Check_TXFIFO_EMPTY(APB_UART1)) ;

    APB_UART0->IntClear = apUART_Get_all_raw_int_stat(APB_UART0);
    APB_UART1->IntClear = apUART_Get_all_raw_int_stat(APB_UART1);
}

int uart_add_buffer(uart_driver_ctx_t *ctx, const void *buffer, int size, int start)
{
    int remain = UART_BUFF_SIZE - start;
    if (remain >= size)
    {
        memcpy(ctx->buffer + start, buffer, size);
        return start + size;
    }
    else
    {
        memcpy(ctx->buffer + start, buffer, remain);
        size -= remain;
        memcpy(ctx->buffer, (const uint8_t *)buffer + remain, size);
        return size;
    }
}

uint32_t driver_append_tx_data(const void *data, int len)
{
    uint16_t next;
    int16_t free_size;
    uint8_t use_mutex = !IS_IN_INTERRUPT();
    printf("driver_append_tx_data\r\n");
    if (use_mutex)
        LOS_MuxPend(ctx.mutex, LOS_WAIT_FOREVER);

    printf("get mutex\r\n");
    next = ctx.write_next;
    free_size = ctx.read_next - ctx.write_next;
    if (free_size <= 0) free_size += UART_BUFF_SIZE;
    if (free_size > 0) free_size--;

    if (len > free_size)
    {
        if (use_mutex)
            LOS_MuxPost(ctx.mutex);
        driver_trigger_output(&ctx);
        return 1;
    }

    next = uart_add_buffer(&ctx, data, len, next) & UART_BUFF_SIZE_MASK;

    ctx.write_next = next;

    if (use_mutex) LOS_MuxPost(ctx.mutex);

    driver_trigger_output(&ctx);
    return 0;
}
#define SEM_INITIAL_VALUE 3
#define SEM_MAX_VALUE 0
#define ATTR_BITS 0U
#define CB_SIZE_0U 0U
#define STACK_SIZE (128 * 4)
#define PRIORITY_LEVEL 5



void uart_driver_init(UART_TypeDef *port, void *user_data, f_uart_rx_byte rx_byte_cb)
{
    LOS_BinarySemCreate(0,&(ctx.tx_sem));
    LOS_MuxCreate(&(ctx.mutex));
    ctx.port = port;
    ctx.rx_byte_cb = rx_byte_cb;
    ctx.user_data = user_data;
 {
    osThreadAttr_t attr;

    attr.name = "uart ";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = STACK_SIZE;
    attr.priority = PRIORITY_LEVEL;

    if (osThreadNew((osThreadFunc_t)uart_driver_task, &ctx, &attr) == NULL) {
        printf("Failed to create uart task!\r\n");
    }else {
        printf("ok to create uart task!\r\n");;
    }
}
#if 0
{
    UINT32 tid;
    TSK_INIT_PARAM_S task_init_param = {0};
    task_init_param.usTaskPrio = 5; 
    task_init_param.pcName = "uart"; 
    task_init_param.pfnTaskEntry = (TSK_ENTRY_FUNC)uart_driver_task;
    task_init_param.uwStackSize = 1024;        
    UINT32 ret = LOS_TaskCreate(&tid, &task_init_param); 
    if (LOS_OK == ret)
    printf("LOS_TaskCreate uart ok!");
    else
     printf("LOS_TaskCreate uart fail!") ;
}
#endif
}