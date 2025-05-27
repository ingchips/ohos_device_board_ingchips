
/*
** COPYRIGHT (c) 2022 by INGCHIPS
*/

#ifndef __LOG_FLASH_H__
#define __LOG_FLASH_H__

#include <stdint.h>

#if defined __cplusplus
    extern "C" {
#endif

#pragma pack (push, 1)

typedef enum 
{
    // 0xA0-0xAF !!! MUST LESS 15
    LOG_FLASH_TLV_INFO = 0xA1,       // string
    LOG_FLASH_TLV_WARN,              // string
    LOG_FLASH_TLV_ERROR,             // string
    LOG_FLASH_TLV_RX_EVENT,          // hex
    LOG_FLASH_TLV_TX_EVENT,          // hex
    LOG_FLASH_TLV_CONNECT_EVENT,     // hex
    LOG_FLASH_TLV_DISCCONNECT_EVENT, // hex
}log_flash_tlv_type;

typedef struct
{
    uint8_t type;
    uint8_t length;
    uint8_t rtc[3];
    uint8_t data[0];
}log_flash_tlv_t;

void log_flash_write(uint8_t type, uint8_t length, const uint8_t *data);
void log_flash_enable(int en);
void log_uart_irq_init(void);

#pragma pack (pop)

#if defined __cplusplus
    }
#endif

#endif

