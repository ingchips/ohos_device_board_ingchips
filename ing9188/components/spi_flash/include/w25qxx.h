/*
 * Copyright (c) 2022 Talkweb Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __BSP_SPIFLASH_H__
#define __BSP_SPIFLASH_H__

#ifdef LOSCFG_DRIVERS_HDF_PLATFORM_SPI
#include "hdf_log.h"
#include "spi_if.h"

#define W25x_ID 0X4018 // W25Q128  16MB

uint8_t W25x_InitSpiFlash(uint32_t busNum, uint32_t csNum);
uint8_t W25x_DeInitSpiFlash(void);
DevHandle W25x_GetSpiHandle(void);
void W25x_SectorErase(uint32_t SectorAddr);
void W25x_BulkErase(void);
void W25x_PageWrite(uint8_t *pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite);
void W25x_BufferWrite(uint8_t *pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite);
void W25x_BufferRead(uint8_t *pBuffer, uint32_t ReadAddr, uint16_t NumByteToRead);
uint32_t W25x_ReadID(void);
uint32_t W25x_ReadDeviceID(void);
void W25x_StartReadSequence(uint32_t ReadAddr);
void W25x_PowerDown(void);
void W25x_WAKEUP(void);

uint8_t W25x_ReadByte(void);
uint8_t W25x_SendByte(uint8_t byte);
void W25x_WriteEnable(void);
void W25x_WaitForWriteEnd(void);

uint8_t write_extend_flash(uint32_t address, uint8_t *data, uint32_t size);
uint8_t read_extend_flash(uint32_t address, uint8_t *data, uint32_t size);
uint8_t erase_chip(void);

void get_jedce_id(void);
void get_device_id(void);


#endif

#endif /* __BSP_SPIFLASH_H__ */
