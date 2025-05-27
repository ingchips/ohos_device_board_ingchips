#ifndef _PROFILESTASK_H_
#define _PROFILESTASK_H_

#include <stdint.h>
#include "bluetooth.h"
#include "zt_comm.h"

uint32_t setup_profile(void *data, void *user_data);
uint8_t scan_control(const settings_t *settings, uint8_t enable);
int i_am_slave_disconnect_master(bd_addr_t peer_addr);
void master_comm_time_out_stop(void);
void master_comm_time_out_start(uint16_t time_out);
void app_rx_frame(void *user_data, simple_frame_t *frame);
#endif


