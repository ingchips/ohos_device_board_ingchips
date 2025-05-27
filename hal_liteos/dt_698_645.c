#include <string.h>
#include <stdlib.h>
#include "dt_698_645.h"
#include "platform_api.h"
#include "app_cfg.h"
#include "main.h"

#define FRAME_START_BYTE (0x68)
#define FRAME_END_BYTE (0x16)
#define FRAME_LEADING_BYTE (0xFE)
#define FRAME_CAN_BE_698 (0x01)
#define FRAME_CANOT_BE_698 (~FRAME_CAN_BE_698)
#define FRAME_CAN_BE_645 (0x01 << 1)
#define FRAME_CANOT_BE_645 (~FRAME_CAN_BE_645)

static dt698_645_frame_t *frame_alloc(void);

static void state_wait_frame_start(dt698_645_fsm_t *fsm, const uint8_t *data, int data_len);

static void goto_wait_frame_start(dt698_645_fsm_t *fsm)
{
    fsm->state = state_wait_frame_start;
}

#define GOTO_WAIT_FRAME_START(fmt, ...) \
    do                                  \
    {                                   \
        PRINT(fmt, ##__VA_ARGS__);      \
        goto_wait_frame_start(fsm);     \
    } while (0)

static void frame_698_645_ok(dt698_645_fsm_t *fsm)
{
    dt698_645_frame_t *frame = NULL;

    dbg_printf("frame ok %u\r\n", fsm->frame.frame_len);
    frame = frame_alloc();
    if (NULL == frame) {
        platform_raise_assertion("dt_698_645.c", __LINE__);
    }

    memcpy(frame, &fsm->frame, fsm->frame.frame_len + sizeof(fsm->frame.frame_len));
    fsm->frame_cb(fsm->user_data, frame);

    goto_wait_frame_start(fsm);
    return;
}
#define DT698_MIN_HEADER_LEN 8
#define DT645_HEADER_LEN 10
static int procses_frame_645_698(dt698_645_fsm_t *fsm)
{

    int frame_formate_can_be_698 = 1;
    int frame_formate_can_be_645 = 1;
	uint32_t dt698_header_len = 0;
    dbg_printf("645_698 %d,%d,%d\r\n", fsm->data_len,fsm->frame.header.dt698_header.len + 2,fsm->frame.header.dt645_header.len + 12);
    if (fsm->data_len >= DT698_MIN_HEADER_LEN) {
        if (fsm->data_len == (fsm->frame.header.dt698_header.len + 2)) {
            if (fsm->frame.data[fsm->frame.header.dt698_header.len - 9] == 0x16 && 
				dt698_calc_fcs(&fsm->frame)) {
                fsm->frame.frame_len = fsm->frame.header.dt698_header.len + 2;
				dbg_printf("f698\r\n");
                frame_698_645_ok(fsm);
                return 1;
            } else {
                frame_formate_can_be_698 = 0;
            }
        } else {
        }
    }

    if (fsm->data_len >= DT645_HEADER_LEN) {
        if (fsm->data_len == (fsm->frame.header.dt645_header.len + 12)) {
            if ((fsm->frame.header.dt645_header.magic1 == FRAME_START_BYTE) &&
                (fsm->frame.data[fsm->frame.header.dt645_header.len + 1] == 0x16) && 
                dt645_fcs_IsOk(&fsm->frame)) {
                fsm->frame.frame_len = fsm->frame.header.dt645_header.len + 12;
				dbg_printf("f645\r\n");
                frame_698_645_ok(fsm);
                return 1;
            } else {
                frame_formate_can_be_645 = 0;
            }
        } else {
        }
    }
	
    if (fsm->data_len >= DT698_MIN_HEADER_LEN) {//
        if (fsm->data_len >= (fsm->frame.header.dt698_header.len + 2)) {
            if (fsm->frame.data[fsm->frame.header.dt698_header.len - 9] == 0x16 && 
				dt698_calc_fcs(&fsm->frame)) {
                fsm->frame.frame_len = fsm->frame.header.dt698_header.len + 2;
				dbg_printf("here 3\r\n");
                frame_698_645_ok(fsm);
                return 1;
            } else {
                frame_formate_can_be_698 = 0;
            }
        } else {
        	if (dt698_calc_hcs(&fsm->frame) != 1)
				frame_formate_can_be_698 = 0;
        }
    }

    if (fsm->data_len >= DT645_HEADER_LEN) {
        if (fsm->data_len >= (fsm->frame.header.dt645_header.len + 12)) {
            if ((fsm->frame.header.dt645_header.magic1 == FRAME_START_BYTE) &&
                (fsm->frame.data[fsm->frame.header.dt645_header.len + 1] == 0x16) &&
				dt645_fcs_IsOk(&fsm->frame)) {
                fsm->frame.frame_len = fsm->frame.header.dt645_header.len + 12;
				dbg_printf("here 4\r\n");
				frame_698_645_ok(fsm);
                return 1;
            } else {
                frame_formate_can_be_645 = 0;
            }
        } else {
        }
    }

	dbg_printf("698:%d,645:%d\r\n",frame_formate_can_be_698,frame_formate_can_be_645);
    if ((frame_formate_can_be_645 == 0) &&
        (frame_formate_can_be_698 == 0)) {
        return 1;
    }

    if ((fsm->data_len >= DT698_MAX_LEN + 8 ) || (fsm->frame.header.dt698_header.len > DT698_MAX_LEN && fsm->frame.header.dt645_header.len > DT645_MAX_LEN )) {
        return 1;
    }

    // return 0 wait_frame_end
    return 0;
}

#define HEAD_645_FROM_68_TO_LEN 10
static void state_wait_frame_end(dt698_645_fsm_t *fsm, const uint8_t *data, int data_len)
{
    uint8_t *p_data = (uint8_t *)&(fsm->frame.header);

    dbg_printf("end\r\n");
    if ((data_len + fsm->data_len) > (DT698_MAX_LEN + 8)) {
        data_len = DT698_MAX_LEN + 8 - fsm->data_len;
    }

    p_data += fsm->data_len;
    memcpy(p_data, data, data_len);
    fsm->data_len += data_len;

    if (procses_frame_645_698(fsm) == 1) {
        goto_wait_frame_start(fsm);
    }
}

static void state_wait_frame_start(dt698_645_fsm_t *fsm, const uint8_t *data, int data_len)
{
    int i;

    dbg_printf("frame_start,%d\r\n",data_len);
    for (i = 0; i < data_len; i++) {
        if (FRAME_START_BYTE == data[i]) {
            fsm->data_len = (data_len - i);
            if (fsm->data_len > DT698_MAX_LEN + 8) {
                fsm->data_len = DT698_MAX_LEN + 8;
            }
            memcpy(&(fsm->frame.header), &data[i], fsm->data_len);
            if (procses_frame_645_698(fsm) == 0) {
                fsm->state = state_wait_frame_end;
            }
            break;
        }
    }
}


typedef struct frame_node
{
    struct frame_node *next;
    dt698_645_frame_t frame;
} frame_node_t;

static frame_node_t frames[DT_698_645_FRAMES_CNT];
static frame_node_t *p_free_frames;

void dt698_645_frame_init(void)
{
    int i;
    p_free_frames = frames;
    for (i = 0; i < DT_698_645_FRAMES_CNT - 1; i++)
        frames[i].next = frames + (i + 1);
    frames[DT_698_645_FRAMES_CNT - 1].next = NULL;
}

static dt698_645_frame_t *frame_alloc(void)
{
    frame_node_t *r = p_free_frames;
    if (r) {
        p_free_frames = r->next;
        r->frame.header.dt698_header.magic = FRAME_START_BYTE;
        return &r->frame;
    }
    platform_raise_assertion("dt_698_645.c", __LINE__);
    return NULL;
}

void dt698_645_frame_free(dt698_645_frame_t *frame)
{
#define OFFSET ((int)&((frame_node_t *)NULL)->frame)
    frame_node_t *r = (frame_node_t *)(((uint32_t)frame) - OFFSET);
    r->next = p_free_frames;
    p_free_frames = r;
}

void dt698_645_frame_create(dt698_645_fsm_t *fsm, void *user_data, f_rx_dt698_645_frame frame_cb)
{
    fsm->user_data = user_data;
    fsm->frame_cb = frame_cb;
    fsm->frame.header.dt698_header.magic = FRAME_START_BYTE;
    goto_wait_frame_start(fsm);
}

void dt698_645_frame_rx_byte(dt698_645_fsm_t *fsm, const uint8_t *data, int data_len)
{
    fsm->state(fsm, data, data_len);
}

void common_frame_rx_byte(dt698_645_fsm_t *fsm, const uint8_t *data, int data_len)
{
    if (data_len >= DT698_MAX_LEN + 8) {
        fsm->data_len = DT698_MAX_LEN + 8;
    } else {
        fsm->data_len = data_len;
    }
    memcpy(&(fsm->frame.header), data, fsm->data_len);
    fsm->frame.frame_len = fsm->data_len;
    frame_698_645_ok(fsm);
    return;
}

typedef uint16_t u16;

/*
* FCS lookup table as calculated by the table generator.
*/
static u16 fcstab[256] = {
    0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
    0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
    0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
    0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
    0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
    0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
    0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
    0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
    0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
    0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
    0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
    0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
    0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
    0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
    0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
    0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
    0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
    0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
    0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
    0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
    0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
    0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
    0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
    0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
    0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
    0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
    0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
    0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
    0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
    0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
    0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
    0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78};

#define PPPINITFCS16 0xffff /* Initial FCS value */
#define PPPGOODFCS16 0xf0b8 /* Good final FCS value */

/*
* Calculate a new fcs given the current fcs and the new data.
*/
u16 pppfcs16(u16 fcs, const uint8_t *cp, int len)
{
    while (len--)
        fcs = (fcs >> 8) ^ fcstab[(fcs ^ *cp++) & 0xff];
    return (fcs);
}


uint8_t dt645_fcs_IsOk(const dt698_645_frame_t *frame)
{
	uint8_t sum = 0;
	for (uint16_t i = 0; i < 10; i++)
		sum += *((uint8_t *)&frame->header.dt645_header+i);
	
	for (uint16_t i = 0; i < frame->header.dt645_header.len; i++)
		sum += frame->data[i];
	dbg_printf("sum=%02x,%02x\r\n",sum,frame->data[frame->header.dt645_header.len]);
	return sum == frame->data[frame->header.dt645_header.len];
}

uint8_t dt698_645_calc_fcs(const dt698_645_frame_t *frame)
{
	dbg_printf("\r\n");

	for (uint8_t i = 0; i < 15; i++)
		dbg_printf("%02x-",frame->header.dt698_header.data[i]);
	dbg_printf("\r\n");
	uint8_t header_len =  (frame->header.dt698_header.data[1]&0x0f) + 1;
	dbg_printf("header_len =%d(0x%02x)\r\n",header_len,frame->header.dt698_header.data[1]);
    u16 trialfcs = pppfcs16(PPPINITFCS16, ((const uint8_t *)&frame->header.dt698_header)+1, 5 + header_len);
	uint16_t cs = trialfcs ^ 0xffff;
	dbg_printf("%04x\r\n",cs);
	uint16_t  cs2 = *(uint16_t *)(((const uint8_t *)&frame->header.dt698_header) + 6 + header_len);
	 dbg_printf("%04x,%04x\r\n",cs,cs2);
    return cs==cs2;           
}

uint8_t dt698_calc_fcs(const dt698_645_frame_t *frame)
{
	dbg_printf("fcs\r\n");
	u16 trialfcs = pppfcs16(PPPINITFCS16, (const uint8_t *)&frame->header.dt698_header.len, frame->header.dt698_header.len - 2);
	uint16_t cs = trialfcs ^ 0xffff;            	
	uint16_t  cs2 = *(uint16_t *)((const uint8_t *)&frame->header.dt698_header.data +  frame->header.dt698_header.len - 4);//no include head&tail&length
	dbg_printf("%04x,%04x\r\n",cs,cs2);	
    return cs==cs2;            /* complement */
}

uint8_t dt698_calc_hcs(const dt698_645_frame_t *frame)
{
	dbg_printf("hcs\r\n");

	for (uint8_t i = 0; i < 15; i++)
		dbg_printf("%02x-",frame->header.dt698_header.data[i]);
	dbg_printf("\r\n");
	uint8_t header_len =  (frame->header.dt698_header.data[1]&0x0f) + 1;
	dbg_printf("header_len =%d(0x%02x)\r\n",header_len,frame->header.dt698_header.data[1]);
    u16 trialfcs = pppfcs16(PPPINITFCS16, ((const uint8_t *)&frame->header.dt698_header)+1, 5 + header_len);
	uint16_t cs = trialfcs ^ 0xffff;
	dbg_printf("%04x\r\n",cs);
	uint16_t  cs2 = *(uint16_t *)(((const uint8_t *)&frame->header.dt698_header) + 6 + header_len);
	 dbg_printf("%04x,%04x\r\n",cs,cs2);
    return cs==cs2;            /* complement */
}

