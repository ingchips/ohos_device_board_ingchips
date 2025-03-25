/* Linker script to configure memory regions.
 * Need modifying for a specific board.
 *   FLASH.ORIGIN: starting address of flash
 *   FLASH.LENGTH: length of flash
 *   RAM.ORIGIN: starting address of RAM bank 0
 *   RAM.LENGTH: length of RAM bank 0
 */
MEMORY
{
  FLASH (rx) : ORIGIN = 0x00024000, LENGTH = 0x61000
  RAM (rwx) : ORIGIN =  0x200009A0, LENGTH = 0xF660
  RAM2 (rwx) : ORIGIN =   0x400A2000, LENGTH = 0xe000
}

/* Linker script to place sections and symbol values. Should be used together
 * with other linker script that defines memory regions FLASH and RAM.
 * It references following symbols, which must be defined in code:
 *   Reset_Handler : Entry of reset handler
 *
 * It defines following symbols, which code can use without definition:
 *   __exidx_start
 *   __exidx_end
 *   __copy_table_start__
 *   __copy_table_end__
 *   __zero_table_start__
 *   __zero_table_end__
 *   __etext
 *   __data_start__
 *   __preinit_array_start
 *   __preinit_array_end
 *   __init_array_start
 *   __init_array_end
 *   __fini_array_start
 *   __fini_array_end
 *   __data_end__ 
 *   __bss_start__ 
 *   __bss_end__
 *   __end__
 *   end
 *   __HeapLimit
 *   __StackLimit
 *   __StackTop
 *   __stack
 */
ENTRY(Reset_Handler)

