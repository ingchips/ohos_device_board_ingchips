/*
* Copyright (C) INGCHIPS. All rights reserved.
* This code is INGCHIPS proprietary and confidential.
* Any use of the code for whatever purpose is subject to
* specific written permission of INGCHIPS.
*/

#include <ingsoc.h>
#include <stdio.h>
#include <string.h>

#include "bluetooth.h"
#include "att_db_util.h"
#include "ota_service.h"
#include "platform_api.h"
#include "rom_tools.h"
#include "zt_frame.h"
#include "zt_comm.h"

#if (INGCHIPS_FAMILY == INGCHIPS_FAMILY_918)

#ifndef SEC_FOTA_APP_ADDR
#define SEC_FOTA_APP_ADDR 0x44000
#endif

#define PAGE_SIZE (8192)

#elif (INGCHIPS_FAMILY == INGCHIPS_FAMILY_916)

#ifndef SEC_FOTA_APP_ADDR
#define SEC_FOTA_APP_ADDR 0x2040000

#else

#error unknown or unsupported chip family

#endif

#define PAGE_SIZE (EFLASH_SECTOR_SIZE)

#endif

#define OTA_STAR_ADD    0x50000

#define DEF_UUID(var, ID)  static const uint8_t var[] = ID;

//DEF_UUID(uuid_ota_service,  INGCHIPS_UUID_OTA_SERVICE);
//DEF_UUID(uuid_ota_ver,      INGCHIPS_UUID_OTA_VER);
//DEF_UUID(uuid_ota_data,     INGCHIPS_UUID_OTA_DATA);
//DEF_UUID(uuid_ota_ctrl,     INGCHIPS_UUID_OTA_CTRL);

uint16_t att_ota_ver_handle = 0;
uint16_t att_ota_data_handle = 0;
uint16_t att_ota_ctrl_handle = 0;

#define ATT_OTA_HANDLE_VER          att_ota_ver_handle
#define ATT_OTA_HANDLE_DATA         att_ota_data_handle
#define ATT_OTA_HANDLE_CTRL         att_ota_ctrl_handle

static uint8_t  ota_ctrl[] = {OTA_STATUS_DISABLED};
static uint8_t  ota_downloading = 0;
static uint32_t ota_start_addr = 0;
static uint32_t ota_page_offset = 0;
static uint8_t  page_buffer[PAGE_SIZE];
uint8_t ota_enable = 0;
void ota_init_service()
{
//    uint8_t ota_data_buff[20];

//    att_db_util_add_service_uuid128(uuid_ota_service);
//    att_ota_ver_handle = att_db_util_add_characteristic_uuid128(uuid_ota_ver,
//        ATT_PROPERTY_READ | ATT_PROPERTY_DYNAMIC, NULL, 0);
//    att_ota_data_handle = att_db_util_add_characteristic_uuid128(uuid_ota_data,
//        ATT_PROPERTY_WRITE_WITHOUT_RESPONSE | ATT_PROPERTY_DYNAMIC, ota_data_buff, sizeof(ota_data_buff));
//    att_ota_ctrl_handle = att_db_util_add_characteristic_uuid128(uuid_ota_ctrl,
//        ATT_PROPERTY_READ | ATT_PROPERTY_WRITE_WITHOUT_RESPONSE | ATT_PROPERTY_DYNAMIC, ota_ctrl, sizeof(ota_ctrl));

    // printf("att_ota_ver_handle   = %d\n"
    //        "att_ota_data_handle  = %d\n"
    //        "att_ota_ctrl_handle  = %d\n", att_ota_ver_handle, att_ota_data_handle, att_ota_ctrl_handle);
}

void ota_init_handles(const uint16_t handle_ver, const uint16_t handle_ctrl, const uint16_t handle_data)
{
    att_ota_ver_handle = handle_ver;
    att_ota_data_handle = handle_data;
    att_ota_ctrl_handle = handle_ctrl;
}
static uint8_t page_time = 0;

int uart_ota_write_and_resp(const uint8_t *data, int len)
{	
	uint16_t handle = *(uint16_t *)data;
	uint16_t mode = *(uint16_t *)(data+2);
	uint16_t offset = *(uint16_t *)(data+4);
	if (handle == ATT_OTA_HANDLE_VER) {
		printf("get version");
		ota_ver_t this_version;
		ota_read_callback(handle,offset,(uint8_t *)&this_version,sizeof(ota_ver_t));
		send_response_frame(CTRL_CODE_RSP_GET_PARAM,CMD_UART_OTA,sizeof(ota_ver_t),&this_version);
	}else {
		ota_write_callback(handle, mode, offset, data+6, len - 6);
		send_response_frame(CTRL_CODE_RSP_GET_PARAM, CMD_UART_OTA,1, &ota_ctrl[0]);
	}
	return 0;

}

