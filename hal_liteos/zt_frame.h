#ifndef zt_frame_h
#define zt_frame_h

#include <stdint.h>
#include "app_cfg.h"
#define DEV_ADDR_LEN            (6)


#pragma pack (push, 1)
typedef struct
{
    uint16_t len;
    uint8_t  ctrl_code;
    uint32_t cmd_code;
    uint8_t  m[DEV_ADDR_LEN];
    uint32_t reserved;
} frame_header_t;

typedef struct
{
    int8_t data[DATA_MAX_LEN];
} frame_body_t;

typedef struct
{
    frame_header_t header;
    uint8_t data[DATA_MAX_LEN];
} simple_frame_t;

#pragma pack (pop)

struct comm_fsm;

typedef void (* f_state)(struct comm_fsm *fsm, uint8_t b);
typedef void (* f_rx_frame)(void *user_data, simple_frame_t *frame);

typedef struct comm_fsm
{
    uint8_t cs;
    uint8_t lead_cnt;
    f_state state;
    int pos;
    void *user_data;
    f_rx_frame frame_cb;
    frame_header_t header;
    frame_body_t body;
} comm_fsm_t;

enum
{
    COMM_FSM_FRAME_ERR_NONE = 0,
    COMM_FSM_FRAME_ERR_HED_LEN_TOO_LONG = 0x70000001,
    COMM_FSM_FRAME_ERR_HED_68H_MIS = 0x70000002,
    COMM_FSM_FRAME_ERR_END_16H_MIS = 0x70000003,
    COMM_FSM_FRAME_ERR_CS = 0x70000004,
};

void comm_frame_init(void);

void comm_frame_create(comm_fsm_t *fsm, void *user_data, f_rx_frame frame_cb);
void comm_frame_rx_byte(comm_fsm_t *fsm, uint8_t b);

void comm_frame_free(simple_frame_t *frame);

void send_dev_response_frame(uint8_t ctrl_code, uint32_t id, int len, const void *data, const uint8_t *addr);
void send_response_frame(uint8_t ctrl_code, uint32_t id, int len, const void *data);
void send_response_id_frame(uint8_t ctrl_code, uint32_t id, int len, const void *data);
void send_encapsulated_frame(uint8_t ctrl_code, const void *frame, int frame_len, const uint8_t *addr);

#endif
