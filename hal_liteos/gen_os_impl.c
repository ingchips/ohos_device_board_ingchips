#include "port_gen_os_driver.h"
#include <stdio.h>
#include "los_task.h"
#include "los_swtmr.h"
#include "los_queue.h"
#include "los_event.h"
#include "los_memory.h"
#include "los_mux.h"
#include "los_sem.h"
#include "ingsoc.h"
#include "los_config.h"
#include "los_tick.h"
#include "los_sched.h"
#include "los_debug.h"

#ifndef LOS_MAX_NEST_DEPTH
#define LOS_MAX_NEST_DEPTH  10
#endif


extern VOID HalPendSV(VOID);
extern VOID HalExcSvcCall(VOID);

static void port_systick_handler(void)
{
    UINT32 intSave = LOS_IntLock();
    OsTickHandler();
    LOS_IntRestore(intSave);
}

void SysTick_Handler(void)
{
    UINT32 intSave = LOS_IntLock();
    // PRINTK("irq\n");
    OsTickHandler();
    LOS_IntRestore(intSave);
}

static struct
{
    uint8_t enhanced_ticks;
} pm_info;

void gen_os_enable_enhanced_ticks(void)
{
    pm_info.enhanced_ticks = 1;
}
VOID RunTaskSample(VOID);
//#define LOSCFG_KERNEL_LOWPOWER

#ifdef LOSCFG_KERNEL_LOWPOWER

#include "los_pm.h"
STATIC volatile UINT32 g_SleepTime = 0;
STATIC VOID UserLpTimeStart(UINT64 nextResponseTime);
STATIC VOID UserLpTimeStop(VOID);
STATIC UINT64 UserLpTimeGet(VOID);
STATIC VOID UserKernelTimerLock(VOID);
STATIC VOID UserKernelTimerUnlock(VOID);
STATIC UINT32 UserDeepSleepSuspend(VOID);
STATIC VOID UserDeepSleepResume(VOID);
STATIC UINT32 UserDeviceSuspend(UINT32 mode);
STATIC VOID UserDeviceResume(UINT32 mode);
void OsSysTickTimerInit(UINT32 reloadValue);


STATIC LosPmTickTimer gs_PmTickSt = {
    .freq = OS_SYS_CLOCK,
    .timerStart = UserLpTimeStart,
    .timerStop = UserLpTimeStop, 
    .timerCycleGet = UserLpTimeGet,
    .tickLock = UserKernelTimerLock,
    .tickUnlock = UserKernelTimerUnlock,
};

STATIC LosPmSysctrl gs_PmSysctrlSt = {
    /* Default handler functions, which are implemented by the product */
    .early = NULL,
    .late = NULL,
    .normalSuspend = ArchEnterSleep,
    .normalResume = NULL,
    .lightSuspend = ArchEnterSleep,
    .lightResume = NULL,
    .deepSuspend = UserDeepSleepSuspend,
    .deepResume = UserDeepSleepResume,
    .shutdownSuspend = NULL,
    .shutdownResume = NULL,
};

STATIC LosPmDevice gs_PmDeviceSt = {
    .suspend = UserDeviceSuspend,
    .resume = UserDeviceResume,
};
#endif
#include "stdlib.h"
#include "ohos_mem_pool.h"
#include "hiview_log.h"
#include "los_debug.h"
#define LOG_FMT_MAX_LEN 256
boolean HilogProc_Impl(const HiLogContent *hilogContent, uint32 len)
{
    char tempOutStr[LOG_FMT_MAX_LEN] = {0};
    if (LogContentFmt(tempOutStr, sizeof(tempOutStr), hilogContent) > 0) {
        printf(tempOutStr);
    }
    return TRUE;
}
extern char __HeapBase, __HeapLimit;

void mainTask(void) {
    // Avoiding HCTEST being called before real LiteParamService
   LiteParamService();
#ifdef LOSCFG_DRIVERS_HDF_STORAGE
	DeviceManagerStart();
#endif
    OHOS_SystemInit();
    /* register hilog output func for mini */
    HiviewRegisterHilogProc(HilogProc_Impl);
   while(1)
   {
       osDelay(1000);
  //     printf("t\r\n");
   }
    return NULL;
}
UINT32 tid;
TSK_INIT_PARAM_S task_init_param = {0};
void MainTaskInit(VOID)
{
    task_init_param.usTaskPrio = 2; 
    task_init_param.pcName = "mainTask"; 
    task_init_param.pfnTaskEntry = (TSK_ENTRY_FUNC)mainTask;
    task_init_param.uwStackSize = 4096;        
    LOS_TaskCreate(&tid, &task_init_param);  
}

void n45_IRQHandler(void)
{
    printf("n45hand\n");
    while(1);
}

void n46_IRQHandler(void)
{
    printf("n46hand\n");
    while(1);
}