int ota_write_callback(uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, const uint8_t *buffer, uint16_t buffer_size)
{
    if (transaction_mode != ATT_TRANSACTION_MODE_NONE)
    {
        printf("transaction_mode: %d\n", transaction_mode);
        return 0;
    }

    
    #if 0
    if(ota_enable == 0)
        {
         ota_ctrl[0] = OTA_STATUS_ERROR;
         printf("ota_disable\r\n");
         return 0;
        }
        
    #endif
    
    if (att_handle == ATT_OTA_HANDLE_CTRL)
    {
        if (OTA_CTRL_START == buffer[0])
        {
            ota_ctrl[0] = OTA_STATUS_OK;
            ota_start_addr = 0;
            ota_downloading = 0;
            return 0;
        }

        switch (buffer[0])
        {
        case OTA_CTRL_PAGE_BEGIN:			
            ota_start_addr = *(uint32_t *)(buffer + 1);
            if (ota_start_addr & 0x3)
            {
                ota_ctrl[0] = OTA_STATUS_ERROR;
                return 0;
            }
            else
                ota_ctrl[0] = OTA_STATUS_OK;
            ota_downloading = 1;
            ota_page_offset = 0;
            break;
        case OTA_CTRL_PAGE_END:
            if (OTA_STATUS_OK != ota_ctrl[0])
                break;
            program_flash(ota_start_addr, page_buffer, ota_page_offset);

            ota_downloading = 0;
            {
                uint16_t len = *(uint16_t *)(buffer + 1);
                uint16_t crc_value = *(uint16_t *)(buffer + 3);
                if (ota_page_offset < len)
                {
                    ota_ctrl[0] = OTA_STATUS_WAIT_DATA;
                    break;
                }

                if (crc((uint8_t *)ota_start_addr, len) != crc_value)
                    ota_ctrl[0] = OTA_STATUS_ERROR;
                else
                    ota_ctrl[0] = OTA_STATUS_OK;
            }
            break;
        case OTA_CTRL_READ_PAGE:
            if (ota_downloading)
                ota_ctrl[0] = OTA_STATUS_ERROR;
            else
            {
                ota_start_addr = *(uint32_t *)(buffer + 1);
                ota_ctrl[0] = OTA_STATUS_OK;
            }
            break;
        case OTA_CTRL_SWITCH_APP:
             platform_switch_app(SEC_FOTA_APP_ADDR);
            break;
        case OTA_CTRL_METADATA:
            if (OTA_STATUS_OK != ota_ctrl[0])
                break;
            if ((0 == ota_downloading) || (buffer_size < 1 + sizeof(ota_meta_t)))
            {
                const ota_meta_t  *meta = (const ota_meta_t *)(buffer + 1);
                int s = buffer_size - 1;
                if (crc((uint8_t *)&meta->entry, s - sizeof(meta->crc_value)) != meta->crc_value)
                {
                    ota_ctrl[0] = OTA_STATUS_ERROR;
                    break;
                }
#if (INGCHIPS_FAMILY == INGCHIPS_FAMILY_918)
				int retValue = program_fota_metadata(meta->entry,
                                      (s - sizeof(ota_meta_t)) / sizeof(meta->blocks[0]),
                                      meta->blocks);
					printf("meta data ret = %d\r\n", retValue);
#elif (INGCHIPS_FAMILY == INGCHIPS_FAMILY_916)
                flash_do_update((s - sizeof(ota_meta_t)) / sizeof(meta->blocks[0]),
                                meta->blocks,
                                page_buffer);
#endif
				if (retValue != 0) ota_ctrl[0] = OTA_STATUS_ERROR;
					
            }
            else
            {
                ota_ctrl[0] = OTA_STATUS_ERROR;
            }
            break;
        case OTA_CTRL_REBOOT:
            if (OTA_STATUS_OK == ota_ctrl[0])
            {
                if (ota_downloading)
                    ota_ctrl[0] = OTA_STATUS_ERROR;
                else
                    platform_reset();
            }
            break;
        default:
            ota_ctrl[0] = OTA_STATUS_ERROR;
        }
    }
    else if (att_handle == ATT_OTA_HANDLE_DATA)
    {
        if (OTA_STATUS_OK == ota_ctrl[0])
        {
            if (   (buffer_size & 0x3) || (0 == ota_downloading)
                || (ota_page_offset + buffer_size > PAGE_SIZE))
            {
                ota_ctrl[0] = OTA_STATUS_ERROR;
                return 0;
            }

            memcpy(page_buffer + ota_page_offset,
                   buffer, buffer_size);
            ota_page_offset += buffer_size;
        }
    }
    else;

    return 0;
}

int ota_read_callback(uint16_t att_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size)
{
    extern prog_ver_t app_ver;
    if (buffer == NULL)
    {
        if (att_handle == ATT_OTA_HANDLE_CTRL)
            return 1;
        else if (att_handle == ATT_OTA_HANDLE_VER)
            return sizeof(ota_ver_t);
        else
            return 0;
    }
	
    if (att_handle == ATT_OTA_HANDLE_CTRL)
    {
        buffer[0] = ota_ctrl[0];
        printf("read status=%x\r\n",buffer[0]);
    }
    else if (att_handle == ATT_OTA_HANDLE_VER )
    {
        ota_ver_t *this_version = (ota_ver_t *)buffer;
        const platform_ver_t * v = platform_get_version();

        this_version->platform.major = v->major;
        this_version->platform.minor = v->minor;
        this_version->platform.patch = v->patch;		
        printf("read v:%d-%d-%d\r\n",v->major,v->minor,v->patch);        
        this_version->app = app_ver;
        printf("read v:%d-%d-%d\r\n",app_ver.major,app_ver.minor,app_ver.patch);        
        
    }

    return buffer_size;
}
