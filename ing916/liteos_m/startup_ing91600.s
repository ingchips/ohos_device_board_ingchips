    .syntax    unified
    .arch    armv7-m

    .section .stack
    .align    3
    .equ    Stack_Size, 0x00000400
    .globl    __StackTop
    .globl    __StackLimit
    .globl    Default_Handler
    .extern   init_memory
    .word     __etext
    .word     __data_start__
    .word     __data_end__

    .text
    .thumb
    .thumb_func
    .align    2

    .globl    Reset_Handler
    .type    Reset_Handler, %function
Reset_Handler:
    push    {r1, lr}
    ldr    r0, =init_memory
    blx    r0 

/*  Single section scheme.
 *
 *  The ranges of copy from/to are specified by following symbols
 *    __etext: LMA of start of the section to copy from. Usually end of text
 *    __data_start__: VMA of start of the section to copy to
 *    __data_end__: VMA of end of the section to copy to
 *
 *  All addresses must be aligned to 4 bytes boundary.
 */
    ldr    r1, =__etext
    ldr    r2, =__data_start__
    ldr    r3, =__data_end__

.L_loop1:
    cmp    r2, r3
    ittt    lt
    ldrlt    r0, [r1], #4
    strlt    r0, [r2], #4
    blt    .L_loop1

/*  Single BSS section scheme.
 *
 *  The BSS section is specified by following symbols
 *    __bss_start__: start of the BSS section.
 *    __bss_end__: end of the BSS section.
 *
 *  Both addresses must be aligned to 4 bytes boundary.
 */
ldr    r1, =__bss_start__ 
ldr    r2, =__bss_end__

movs   r0, 0
.L_loop_bss:
    cmp    r1, r2
    itt    lt
    strlt  r0, [r1], #4
    blt    .L_loop_bss

ldr    r0, =app_main
blx    r0

pop    {r1, pc}
    .pool
    .size    Reset_Handler, . - Reset_Handler
__StackLimit:
    .space    Stack_Size
    .size    __StackLimit, . - __StackLimit
__StackTop:
    .size    __StackTop, . - __StackTop

    .section .heap
    .align    3
    .equ    Heap_Size, 1024
    .globl    __HeapBase
    .globl    __HeapLimit
__HeapBase:
    .if    Heap_Size
    .space    Heap_Size
    .endif
    .size    __HeapBase, . - __HeapBase
__HeapLimit:
    .size    __HeapLimit, . - __HeapLimit


    .section .text.Default_Handler,"ax",%progbits
Default_Handler:
    b .

    .section .isr_vector
    .align    2
    .globl    __isr_vector