const gen_os_driver_t *os_impl_get_driver(void)
{
	extern char __end__;
	extern char __HeapLimit;
    NVIC_ClearPendingIRQ(SysTick_IRQn);
    NVIC_DisableIRQ(SysTick_IRQn);
	printf("Heap starts at: %p\n", &__HeapBase);
	printf("Heap ends at: %p\n", &__HeapLimit);
	void *ptr = malloc(4);
	printf("Allocated memory: %p\n", ptr);	
    ptr = malloc(4);
	printf("Allocated memory: %p\n", ptr);	
	//ptr = ll_malloc(0x9000);
	//printf("Allocated ll mem: %p\n", ptr);	
    //LOS_KernelInit initializes the NVIC Settings. we don't want to do that.
    LOS_KernelInit();	
    OsSysTickTimerInit(LOSCFG_BASE_CORE_TICK_RESPONSE_MAX);
    NVIC_EnableIRQ(SysTick_IRQn);
    // RunTaskSample();
    MainTaskInit();
#ifdef LOSCFG_KERNEL_LOWPOWER
    LOS_PmRegister(LOS_PM_TYPE_TICK_TIMER, &gs_PmTickSt);
    LOS_PmRegister(LOS_PM_TYPE_SYSCTRL, &gs_PmSysctrlSt);
    LOS_PmRegister(LOS_PM_TYPE_DEVICE, &gs_PmDeviceSt);
    LOS_PmModeSet(LOS_SYS_DEEP_SLEEP);
#endif
LOS_Start();
while (1) {

}
    return 0;
}



#define _SYSTICK_PRI    (*(uint8_t  *)(0xE000ED23UL))

/* Constants required to manipulate the core.  Registers first... */
#define portNVIC_SYSTICK_CTRL_REG			( * ( ( volatile uint32_t * ) 0xe000e010 ) )
#define portNVIC_SYSTICK_LOAD_REG			( * ( ( volatile uint32_t * ) 0xe000e014 ) )
#define portNVIC_SYSTICK_CURRENT_VALUE_REG	( * ( ( volatile uint32_t * ) 0xe000e018 ) )
#define portNVIC_SYSPRI2_REG				( * ( ( volatile uint32_t * ) 0xe000ed20 ) )
#define portNVIC_CCR_REG                    ( * ( ( volatile uint32_t * ) 0xE000ED14 ) )
/* ...then bits in the registers. */
#define portNVIC_SYSTICK_CLK_BIT	        ( 1UL << 2UL )
#define portNVIC_SYSTICK_INT_BIT			( 1UL << 1UL )
#define portNVIC_SYSTICK_ENABLE_BIT			( 1UL << 0UL )
#define portNVIC_SYSTICK_COUNT_FLAG_BIT		( 1UL << 16UL )
#define portNVIC_PENDSVCLEAR_BIT 			( 1UL << 27UL )
#define portNVIC_PEND_SYSTICK_CLEAR_BIT		( 1UL << 25UL )
#define portSY_FULL_READ_WRITE              15

#define RTC_CYCLES_PER_TICK                 (RTC_CLK_FREQ / LOSCFG_BASE_CORE_TICK_PER_SECOND)
#define MAXIMUM_SUPPRESSED_TICKS            (0xffffff / RTC_CYCLES_PER_TICK)
#define EXPECTED_IDLE_TIME_BEFORE_SLEEP     2
#define MISSED_COUNTS_FACTOR                12
#if(INGCHIPS_FAMILY == INGCHIPS_FAMILY_916)
#define STOPPED_TIMER_COMPENSATION          (MISSED_COUNTS_FACTOR / ( SYSCTRL_GetPLLClk() / RTC_CLK_FREQ ))
#elif(INGCHIPS_FAMILY == INGCHIPS_FAMILY_918)
#define STOPPED_TIMER_COMPENSATION          (MISSED_COUNTS_FACTOR / ( PLL_CLK_FREQ / RTC_CLK_FREQ ))
#else
#error "unknown chip"
#endif

void OsSysTickTimerInit(UINT32 reloadValue)
{
    // if ((reloadValue - 1UL) > 0xffffff)
    // {
    //     return;                                               /* Reload value impossible */
    // }
    // portNVIC_SYSTICK_CTRL_REG = 0;
    // portNVIC_SYSTICK_LOAD_REG  = (uint32_t)(reloadValue - 1UL);                         /* set reload register */
    // portNVIC_SYSTICK_CURRENT_VALUE_REG   = 0UL;                                             /* Load the SysTick Counter Value */
    // portNVIC_SYSTICK_CTRL_REG  =    portNVIC_SYSTICK_CLK_BIT   | 
    //                                 portNVIC_SYSTICK_INT_BIT   |
    //                                 portNVIC_SYSTICK_ENABLE_BIT;
    portNVIC_CCR_REG = 0x200;//remove div 0 and unalign falut error;
}


#ifdef LOSCFG_KERNEL_LOWPOWER
STATIC VOID UserLpTimeStart(UINT64 nextResponseTime)
{
    UINT32 intSave;

    intSave = LOS_IntLock();
    g_SleepTime = nextResponseTime/(LOSCFG_BASE_CORE_TICK_RESPONSE_MAX);
    if(g_SleepTime < MISSED_COUNTS_FACTOR)
    {
        g_SleepTime = 0;
        __WFI();
        return;
    }
    
    g_SleepTime = platform_pre_suppress_ticks_and_sleep_processing(g_SleepTime);
    LOS_IntRestore(intSave);
}

