#ifndef dt_698_645_h
#define dt_698_645_h

#include <stdint.h>
#include "app_cfg.h"

#define DT645_ADDR_LEN               (6)

#pragma pack (push, 1)

typedef struct
{
    uint8_t magic0;
    uint8_t addr[DT645_ADDR_LEN];
    uint8_t magic1;
    uint8_t ctrl_code;
    uint8_t len;
    //uint8_t data[DT645_MAX_LEN + 2];   //2 means: CS and endflag
} dt645_frame_t;

typedef struct
{
    uint8_t magic;
    uint16_t len;
    uint8_t data[7];
} dt698_frame_t;

typedef union
{
    dt645_frame_t dt645_header;
    dt698_frame_t dt698_header;
} dt698_645_frame_header;

typedef struct
{
    int frame_len;
    dt698_645_frame_header header;
    uint8_t data[DT698_MAX_LEN - 2];
} dt698_645_frame_t;

#pragma pack (pop)

struct dt698_645_fsm;

typedef void (* f_dt698_645_state)(struct dt698_645_fsm *fsm, const uint8_t *data, int data_len);
typedef void (* f_rx_dt698_645_frame)(void *user_data, dt698_645_frame_t *frame);

typedef struct dt698_645_fsm
{
    f_dt698_645_state state;
    void *user_data;
    f_rx_dt698_645_frame frame_cb;
    dt698_645_frame_t frame;
    int data_len;
} dt698_645_fsm_t;

void dt698_645_frame_init(void);

void dt698_645_frame_create(dt698_645_fsm_t *fsm, void *user_data, f_rx_dt698_645_frame frame_cb);
void dt698_645_frame_rx_byte(dt698_645_fsm_t *fsm, const uint8_t *data, int data_len);
void common_frame_rx_byte(dt698_645_fsm_t *fsm, const uint8_t *data, int data_len);

void dt698_645_frame_free(dt698_645_frame_t *frame);

uint8_t dt698_calc_fcs(const dt698_645_frame_t *frame);
uint8_t dt698_calc_hcs(const dt698_645_frame_t *frame);
uint8_t dt645_fcs_IsOk(const dt698_645_frame_t *frame);

#endif
