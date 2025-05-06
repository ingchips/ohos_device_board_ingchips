#include <stdio.h>
#include <string.h>
#include "los_debug.h"
#include "profile.h"
#include "ingsoc.h"
#include "port_gen_os_driver.h"
#include "target_config.h"
#if TARCE_ENABLE
#include "trace.h"
#endif
#if (INGCHIPS_FAMILY == INGCHIPS_FAMILY_916)
#include "../data/setup_soc916.cgen"
#endif
#if (INGCHIPS_FAMILY == INGCHIPS_FAMILY_918)
#include "../data/setup_soc918.cgen"
#endif

void NVIC_SetVectorTable(int NVIC_VectTab, int Offset)
{ 
    /* Check the parameters */ 
    SCB->VTOR = NVIC_VectTab | (Offset & (int)0xFFFFFF80);
}

uint32_t HardFault_Handler(void)
{
    PRINTK("hard fault\n");
    for (;;);
}

#define TRACE_PORT    APB_UART1

#define PRINT_PORT    APB_UART0

uint32_t cb_putc(char *c, void *dummy)
{
    while (apUART_Check_TXFIFO_FULL(PRINT_PORT) == 1);
    UART_SendData(PRINT_PORT, (uint8_t)*c);
    return 0;
}

int _write(int fd, char *ptr, int len)
{
    int i;
    for (i = 0; i < len; i++)
        cb_putc(ptr + i, NULL);

    return len;
}

void setup_peripherals(void)
{
    cube_setup_peripherals();
}

void init_memory(void)
{
    #if (INGCHIPS_FAMILY == INGCHIPS_FAMILY_916)
    SYSCTRL_CacheControl(SYSCTRL_MEM_BLOCK_AS_SYS_MEM, SYSCTRL_MEM_BLOCK_AS_SYS_MEM);//set 
    #endif
}


#if TARCE_ENABLE
trace_rtt_t trace_rtt ;
#endif

#include "soc.h"
void SysInit(void)
{
  if(aon2_ctrl_reg->pwr_ctrl_status0.f.boot_power_up == 0x1){
    aon1_ctrl_reg->aon1_reg3.f.reg_boot_pin_clr = 0x1;
    aon1_ctrl_reg->aon1_boot.r = ((0x1 << 0 ) | //BootConfig Enable
                                  (0x1 << 1 ) | //Pll Enable
                                  (0x1 << 2 ) | //Pll wait time or lock, 0:time, 1:lock
                                  (0x0 << 3 ) | //pll time, 110us/130us/150us/170us
                                  (80  << 5 ) | //pll div loop reg
                                  (0x1 << 13) | //hclk sel
                                  (4   << 14) | //hclk div denom
                                  (0x1 << 18) | //flash clk sel
                                  (2   << 19) | //flash div denom
                                  (0x1 << 23) | //flash 4line
                                  (0x2 << 24) | //flash sample delay
                                  (0x1 << 27) | //cache enable
                                  (0x0 << 28) | //wdt enable
                                  (7UL << 29)   //other, must be 0x7
                                 );
    NVIC_SystemReset();
  }
  

  extern uint32_t __isr_vector;
  NVIC_SetVectorTable(0x0, (uint32_t)&__isr_vector);
  
}

const char welcome_msg[] = "Built with LiteOS (" HW_LITEOS_KERNEL_VERSION_STRING ")";
// TODO: add RTOS source code to the project.
extern const gen_os_driver_t *os_impl_get_driver(void);
uintptr_t app_main()
{
    NVIC_SetPriority(SysTick_IRQn, 0);
    NVIC_DisableIRQ(SysTick_IRQn);
    SysInit();
    SYSCTRL_ConfigPLLClk(5, 70, 1);
    SYSCTRL_SelectHClk(SYSCTRL_CLK_PLL_DIV_3);
    SYSCTRL_Init();

    setup_peripherals();
	SYSCTRL_SelectMemoryBlocks(0x3FF);//全选
  
    printf("build@%s\r\n",__TIME__);
    printf("build@%d\r\n",__LINE__);
    printf("start up\r\n");

    uint32_t SystemCoreClock = SYSCTRL_GetPLLClk();

    printf("starting up: %u\n", SYSCTRL_GetHClk());
    // while(1);

    os_impl_get_driver();
    return 0;
}
