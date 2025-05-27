#ifndef _cross_param_h
#define _cross_param_h

#include <stdint.h>

typedef uint8_t addr_t[6];

typedef void (*f_on_uart_rx_byte)(void * param, uint8_t c);
typedef void (*f_on_app_started)(uint8_t err_code);
typedef void (*f_ble_write_callback)(const uint8_t *buffer, uint16_t buffer_size);
typedef void (*f_request_to_change_test_mode)(uint8_t mode);

typedef uint32_t (* f_uart_append_tx_data)(const void *data, int len);
typedef void (*f_test_ctrl)(void);
typedef void (*f_send_to_dev)(const uint8_t *addr, const uint8_t *data, int len);
typedef void (*f_wdg_reset)(void);

typedef struct
{
    // written by main app
    addr_t remote;
    addr_t local;
    addr_t comm_addr;
    addr_t targets[12];
    uint8_t meter_no;
    uint8_t pulse_type;
    uint8_t pow_level;
    uint8_t band_sel;
    uint16_t interval;       // tx interval in us, default 2000us
    uint8_t uart0_tx, uart0_rx, uart1_tx, uart1_rx;
    uint8_t uart_parity;    // (SPS << 2) | (EPS << 1) | PEN
    uint8_t uart_two_stop_bits; // 1 for two stop bits
    uint32_t uart_baud;
    uint8_t chan_gen_type;
    uint8_t chan_no;
    uint16_t chan_freqs[5];
    uint8_t gpio_for_test[8];
    void *uart_rx_byte_param;
    f_on_app_started on_app_started;
    f_on_uart_rx_byte uart_rx_byte;
    f_ble_write_callback ble_write_callback;
    f_request_to_change_test_mode request_to_change_test_mode;

    // written by certification app
    f_uart_append_tx_data uart_append_tx_data;
    f_test_ctrl test_ctrl;
    f_send_to_dev send_to_dev;
    f_wdg_reset wdg_reset;
} global_param_t;


#ifdef TEST_MODE
    extern global_param_t g_para;
    #define GLOBAL_PARAM (&g_para)
#else
    #define GLOBAL_PARAM    ((global_param_t *)0x2000fa00)
#endif

#endif
