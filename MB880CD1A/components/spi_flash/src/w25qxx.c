#include <stdio.h>
#include <string.h>
#include "profile.h"
#include "ingsoc.h"
#include "platform_api.h"

/*###################**************** spi flash ****************###################*/
#define SPI_CH SPI_PORT_0
#define SPI_SSP AHB_SSP0
#define SSP_Ptr SPI_SSP

#define SPI_MIC_CLK GIO_GPIO_9
//#define SPI_MIC_CS GIO_GPIO_5 // 这里没使用，因为传输的时候每个字节都会控制一次CS，改为在代码中控制
#define SPI_MIC_MOSI GIO_GPIO_10
#define SPI_MIC_MISO GIO_GPIO_8

#define SPI_PIN_CS GIO_GPIO_7 // 使用这个控制CS


#define CS_HIGH() GIO_WriteValue(SPI_PIN_CS, 1)
#define CS_LOW() GIO_WriteValue(SPI_PIN_CS, 0)

#define FLASH_BYTE ((4) * 1024 * 1024) // 4MB
#define SECTOR_BYTE ((4) * 1024)       // 4kb
#define BLOCK_BYTE ((64) * 1024)       // 64kb
#define PAGE_BYTE ((1) * 256)          // 256byte

#define PAGE_SIZE (FLASH_BYTE / PAGE_BYTE) //((1) * 1024) //1024*256 = 4mb
#define SECTOR_SIZE (FLASH_BYTE / SECTOR_BYTE)
#define BLOCK_SIZE (FLASH_BYTE / BLOCK_BYTE)

// 写操作控制
#define WRITE_ENABLE 0x06
#define WRITE_DISABLE 0x04

// 数据读取
#define READ_DATA 0x03

#define READ_DEVICE_ID 0x90
#define READ_JEDEC_ID 0x9F

// 数据写入
#define PAGE_PROGRAM 0x02

// 擦除操作
#define SECTOR_ERASE_4KB 0x20
#define BLOCK_ERASE_32KB 0x52
#define BLOCK_ERASE_64KB 0xD8
#define CHIP_ERASE 0xC7
#define ERASE_PROGRAM_RESUME 0x7A
#define ERASE_PROGRAM_SUSPEND 0x75

// 状态寄存器操作
#define READ_STATUS_REGISTER_1 0x05
#define WRITE_STATUS_REGISTER_1 0x01
#define READ_STATUS_REGISTER_2 0x35
#define WRITE_STATUS_REGISTER_2 0x31

// 特殊操作
#define POWER_DOWN 0xB9
#define RELEASE_POWER_DOWN_READ_DEVICE_ID 0xAB

#define FLASH_OK 0
#define FLASH_ERR 1
#define FLASH_ERR_PARAM 2


static void setup_peripherals_spi_pin(void)
{

     SYSCTRL_ClearClkGateMulti(
        (1 << SYSCTRL_ClkGate_AHB_SPI0) 
        | (1 << SYSCTRL_ClkGate_APB_PinCtrl) 
        | (1 << SYSCTRL_ClkGate_APB_GPIO0));

    PINCTRL_SetPadMux(SPI_MIC_CLK, IO_SOURCE_SPI0_CLK);
    //PINCTRL_SetPadMux(SPI_MIC_CS, IO_SOURCE_SPI0_SSN);  //这里默认的CS配置不知道能不能去除，还没来得及验证
    PINCTRL_SetPadMux(SPI_MIC_MOSI, IO_SOURCE_SPI0_DO);
    PINCTRL_SelSpiDiIn(SPI_PORT_0, SPI_MIC_MISO);

    //config_uart(OSC_CLK_FREQ, 115200);
    PINCTRL_SetPadMux(SPI_PIN_CS, IO_SOURCE_GENERAL);
    GIO_SetDirection(SPI_PIN_CS, GIO_DIR_OUTPUT);
    GIO_WriteValue(SPI_PIN_CS, 1);
}

void setup_peripherals_init(void)
{
    // setup clock
    setup_peripherals_spi_pin();
    SYSCTRL_ResetBlock(SYSCTRL_Reset_AHB_SPI0);
    SYSCTRL_ReleaseBlock(SYSCTRL_Reset_AHB_SPI0);

    apSSP_sDeviceControlBlock param;
    apSSP_Initialize(SSP_Ptr);

    /* Set Device Parameters */
    param.ClockRate = 5;     // sspclkout = sspclk/(ClockPrescale*(ClockRate+1))  //这里原来是11，对应2M的时钟，我改成5，对应4M时钟
    param.ClockPrescale = 2; // Must be an even number from 2 to 254
    param.eSCLKPhase = apSSP_SCLKPHASE_LEADINGEDGE;
    param.eSCLKPolarity = apSSP_SCLKPOLARITY_IDLELOW;
    param.eFrameFormat = apSSP_FRAMEFORMAT_MOTOROLASPI;
    param.eDataSize = apSSP_DATASIZE_8BITS;
    param.eLoopBackMode = apSSP_LOOPBACKOFF;
    param.eMasterSlaveMode = apSSP_MASTER;
    param.eSlaveOutput = apSSP_SLAVEOUTPUTDISABLED;
    apSSP_DeviceParametersSet(SSP_Ptr, &param);
    apSSP_DeviceEnable(AHB_SSP0);
}