SECTIONS
{
	.text :
	{
		KEEP(*(.isr_vector))
		*(.text*)
		. = ALIGN(4);
	   __zinitcall_bsp_start = .;
	  KEEP (*(.zinitcall.bsp0.init))
	  KEEP (*(.zinitcall.bsp1.init))
	  KEEP (*(.zinitcall.bsp2.init))
	  KEEP (*(.zinitcall.bsp3.init))
	  KEEP (*(.zinitcall.bsp4.init))
	  __zinitcall_bsp_end = .;
	  	. = ALIGN(4);
	  __zinitcall_device_start = .;
	  KEEP (*(.zinitcall.device0.init))
	  KEEP (*(.zinitcall.device1.init))
	  KEEP (*(.zinitcall.device2.init))
	  KEEP (*(.zinitcall.device3.init))
	  KEEP (*(.zinitcall.device4.init))
	  __zinitcall_device_end = .;
	  	. = ALIGN(4);
	  __zinitcall_core_start = .;
	  KEEP (*(.zinitcall.core0.init))
	  KEEP (*(.zinitcall.core1.init))
	  KEEP (*(.zinitcall.core2.init))
	  KEEP (*(.zinitcall.core3.init))
	  KEEP (*(.zinitcall.core4.init))
	  __zinitcall_core_end = .;
	  	. = ALIGN(4);
	  __zinitcall_sys_service_start = .;
	  KEEP (*(.zinitcall.sys.service0.init))
	  KEEP (*(.zinitcall.sys.service1.init))
	  KEEP (*(.zinitcall.sys.service2.init))
	  KEEP (*(.zinitcall.sys.service3.init))
	  KEEP (*(.zinitcall.sys.service4.init))
	  __zinitcall_sys_service_end = .;
	  	. = ALIGN(4);
	  __zinitcall_sys_feature_start = .;
	  KEEP (*(.zinitcall.sys.feature0.init))
	  KEEP (*(.zinitcall.sys.feature1.init))
	  KEEP (*(.zinitcall.sys.feature2.init))
	  KEEP (*(.zinitcall.sys.feature3.init))
	  KEEP (*(.zinitcall.sys.feature4.init))
	  __zinitcall_sys_feature_end = .;
		. = ALIGN(4);
	  __zinitcall_run_start = .;
	  KEEP (*(.zinitcall.run0.init))
	  KEEP (*(.zinitcall.run1.init))
	  KEEP (*(.zinitcall.run2.init))
	  KEEP (*(.zinitcall.run3.init))
	  KEEP (*(.zinitcall.run4.init))
	  __zinitcall_run_end = .;
	  	. = ALIGN(4);
	  __zinitcall_app_service_start = .;
	  KEEP (*(.zinitcall.app.service0.init))
	  KEEP (*(.zinitcall.app.service1.init))
	  KEEP (*(.zinitcall.app.service2.init))
	  KEEP (*(.zinitcall.app.service3.init))
	  KEEP (*(.zinitcall.app.service4.init))
	  __zinitcall_app_service_end = .;
		. = ALIGN(4);
	  __zinitcall_app_feature_start = .;
	  KEEP (*(.zinitcall.app.feature0.init))
	  KEEP (*(.zinitcall.app.feature1.init))
	  KEEP (*(.zinitcall.app.feature2.init))
	  KEEP (*(.zinitcall.app.feature3.init))
	  KEEP (*(.zinitcall.app.feature4.init))
	  __zinitcall_app_feature_end = .;
	  	. = ALIGN(4);
	  __zinitcall_test_start = .;
	  KEEP (*(.zinitcall.test0.init))
	  KEEP (*(.zinitcall.test1.init))
	  KEEP (*(.zinitcall.test2.init))
	  KEEP (*(.zinitcall.test3.init))
	  KEEP (*(.zinitcall.test4.init))
	  __zinitcall_test_end = .;
		. = ALIGN(4);
	  __zinitcall_exit_start = .;
	  KEEP (*(.zinitcall.exit0.init))
	  KEEP (*(.zinitcall.exit1.init))
	  KEEP (*(.zinitcall.exit2.init))
	  KEEP (*(.zinitcall.exit3.init))
	  KEEP (*(.zinitcall.exit4.init))
	  __zinitcall_exit_end = .;
		. = ALIGN(4);
		*(.init)
		*(.fini)

		/* .ctors */
		*crtbegin.o(.ctors)
		*crtbegin?.o(.ctors)
		*(EXCLUDE_FILE(*crtend?.o *crtend.o) .ctors)
		*(SORT(.ctors.*))
		*(.ctors)

		/* .dtors */
 		*crtbegin.o(.dtors)
 		*crtbegin?.o(.dtors)
 		*(EXCLUDE_FILE(*crtend?.o *crtend.o) .dtors)
 		*(SORT(.dtors.*))
 		*(.dtors)

		*(.rodata*)

		*(.eh_frame*)
	} > FLASH

 .hdf_drivers_entry :
  {
    _hdf_drivers_start = .;
    KEEP(*(.hdf.driver))
    _hdf_drivers_end = .;
  } >FLASH

	
	.ARM.extab :
	{
		*(.ARM.extab* .gnu.linkonce.armextab.*)
	} > FLASH

	__exidx_start = .;
	.ARM.exidx :
	{
		*(.ARM.exidx* .gnu.linkonce.armexidx.*)
	} > FLASH
	__exidx_end = .;

	/* To copy multiple ROM to RAM sections,
	 * uncomment .copy.table section and,
	 * define __STARTUP_COPY_MULTIPLE in startup_ARMCMx.S */
	/*
	.copy.table :
	{
		. = ALIGN(4);
		__copy_table_start__ = .;
		LONG (__etext)
		LONG (__data_start__)
		LONG (__data_end__ - __data_start__)
		LONG (__etext2)
		LONG (__data2_start__)
		LONG (__data2_end__ - __data2_start__)
		__copy_table_end__ = .;
	} > FLASH
	*/

	/* To clear multiple BSS sections,
	 * uncomment .zero.table section and,
	 * define __STARTUP_CLEAR_BSS_MULTIPLE in startup_ARMCMx.S */
	/*
	.zero.table :
	{
		. = ALIGN(4);
		__zero_table_start__ = .;
		LONG (__bss_start__)
		LONG (__bss_end__ - __bss_start__)
		__zero_table_end__ = .;
	} > FLASH
	*/

	/* Location counter can end up 2byte aligned with narrow Thumb code but
	   __etext is assumed by startup code to be the LMA of a section in RAM
	   which must be 4byte aligned */
	__etext = ALIGN (4);

	.data : AT (__etext)
	{
		__data_start__ = .;
		*(vtable)
		*(.data*)

		. = ALIGN(4);
		/* preinit data */
		PROVIDE_HIDDEN (__preinit_array_start = .);
		*(.preinit_array)
		PROVIDE_HIDDEN (__preinit_array_end = .);

		. = ALIGN(4);
		/* init data */
		PROVIDE_HIDDEN (__init_array_start = .);
		*(SORT(.init_array.*))
		*(.init_array)
		PROVIDE_HIDDEN (__init_array_end = .);


		. = ALIGN(4);
		/* finit data */
		PROVIDE_HIDDEN (__fini_array_start = .);
		*(SORT(.fini_array.*))
		*(.fini_array)
		PROVIDE_HIDDEN (__fini_array_end = .);

		*(.jcr)
		. = ALIGN(4);
		/* All data end */
		__data_end__ = .;

	} > RAM

	.bss2 :
	{
	  __los_heap_addr_start__ = .;
	  __los_heap_addr_end__ = ORIGIN(RAM2) + LENGTH(RAM2) -1;
	} > RAM2

	.bss :
	{
		. = ALIGN(4);
		__bss_start__ = .;
		
		*(.bss)
    	*(.bss*)
		*(COMMON)
		. = ALIGN(4);
		__bss_end__ = .;
		__bss_end = .;
	} > RAM

	.heap (COPY):
	{
		__end__ = .;
		PROVIDE(end = .);
		 *(.heap*) 
 		__HeapLimit = .;
	} > RAM

	/* .stack_dummy section doesn't contains any symbols. It is only
	 * used for linker to calculate size of stack sections, and assign
	 * values to stack symbols later */
	.stack_dummy (COPY):
	{
		*(.stack*)
	} > RAM

	/* Set stack top to end of RAM, and stack limit move down by
	 * size of stack_dummy section */
	__StackTop = ORIGIN(RAM) + LENGTH(RAM);
	__StackLimit = __StackTop - SIZEOF(.stack_dummy);
	PROVIDE(__stack = __StackTop);

	/* Check if data + heap + stack exceeds RAM limit */
	ASSERT(__StackLimit >= __HeapLimit, "region RAM overflowed with stack")
}