__isr_vector:
    .word    __StackTop            /* Top of Stack */
    .word    Reset_Handler         /* Reset Handler */
    .word    NMI_Handler           /* NMI Handler */
    .word    HardFault_Handler     /* Hard Fault Handler */
    .word    MemManage_Handler     /* Memory Management Fault Handler */
    .word    BusFault_Handler      /* Bus Fault Handler */
    .word    UsageFault_Handler    /* Usage Fault Handler */
    .word    0                     /* Reserved */
    .word    0                     /* Reserved */
    .word    0                     /* Reserved */
    .word    0                     /* Reserved */
    .word    HalExcSvcCall           /* SVCall Handler */
    .word    DebugMon_Handler      /* Debug Monitor Handler */
    .word    0                     /* Reserved */
    .word    HalPendSV        /* PendSV Handler */
    .word    SysTick_Handler       /* SysTick Handler */
    
    /* External Interrupts */
    .word   n00_IRQHandler
    .word   n01_IRQHandler
    .word   n02_IRQHandler
    .word   n03_IRQHandler
    .word   n04_IRQHandler
    .word   n05_IRQHandler
    .word   n06_IRQHandler
    .word   n07_IRQHandler
    .word   n08_IRQHandler
    .word   n09_IRQHandler
    .word   n10_IRQHandler
    .word   n11_IRQHandler
    .word   n12_IRQHandler
    .word   n13_IRQHandler
    .word   n14_IRQHandler
    .word   n15_IRQHandler
    .word   n16_IRQHandler
    .word   n17_IRQHandler
    .word   n18_IRQHandler
    .word   n19_IRQHandler
    .word   n20_IRQHandler
    .word   n21_IRQHandler
    .word   n22_IRQHandler
    .word   n23_IRQHandler
    .word   n24_IRQHandler
    .word   n25_IRQHandler
    .word   n26_IRQHandler
    .word   n27_IRQHandler
    .word   n28_IRQHandler
    .word   n29_IRQHandler
    .word   n30_IRQHandler
    .word   n31_IRQHandler
    .word   n32_IRQHandler
    .word   n33_IRQHandler
    .word   n34_IRQHandler
    .word   n35_IRQHandler
    .word   n36_IRQHandler
    .word   n37_IRQHandler
    .word   n38_IRQHandler
    .word   n39_IRQHandler
    .word   n40_IRQHandler
    .word   n41_IRQHandler
    .word   n42_IRQHandler
    .word   n43_IRQHandler
    .word   n44_IRQHandler
    .word   n45_IRQHandler
    .word   n46_IRQHandler
    .size    __isr_vector, . - __isr_vector

    .weak HardFault_Handler
    .thumb_set HardFault_Handler,Default_Handler

    .weak NMI_Handler
    .thumb_set NMI_Handler,Default_Handler

    .weak MemManage_Handler
    .thumb_set MemManage_Handler,Default_Handler

    .weak BusFault_Handler
    .thumb_set BusFault_Handler,Default_Handler

    .weak UsageFault_Handler
    .thumb_set UsageFault_Handler,Default_Handler

    .weak SVC_Handler
    .thumb_set SVC_Handler,Default_Handler

    .weak DebugMon_Handler
    .thumb_set DebugMon_Handler,Default_Handler

    .weak PendSV_Handler
    .thumb_set PendSV_Handler,Default_Handler

    .weak SysTick_Handler
    .thumb_set SysTick_Handler,Default_Handler

    .weak n00_IRQHandler
    .thumb_set n00_IRQHandler,Default_Handler

    .weak n01_IRQHandler
    .thumb_set n01_IRQHandler,Default_Handler

    .weak n02_IRQHandler
    .thumb_set n02_IRQHandler,Default_Handler

    .weak n03_IRQHandler
    .thumb_set n03_IRQHandler,Default_Handler

    .weak n04_IRQHandler
    .thumb_set n04_IRQHandler,Default_Handler

    .weak n05_IRQHandler
    .thumb_set n05_IRQHandler,Default_Handler

    .weak n06_IRQHandler
    .thumb_set n06_IRQHandler,Default_Handler

    .weak n07_IRQHandler
    .thumb_set n07_IRQHandler,Default_Handler

    .weak n08_IRQHandler
    .thumb_set n08_IRQHandler,Default_Handler

    .weak n09_IRQHandler
    .thumb_set n09_IRQHandler,Default_Handler

    .weak n10_IRQHandler
    .thumb_set n10_IRQHandler,Default_Handler

    .weak n11_IRQHandler
    .thumb_set n11_IRQHandler,Default_Handler

    .weak n12_IRQHandler
    .thumb_set n12_IRQHandler,Default_Handler

    .weak n13_IRQHandler
    .thumb_set n13_IRQHandler,Default_Handler

    .weak n14_IRQHandler
    .thumb_set n14_IRQHandler,Default_Handler

    .weak n15_IRQHandler
    .thumb_set n15_IRQHandler,Default_Handler

    .weak n16_IRQHandler
    .thumb_set n16_IRQHandler,Default_Handler

    .weak n17_IRQHandler
    .thumb_set n17_IRQHandler,Default_Handler

    .weak n18_IRQHandler
    .thumb_set n18_IRQHandler,Default_Handler

    .weak n19_IRQHandler
    .thumb_set n19_IRQHandler,Default_Handler

    .weak n20_IRQHandler
    .thumb_set n20_IRQHandler,Default_Handler

    .weak n21_IRQHandler
    .thumb_set n21_IRQHandler,Default_Handler

    .weak n22_IRQHandler
    .thumb_set n22_IRQHandler,Default_Handler

    .weak n23_IRQHandler
    .thumb_set n23_IRQHandler,Default_Handler

    .weak n24_IRQHandler
    .thumb_set n24_IRQHandler,Default_Handler

    .weak n25_IRQHandler
    .thumb_set n25_IRQHandler,Default_Handler

    .weak n26_IRQHandler
    .thumb_set n26_IRQHandler,Default_Handler

    .weak n27_IRQHandler
    .thumb_set n27_IRQHandler,Default_Handler

    .weak n28_IRQHandler
    .thumb_set n28_IRQHandler,Default_Handler

    .weak n29_IRQHandler
    .thumb_set n29_IRQHandler,Default_Handler

    .weak n30_IRQHandler
    .thumb_set n30_IRQHandler,Default_Handler

    .weak n31_IRQHandler
    .thumb_set n31_IRQHandler,Default_Handler

    .weak n32_IRQHandler
    .thumb_set n32_IRQHandler,Default_Handler

    .weak n33_IRQHandler
    .thumb_set n33_IRQHandler,Default_Handler

    .weak n34_IRQHandler
    .thumb_set n34_IRQHandler,Default_Handler

    .weak n35_IRQHandler    
    .thumb_set n35_IRQHandler,Default_Handler

    .weak n36_IRQHandler
    .thumb_set n36_IRQHandler,Default_Handler

    .weak n37_IRQHandler
    .thumb_set n37_IRQHandler,Default_Handler

    .weak n38_IRQHandler
    .thumb_set n38_IRQHandler,Default_Handler

    .weak n39_IRQHandler
    .thumb_set n39_IRQHandler,Default_Handler

    .weak n40_IRQHandler
    .thumb_set n40_IRQHandler,Default_Handler

    .weak n41_IRQHandler
    .thumb_set n41_IRQHandler,Default_Handler

    .weak n42_IRQHandler
    .thumb_set n42_IRQHandler,Default_Handler

    .weak n43_IRQHandler
    .thumb_set n43_IRQHandler,Default_Handler

    .weak n44_IRQHandler
    .thumb_set n44_IRQHandler,Default_Handler

    .weak n45_IRQHandler
    .thumb_set n45_IRQHandler,Default_Handler

    .weak n46_IRQHandler
    .thumb_set n46_IRQHandler,Default_Handler

