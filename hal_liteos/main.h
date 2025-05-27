

#ifndef __MAIN_H__
#define __MAIN_H__

#include <stdint.h>

#include "eflash.h"

#if defined __cplusplus
    extern "C" {
#endif

// release DEBUG=0 RELEASE_VER=1
#define DEBUG 1
#define RELEASE_VER 0

#ifndef DEBUG
    #warning DEBUG should be defined 0 or 1
    #define DEBUG 0
#endif

#ifndef RELEASE_VER
    #warning RELEASE_VER should be defined 0 or 1
    #define RELEASE_VER 1
#endif

#if (DEBUG == 0)
    #define dbg_printf(...)
    #define debug_out(...)
#else
    #define dbg_printf(...) platform_printf(__VA_ARGS__)
    #define debug_out(...) platform_printf(__VA_ARGS__)
#endif

#if (RELEASE_VER == 1)
    #define log_printf(...)
#else
    #define log_printf(...) platform_printf(__VA_ARGS__)
#endif

#if 0
    #define log_error(...)
#else
    #define log_error(...) platform_printf(__VA_ARGS__)
#endif


#define PRINT_PORT    APB_UART0
#define COMM_PORT     APB_UART1
#define COMM_ISR_ID   PLATFORM_CB_IRQ_UART1

#define LOG_FLASH_START_ADDRESS 0x44000
#define LOG_FLASH_END_ADDRESS 0x84000
#define LOG_FLASH_PAGE_SIZE EFLASH_PAGE_SIZE
#define LOG_FLASH_PAGE_NUM ((LOG_FLASH_END_ADDRESS - LOG_FLASH_START_ADDRESS) / LOG_FLASH_PAGE_SIZE)

void platform_wdg_reset(void);

#if defined __cplusplus
    }
#endif


#endif