static uint8_t xchg_spi(uint8_t dat)
{
    apSSP_WriteFIFO(AHB_SSP0, dat);
    while (apSSP_DeviceBusyGet(AHB_SSP0))
        ;
    return (uint8_t)apSSP_ReadFIFO(AHB_SSP0);
}

uint8_t write_cmd(uint8_t cmd)
{
    uint8_t status = FLASH_OK;
    CS_LOW();
    status = xchg_spi(cmd);
    CS_HIGH();
    return status == FLASH_OK;
}

void read_register(uint8_t cmd, uint8_t *regdata, uint8_t size)
{
    CS_LOW();
    xchg_spi(cmd);
    for (uint8_t i = 0; i < size; i++)
    {
        regdata[i] = xchg_spi(0x00);
    }
    CS_HIGH();
}

static void write_enable()
{
    write_cmd(WRITE_ENABLE);
    // Wait
    uint8_t reg1[1], is_enable = 0;
    while (!is_enable)
    {
        read_register(READ_STATUS_REGISTER_1, reg1, 1);
        is_enable = ((reg1[0]) & 0x02);
        //vTaskDelay(pdMS_TO_TICKS(1));
    }
}

static void wait_flash()
{

    uint8_t is_busy = 1;
    while (is_busy)
    {
        CS_LOW();
        xchg_spi(READ_STATUS_REGISTER_1);
        is_busy = (0x01 & xchg_spi(0x00));
        CS_HIGH();
        //vTaskDelay(pdMS_TO_TICKS(100));
        // printf("waiting flash\n");
    }
    //printf("flash ok\n");
}

uint8_t write_register(uint8_t cmd, uint8_t *regdata, uint8_t size)
{

    uint8_t status = FLASH_OK;
    wait_flash();
    CS_LOW();
    xchg_spi(WRITE_ENABLE);
    for (uint8_t i = 0; i < size; i++)
    {
        status = xchg_spi(regdata[i]);
    }
    CS_HIGH();
    return status;
}

void write_addr(uint32_t cmd, uint32_t address)
{

    xchg_spi(cmd);
    xchg_spi((address >> 16) & 0xff); //(MSB)
    xchg_spi((address >> 8) & 0xff);
    xchg_spi(address & 0xff); // (LSB)
}

uint8_t write_extend_flash(uint32_t address, uint8_t *data, uint32_t size)
{
    uint8_t status = FLASH_OK;
    int32_t remaining = size;
    uint32_t offset = 0; 

    if (size > (PAGE_SIZE * PAGE_BYTE))
    {
        return FLASH_ERR_PARAM;
    }

    while (remaining > 0)
    {
        write_enable();
        uint32_t chunk_size = (remaining > PAGE_BYTE) ? PAGE_BYTE : remaining;

        CS_LOW();
        write_addr(PAGE_PROGRAM, address);

        for (uint32_t i = 0; i < chunk_size; i++)
        {
            status = xchg_spi(data[offset + i]); 
        }
        CS_HIGH();

        address += chunk_size;
        offset += chunk_size; 
        remaining -= chunk_size;

        wait_flash();
    }


    return FLASH_OK;
}

uint8_t read_extend_flash(uint32_t address, uint8_t *data, uint32_t size)
{
    uint8_t status = FLASH_OK;
    CS_LOW();
    write_addr(READ_DATA, address);
    for (uint32_t i = 0; i < size; i++)
    {
        data[i] = xchg_spi(0x00);
    }
    CS_HIGH();
    return status;
}

 uint8_t erase_chip(void)
{
    uint8_t status = 1;
    // enable
    printf("write_enable\n");
    write_enable();
    // erase
    status = write_cmd(CHIP_ERASE);
    // printf("write_cmd\n");
    // waiting
    wait_flash();
    // printf("wait_flash\n");
    return status == FLASH_OK;
}

void W25x_SectorErase(uint32_t SectorAddr)
{
    if (SectorAddr > SECTOR_SIZE*SECTOR_BYTE)
    {
        return FLASH_ERR_PARAM;
    }
    // enable
    write_enable();
    // resume
    write_cmd(ERASE_PROGRAM_RESUME);
    // erase sector
    CS_LOW();
	
    write_addr(SECTOR_ERASE_4KB, SectorAddr);
   // write_addr(SECTOR_ERASE_4KB, sector * SECTOR_BYTE);
    CS_HIGH();
    // waiting finish
    wait_flash();
    return FLASH_OK;
}

void flash_init(void)
{

    uint8_t read_data[5] = {0};
    read_register(RELEASE_POWER_DOWN_READ_DEVICE_ID, read_data, 4);
    vTaskDelay(pdMS_TO_TICKS(100));
    printf("Release Power-down ID: ");
    for (int i = 0; i < 4; i++)
    {
        printf("%02x ", read_data[i]);
    }
    printf("\n");
}

void get_jedce_id(void)
{

    uint8_t read_data[5] = {0};
    read_register(READ_JEDEC_ID, read_data, 4);

    printf("JEDEC ID: ");
    for (size_t i = 0; i < 4; i++)
    {
        printf("%02x ", read_data[i]);
    }
    printf("\n");

    printf("flash size: %llu Mbit\n", ((unsigned long long)pow(2, read_data[2])) / (1024 * 1024) * 8);
}

 void get_device_id(void)
{

    uint8_t read_data[6] = {0};
    read_register(READ_DEVICE_ID, read_data, 5);
    printf("Device ID: ");
    for (size_t i = 0; i < 5; i++)
    {
        printf("%02x ", read_data[i]);
    }
    printf("\n");
}

