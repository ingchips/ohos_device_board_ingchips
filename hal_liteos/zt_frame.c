#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "zt_frame.h"
#include "platform_api.h"
#include "uart_driver.h"
#include "app_cfg.h"
#include "cross_param.h"
#include "btstack_util.h" 
#include "log_flash.h"
#include "main.h"
#include "cmsis_os2.h"


#define FRAME_START_BYTE        (0x68)
#define FRAME_END_BYTE          (0x16)

void state_wait_frame_start(comm_fsm_t *fsm, uint8_t b);
simple_frame_t *comm_frame_alloc(int body_len);

static void goto_wait_frame_start(comm_fsm_t *fsm)
{
    fsm->lead_cnt = 0;
    fsm->state = state_wait_frame_start;
}

#define GOTO_WAIT_FRAME_START(fmt, ...)  do { platform_printf(fmt, ##__VA_ARGS__); goto_wait_frame_start(fsm); } while (0)

uint8_t calc_check_sum(frame_header_t *header, const uint8_t *body)
{
    const uint8_t *p = (const uint8_t *)header;
    uint8_t sum = (FRAME_START_BYTE * 2);
    int i;
    for (i = 0; i < sizeof(*header); i++)
        sum += p[i];
    p = body;
    for (i = 0; i < header->len; i++)
        sum += p[i];
    return sum;
}

void state_rx_frame_end(comm_fsm_t *fsm, uint8_t b)
{
    simple_frame_t *frame;
    uint8_t sum = (FRAME_START_BYTE * 2);
    #ifdef LOG_FLASH_ENABLE
    uint32_t status = COMM_FSM_FRAME_ERR_NONE;
    #endif
    
    if (b != FRAME_END_BYTE)
    {
        #ifdef LOG_FLASH_ENABLE
        status = COMM_FSM_FRAME_ERR_END_16H_MIS;
        #endif
        GOTO_WAIT_FRAME_START("frame end missing\n");
    }
    else
    {
        sum = calc_check_sum(&fsm->header, (const uint8_t *)&fsm->body);

        if (sum != fsm->cs)
        {
            #ifdef LOG_FLASH_ENABLE
            status = COMM_FSM_FRAME_ERR_CS;
            #endif
            GOTO_WAIT_FRAME_START("CS error: %02x\n", sum);
        }
        else
        {
            goto_wait_frame_start(fsm);

		    frame = comm_frame_alloc(fsm->header.len);
		    if (NULL == frame)
		        platform_raise_assertion("zt_frame.c", __LINE__);
		    frame->header = fsm->header;
		    memcpy(frame->data, fsm->body.data, fsm->header.len);
		    fsm->frame_cb(fsm->user_data, frame);
            #ifdef LOG_FLASH_ENABLE
            log_flash_write(LOG_FLASH_TLV_RX_EVENT, fsm->header.len + sizeof(frame->header), (uint8_t *)frame);
            #endif
        }
    }
    
    #ifdef LOG_FLASH_ENABLE
    if(status)
    {
        log_flash_write(LOG_FLASH_TLV_DEBUG, sizeof(status), (uint8_t *)&status);
    }
    #endif
}

void state_rx_cs(comm_fsm_t *fsm, uint8_t b)
{
    fsm->cs = b;
    fsm->state = state_rx_frame_end;
}

void state_rx_frame_body(comm_fsm_t *fsm, uint8_t b)
{
    uint8_t *p = (uint8_t *)&fsm->body;
    p[fsm->pos++] = b;

    if (fsm->pos < fsm->header.len)
        return;

    fsm->state = state_rx_cs;
}