STATIC VOID UserLpTimeStop(VOID)
{

}

STATIC UINT64 UserLpTimeGet(VOID)
{
    // if(g_SleepTime < MISSED_COUNTS_FACTOR)
    //     return 0;
    return g_SleepTime * LOSCFG_BASE_CORE_TICK_RESPONSE_MAX;
}

STATIC VOID UserKernelTimerLock(VOID) 
{
    
}

STATIC VOID UserKernelTimerUnlock(VOID)
{
    
}

STATIC UINT32 UserDeepSleepSuspend(VOID)
{
    UINT32 intSave;
    uint32_t ulCompleteTickPeriods;
    
    if(g_SleepTime == 0)
    {
        return 0;
    }
    intSave = LOS_IntLock();
    portNVIC_SYSTICK_CTRL_REG &= ~portNVIC_SYSTICK_ENABLE_BIT;//close systick
    // calculate the expected ticks
    uint32_t ulReloadValue = portNVIC_SYSTICK_CURRENT_VALUE_REG + (RTC_CYCLES_PER_TICK * (g_SleepTime - 1UL));
    if( ulReloadValue > STOPPED_TIMER_COMPENSATION )
    {
        ulReloadValue -= STOPPED_TIMER_COMPENSATION;
    }
    portNVIC_SYSTICK_LOAD_REG = ulReloadValue;
    portNVIC_SYSTICK_CURRENT_VALUE_REG = 0UL;
    portNVIC_SYSTICK_CTRL_REG |= portNVIC_SYSTICK_ENABLE_BIT;

    platform_pre_sleep_processing();
    platform_post_sleep_processing();

    portNVIC_SYSTICK_CTRL_REG = ( portNVIC_SYSTICK_CLK_BIT | portNVIC_SYSTICK_INT_BIT );
    if( ( portNVIC_SYSTICK_CTRL_REG & portNVIC_SYSTICK_COUNT_FLAG_BIT ) != 0 )
    {
        uint32_t ulCalculatedLoadValue;

        ulCalculatedLoadValue = (RTC_CYCLES_PER_TICK - 1UL) - (ulReloadValue - portNVIC_SYSTICK_CURRENT_VALUE_REG);

        if ((ulCalculatedLoadValue < STOPPED_TIMER_COMPENSATION ) || ( ulCalculatedLoadValue > RTC_CYCLES_PER_TICK))
        {
            ulCalculatedLoadValue = (RTC_CYCLES_PER_TICK - 1UL);
        }

        portNVIC_SYSTICK_LOAD_REG = ulCalculatedLoadValue;
        ulCompleteTickPeriods = g_SleepTime  - 1UL;
    }
    else
    {
        uint32_t ulCompletedSysTickDecrements = (g_SleepTime * RTC_CYCLES_PER_TICK) - portNVIC_SYSTICK_CURRENT_VALUE_REG;
        ulCompleteTickPeriods = ulCompletedSysTickDecrements / RTC_CYCLES_PER_TICK;

        portNVIC_SYSTICK_LOAD_REG = ((ulCompleteTickPeriods + 1UL) * RTC_CYCLES_PER_TICK) - ulCompletedSysTickDecrements;
    }

    portNVIC_SYSTICK_CURRENT_VALUE_REG = 0UL;
    portNVIC_SYSTICK_CTRL_REG |= portNVIC_SYSTICK_ENABLE_BIT;
    //update tick

    portNVIC_SYSTICK_LOAD_REG = RTC_CYCLES_PER_TICK - 1UL;
    g_SleepTime = ulCompleteTickPeriods;
    
    LOS_IntRestore(intSave);

    return 0;
}

STATIC VOID UserDeepSleepResume(VOID)
{
    UINT32 intSave;

    intSave = LOS_IntLock();
    platform_os_idle_resumed_hook();
    LOS_IntRestore(intSave);
}

STATIC UINT32 UserDeviceSuspend(UINT32 mode)
{
    (UINT32)mode;
    return 0;
}

STATIC VOID UserDeviceResume(UINT32 mode)
{
    (UINT32)mode;
}
#endif

// extern UINT8 *m_aucSysMem0;
// void platform_get_heap_status(platform_heap_status_t *status)
// {   static uint16_t bytes_minimum_ever_free = 0;//LOSCFG_SYS_HEAP_SIZE;
//     LOS_MEM_POOL_STATUS s;
//     UINT32 used_size, pool_size;

//     status->bytes_minimum_ever_free = bytes_minimum_ever_free;
//     if (LOS_OK == LOS_MemInfoGet(m_aucSysMem0, &s))
//     {
//         status->bytes_free = s.totalFreeSize;
//         if (status->bytes_free < bytes_minimum_ever_free)
//             bytes_minimum_ever_free = status->bytes_free;
//         return;
//     }
//     else
//         status->bytes_free = 0;
//     status->bytes_minimum_ever_free = bytes_minimum_ever_free;
// }

#ifndef LOSCFG_FS_LITTLEFS
int fsync(int fd)
{
    (void)fd;
    return 0;
}
#endif

