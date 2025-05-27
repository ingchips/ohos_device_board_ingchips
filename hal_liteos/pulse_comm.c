#include "zt_comm.h"

#include <stdlib.h>
#include <string.h>
#include "kv_storage.h"
#include "platform_api.h"
#include "btstack_util.h"
#include "cross_param.h"
#include "app_cfg.h"
#include "main.h"

extern int check_CMD_PD_param(const int len, const uint8_t *param);
static void handle_set_param(uint32_t id, const int len, const uint8_t *param)
{
    uint8_t dar = COMM_ERR_DATAGRAM_PARAM;
    settings_t *p = get_settings();
    switch (id)
    {
    case CMD_PD:
        {
            const test_mode_t *test = (const test_mode_t *)param;
            if (check_CMD_PD_param(len, param) != 0) {
                dar = COMM_ERR_UNDEF_CMD;
                send_response_frame(CTRL_CODE_RSP_SET_PARAM, id, 1, &dar);
                break;
            }
            dar = COMM_ERR_SUCC;
            GLOBAL_PARAM->pulse_type = test->pulse_type;
            if (test->pulse_type != 0xff)
            {
                GLOBAL_PARAM->interval = test->interval;
                GLOBAL_PARAM->pow_level = test->pow_level;

            #if (RELEASE_VERSION != RTK_VEISHENG_VERSION)
                GLOBAL_PARAM->chan_no = test->chan_no;
            #else
                GLOBAL_PARAM->chan_no = 5;            
            #endif

                memcpy(GLOBAL_PARAM->chan_freqs, test->chan_freqs, sizeof(test->chan_freqs));
                send_response_frame(CTRL_CODE_RSP_SET_PARAM, id, 1, &dar);
            }
            GLOBAL_PARAM->test_ctrl();
        }
        break;

    default:
        dar = COMM_ERR_UNDEF_CMD;
        send_response_frame(CTRL_CODE_RSP_SET_PARAM, id, 1, &dar);
        break;
    }
}

void handle_certi_app_frame(void *user_data, simple_frame_t *frame)
{
    switch (frame->header.ctrl_code & 0x7)
    {
    case CTRL_CODE_METER2MODULE:
        GLOBAL_PARAM->send_to_dev(frame->header.m, frame->data, frame->header.len);
        break;

    case CTRL_CODE_SET_PARAM:
        handle_set_param(frame->header.cmd_code, frame->header.len, frame->data);
        break;

    default:
        //platform_reset();
        break;
    }
    comm_frame_free(frame);
}

static void on_app_started(uint8_t err_code)
{
#if (AVOID_TENG_HE_BUG == 1)
    return;
#else
    uint8_t conn_info = 0;
#if(RELEASE_VERSION != RTK_VEISHENG_VERSION)
    send_response_id_frame(CTRL_CODE_PROACTIVE_REPORT, CMD_BLE_CONN_INFO, 1, &conn_info);
#endif
    return;
#endif
}

static void ble_write_callback(const uint8_t *buffer, uint16_t buffer_size)
{
#if(RELEASE_VERSION == RTK_VEISHENG_VERSION)
	uint8_t op = CTRL_CODE_MODULE2METER;
#else
	uint8_t op = CTRL_CODE_METER2MODULE;
#endif

    send_encapsulated_frame(op, buffer, buffer_size,
                            GLOBAL_PARAM->remote);
}

static void request_to_change_test_mode(uint8_t mode)
{
    // TODO: RTK module doesn't have this pro-active report message
}

comm_fsm_t certi_app_comm;

// TODO: test pins
const uint8_t test_pins[] = TEST_PINS;

void init_for_pulse_app(int init)
{
    if (init)
    {
        memset(GLOBAL_PARAM, 0, sizeof(*GLOBAL_PARAM));

        GLOBAL_PARAM->interval = 2000;
        GLOBAL_PARAM->uart1_rx = PIN_PRINT_RX;
        GLOBAL_PARAM->uart1_tx = PIN_PRINT_TX;
        GLOBAL_PARAM->uart0_rx = PIN_COMM_RX;
        GLOBAL_PARAM->uart0_tx = PIN_COMM_TX;

        GLOBAL_PARAM->uart_rx_byte = (f_on_uart_rx_byte)comm_frame_rx_byte;
        GLOBAL_PARAM->uart_rx_byte_param = &certi_app_comm;
        GLOBAL_PARAM->on_app_started = on_app_started;
        GLOBAL_PARAM->ble_write_callback = ble_write_callback;
        GLOBAL_PARAM->request_to_change_test_mode = request_to_change_test_mode;
        GLOBAL_PARAM->wdg_reset = platform_wdg_reset;
        memcpy(GLOBAL_PARAM->gpio_for_test, test_pins, sizeof(GLOBAL_PARAM->gpio_for_test));
    }

    comm_frame_create(&certi_app_comm, NULL, handle_certi_app_frame);
}