void state_rx_frame_header(comm_fsm_t *fsm, uint8_t b)
{
    uint8_t *p = (uint8_t *)&fsm->header;
    #ifdef LOG_FLASH_ENABLE
    uint32_t status = COMM_FSM_FRAME_ERR_NONE;
    #endif
    
    if (fsm->pos < sizeof(fsm->header))
    {
        p[fsm->pos++] = b;
    }
    else if (FRAME_START_BYTE == b)
    {
        if (fsm->header.len == 0)
        {
            fsm->state = state_rx_cs;
        }
        else if (fsm->header.len <= DATA_MAX_LEN)
        {
            fsm->state = state_rx_frame_body;
            fsm->pos = 0;
        }
        else
        {
            #ifdef LOG_FLASH_ENABLE
            status = COMM_FSM_FRAME_ERR_HED_LEN_TOO_LONG;
            #endif
			printf("fsm->header.len=%d(0x%04x)\r\n",fsm->header.len,fsm->header.len);
            GOTO_WAIT_FRAME_START("Len too large\n");
        }
    }
    else
    {
        #ifdef LOG_FLASH_ENABLE
        status = COMM_FSM_FRAME_ERR_HED_68H_MIS;
        #endif
        GOTO_WAIT_FRAME_START("68H missing in header\n");
    }
    
    #ifdef LOG_FLASH_ENABLE
    if(status)
    {
        log_flash_write(LOG_FLASH_TLV_DEBUG, sizeof(status), (uint8_t *)&status);
    }
    #endif
}

extern osTimerId_t rx_reset_timer;
extern comm_fsm_t uart_comm;
uint8_t lead_cnt = 0;
void state_wait_frame_start(comm_fsm_t *fsm, uint8_t b)
{
    if(0xFE == b) fsm->lead_cnt = fsm->lead_cnt + 1;
    if (FRAME_START_BYTE != b) return;
    if (fsm->lead_cnt != 4){
        fsm->lead_cnt = 0;
        return;
    }
    //printf("find header\r\n");
    fsm->state = state_rx_frame_header;
    fsm->pos = 0;
}

void rx_reset_timer_callback(osTimerId_t xTimer)
{
    comm_fsm_t *fsm = &uart_comm;
    GOTO_WAIT_FRAME_START("rx timeout\n");
    //printf("rx_reset_timer_callback");
}

typedef struct frame_node
{
    struct frame_node *next;
    simple_frame_t frame;
} frame_node_t;

static frame_node_t frames[FRAMES_CNT];
static frame_node_t *p_free_frames;
static osSemaphoreId_t frame_mutex = 0;

void comm_frame_enter_critical()
{
    if (IS_IN_INTERRUPT())
    {
        osSemaphoreAcquire(frame_mutex, 0);
    }
    else
        osSemaphoreAcquire(frame_mutex, osWaitForever);
}

void comm_frame_leave_critical()
{

        osSemaphoreRelease(frame_mutex);
}

void comm_frame_init(void)
{
    int i;
    if (frame_mutex == 0)
        frame_mutex = osSemaphoreNew(0, 1, NULL);

    p_free_frames = frames;
    for (i = 0; i < FRAMES_CNT - 1; i++)
        frames[i].next = frames + (i + 1);
    frames[FRAMES_CNT - 1].next = NULL;
}

void comm_frame_create(comm_fsm_t *fsm, void *user_data, f_rx_frame frame_cb)
{
    fsm->user_data = user_data;
    fsm->state = state_wait_frame_start;
    fsm->frame_cb = frame_cb;
}

simple_frame_t *comm_frame_alloc(int body_len)
{
    frame_node_t *r = NULL;
    comm_frame_enter_critical();
    {
        r = p_free_frames;
        if (r)
            p_free_frames = r->next;
    }
    comm_frame_leave_critical();
    return r ? &r->frame : NULL;
}

void comm_frame_rx_byte(comm_fsm_t *fsm, uint8_t b)
{
   // printf("%02x-",b);
    fsm->state(fsm, b);
}

void comm_frame_free(simple_frame_t *frame)
{
    #define OFFSET  ((int)&((frame_node_t *)NULL)->frame)
    comm_frame_enter_critical();
    {
        frame_node_t *r = (frame_node_t *)(((uint32_t)frame) - OFFSET);
        r->next = p_free_frames;
        p_free_frames = r;
    }
    comm_frame_leave_critical();
}

#pragma pack (push, 1)
typedef struct
{
    uint32_t prelude;
    uint8_t magic0;
    frame_header_t header;
    uint8_t magic1;
    uint8_t body[DATA_MAX_LEN + 2];
} full_frame_t;
#pragma pack (pop)

extern uint8_t get_cur_sta1(void);

static void send_frame(full_frame_t *frame)
{
    uint8_t cs = 0;
    int i;
    uint8_t *p = &frame->magic0;
    int len = frame->header.len;
    #ifdef LOG_FLASH_ENABLE
    uint32_t status;
    #endif
    
    for (i = 0; i < len + sizeof(frame->header) + 2; i++)
        cs += p[i];
    frame->body[len] = cs;
    frame->body[len + 1] = FRAME_END_BYTE;
    if (GLOBAL_PARAM->uart_append_tx_data)
        GLOBAL_PARAM->uart_append_tx_data(frame, sizeof(*frame) - sizeof(frame->body) + len + 2);
    else
    {
        #ifdef LOG_FLASH_ENABLE
        status = driver_append_tx_data(frame, sizeof(*frame) - sizeof(frame->body) + len + 2);
        if(status)
        {
            log_flash_write(LOG_FLASH_TLV_ERROR, 8, (uint8_t *)"BUF FULL");
        }
        #else
        if(driver_append_tx_data(frame, sizeof(*frame) - sizeof(frame->body) + len + 2))
        {
          platform_printf("tx full!");
        }
        #endif
    }

    #ifdef LOG_FLASH_ENABLE
    log_flash_write(LOG_FLASH_TLV_TX_EVENT, sizeof(*frame) - sizeof(frame->body) + len - 3, (uint8_t *)&(frame->header));
    #endif
}

full_frame_t full_frame =
{
    .prelude = 0xfefefefe,
    .magic0 = FRAME_START_BYTE,
    .header =
    {
        .reserved = 0
    },
    .magic1 = FRAME_START_BYTE,
};

void send_response_frame(uint8_t ctrl_code, uint32_t id, int len, const void *data)
{
    send_dev_response_frame(ctrl_code, id, len, data, NULL);
}

void send_dev_response_frame(uint8_t ctrl_code, uint32_t id, int len, const void *data, const uint8_t *addr)
{
    if (addr)
        memcpy(full_frame.header.m, addr, DEV_ADDR_LEN);
    else
        memset(full_frame.header.m, 0xff, DEV_ADDR_LEN);

    full_frame.header.len = len;
    full_frame.header.ctrl_code = ctrl_code;
    full_frame.header.cmd_code = id;
    memcpy(full_frame.body, data, len);
    send_frame(&full_frame);
}

void send_response_id_frame(uint8_t ctrl_code, uint32_t id, int len, const void *data)
{
    memset(full_frame.header.m, 0xff, DEV_ADDR_LEN);
    full_frame.header.len = len;
    full_frame.header.ctrl_code = ctrl_code;
    full_frame.header.cmd_code = id;
    memcpy(full_frame.body, data, len);
    send_frame(&full_frame);
}

void send_encapsulated_frame(uint8_t ctrl_code, const void *frame, int frame_len, const uint8_t *addr)
{
    int size = frame_len;
    uint8_t *p = full_frame.body;
    if (addr)
        memcpy(full_frame.header.m, addr, DEV_ADDR_LEN);
    else
        memset(full_frame.header.m, 0xff, DEV_ADDR_LEN);

    full_frame.header.ctrl_code = ctrl_code;
    full_frame.header.cmd_code = 0;

    #if (RELEASE_VERSION != RTK_VEISHENG_VERSION)
    memset(p, 0xfe, 4);
    p += 4;
    #endif
    
    memcpy(p, frame, size);
    p += size;

    full_frame.header.len = p - full_frame.body;

    send_frame(&full_frame);
}
