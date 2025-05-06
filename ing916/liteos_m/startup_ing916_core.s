;/**************************************************************************//**
; * @file     startup_ARMCM4.s
; * @brief    CMSIS Core Device Startup File for
; *           ARMCM4 Device Series
; * @version  V1.08
; * @date     23. November 2012
; *
; * @note
; *
; ******************************************************************************/
;/* Copyright (c) 2011 - 2012 ARM LIMITED
;
;   All rights reserved.
;   Redistribution and use in source and binary forms, with or without
;   modification, are permitted provided that the following conditions are met:
;   - Redistributions of source code must retain the above copyright
;     notice, this list of conditions and the following disclaimer.
;   - Redistributions in binary form must reproduce the above copyright
;     notice, this list of conditions and the following disclaimer in the
;     documentation and/or other materials provided with the distribution.
;   - Neither the name of ARM nor the names of its contributors may be used
;     to endorse or promote products derived from this software without
;     specific prior written permission.
;   *
;   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
;   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
;   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
;   ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS AND CONTRIBUTORS BE
;   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
;   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
;   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
;   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
;   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
;   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
;   POSSIBILITY OF SUCH DAMAGE.
;   ---------------------------------------------------------------------------*/
;/*
;//-------- <<< Use Configuration Wizard in Context Menu >>> ------------------
;*/


; <h> Stack Configuration
;   <o> Stack Size (in Bytes) <0x0-0xFFFFFFFF:8>
; </h>

Stack_Size      EQU     0x00000400

                AREA    STACK, NOINIT, READWRITE, ALIGN=3
Stack_Mem       SPACE   Stack_Size
__initial_sp


; <h> Heap Configuration
;   <o>  Heap Size (in Bytes) <0x0-0xFFFFFFFF:8>
; </h>

Heap_Size       EQU     0x00000C00

                AREA    HEAP, NOINIT, READWRITE, ALIGN=3
__heap_base
Heap_Mem        SPACE   Heap_Size
__heap_limit


                PRESERVE8
                THUMB


; Vector Table Mapped to Address 0 at Reset

                AREA    RESET, DATA, READONLY
                EXPORT  __Vectors
                EXPORT  __Vectors_End
                EXPORT  __Vectors_Size
                IMPORT  g_board_info
__Vectors       DCD     __initial_sp              ; Top of Stack
                DCD     Reset_Handler             ; Reset Handler
                DCD     NMI_Handler               ; NMI Handler
                DCD     HardFault_Handler         ; Hard Fault Handler
                DCD     MemManage_Handler         ; MPU Fault Handler
                DCD     BusFault_Handler          ; Bus Fault Handler
                DCD     UsageFault_Handler        ; Usage Fault Handler
                DCD     0                         ; Reserved
                DCD     DAPLINK_BUILD_KEY         ; DAPLINK: Build type (BL/IF)
                DCD     DAPLINK_HIC_ID            ; DAPLINK: Compatibility
                DCD     DAPLINK_VERSION           ; DAPLINK: Version
                DCD     SVC_Handler               ; SVCall Handler
                DCD     DebugMon_Handler          ; Debug Monitor Handler
                DCD     g_board_info              ; DAPLINK: Pointer to board/family/target info
                DCD     PendSV_Handler            ; PendSV Handler
                DCD     SysTick_Handler           ; SysTick Handler

                ; External Interrupts
                DCD     n00_IRQHandler        
                DCD     n01_IRQHandler        
                DCD     n02_IRQHandler     
                DCD     n03_IRQHandler     
                DCD     n04_IRQHandler       
                DCD     n05_IRQHandler       
                DCD     n06_IRQHandler       
                DCD     n07_IRQHandler        
                DCD     n08_IRQHandler      
                DCD     n09_IRQHandler       
                DCD     n10_IRQHandler        
                DCD     n11_IRQHandler        
                DCD     n12_IRQHandler        
                DCD     n13_IRQHandler       
                DCD     n14_IRQHandler       
                DCD     n15_IRQHandler       
                DCD     n16_IRQHandler       
                DCD     n17_IRQHandler       
                DCD     n18_IRQHandler        
                DCD     n19_IRQHandler        
                DCD     n20_IRQHandler        
                DCD     n21_IRQHandler        
                DCD     n22_IRQHandler       
                DCD     n23_IRQHandler       
                DCD     n24_IRQHandler      
                DCD     n25_IRQHandler       
                DCD     n26_IRQHandler   
                DCD     n27_IRQHandler  
                DCD     n28_IRQHandler  
                DCD     n29_IRQHandler  
                DCD     n30_IRQHandler  
                DCD     n31_IRQHandler  
                DCD     n32_IRQHandler  
                DCD     n33_IRQHandler  
                DCD     n34_IRQHandler  
                DCD     n35_IRQHandler  
                DCD     n36_IRQHandler  
                DCD     n37_IRQHandler  
                DCD     n38_IRQHandler  
                DCD     n39_IRQHandler  
                DCD     n40_IRQHandler  
                DCD     n41_IRQHandler  
                DCD     n42_IRQHandler  
                DCD     n43_IRQHandler  
                DCD     n44_IRQHandler  
                DCD     n45_IRQHandler  
                DCD     n46_IRQHandler  
__Vectors_End

__Vectors_Size  EQU     __Vectors_End - __Vectors

                AREA    |.text|, CODE, READONLY


; Reset Handler

Reset_Handler   PROC
                EXPORT  Reset_Handler             [WEAK]
                IMPORT  init_memory

				LDR     R0, =init_memory
                BLX     R0
                
                IMPORT  __main
                LDR     R0, =__main
                BX      R0
                ENDP


; Dummy Exception Handlers (infinite loops which can be modified)

NMI_Handler     PROC
                EXPORT  NMI_Handler               [WEAK]
                B       .
                ENDP
HardFault_Handler\
                PROC
                EXPORT  HardFault_Handler         [WEAK]
                B       .
                ENDP
MemManage_Handler\
                PROC
                EXPORT  MemManage_Handler         [WEAK]
                B       .
                ENDP
BusFault_Handler\
                PROC
                EXPORT  BusFault_Handler          [WEAK]
                B       .
                ENDP
UsageFault_Handler\
                PROC
                EXPORT  UsageFault_Handler        [WEAK]
                B       .
                ENDP
SVC_Handler     PROC
                EXPORT  SVC_Handler               [WEAK]
                B       .
                ENDP
DebugMon_Handler\
                PROC
                EXPORT  DebugMon_Handler          [WEAK]
                B       .
                ENDP
PendSV_Handler  PROC
                EXPORT  PendSV_Handler            [WEAK]
                B       .
                ENDP
SysTick_Handler PROC
                EXPORT  SysTick_Handler           [WEAK]
                B       .
                ENDP

Default_Handler PROC

                EXPORT n00_IRQHandler    [WEAK]
                EXPORT n01_IRQHandler    [WEAK]
                EXPORT n02_IRQHandler    [WEAK]
                EXPORT n03_IRQHandler    [WEAK]
                EXPORT n04_IRQHandler    [WEAK]
                EXPORT n05_IRQHandler    [WEAK]  
                EXPORT n06_IRQHandler    [WEAK]
                EXPORT n07_IRQHandler    [WEAK]
                EXPORT n08_IRQHandler    [WEAK]
                EXPORT n09_IRQHandler    [WEAK]
                EXPORT n10_IRQHandler    [WEAK]
                EXPORT n11_IRQHandler    [WEAK]
                EXPORT n12_IRQHandler    [WEAK]
                EXPORT n13_IRQHandler    [WEAK]
                EXPORT n14_IRQHandler    [WEAK]
                EXPORT n15_IRQHandler    [WEAK]
                EXPORT n16_IRQHandler    [WEAK]
                EXPORT n17_IRQHandler    [WEAK]
                EXPORT n18_IRQHandler    [WEAK]
                EXPORT n19_IRQHandler    [WEAK]
                EXPORT n20_IRQHandler    [WEAK]
                EXPORT n21_IRQHandler    [WEAK]
                EXPORT n22_IRQHandler    [WEAK]
                EXPORT n23_IRQHandler    [WEAK]
                EXPORT n24_IRQHandler    [WEAK]
                EXPORT n25_IRQHandler    [WEAK]
                EXPORT n26_IRQHandler    [WEAK]
                EXPORT n27_IRQHandler    [WEAK]
                EXPORT n28_IRQHandler    [WEAK]
                EXPORT n29_IRQHandler    [WEAK]
                EXPORT n30_IRQHandler    [WEAK]
                EXPORT n31_IRQHandler    [WEAK]
                EXPORT n32_IRQHandler    [WEAK]
                EXPORT n33_IRQHandler    [WEAK]
                EXPORT n34_IRQHandler    [WEAK]
                EXPORT n35_IRQHandler    [WEAK]
                EXPORT n36_IRQHandler    [WEAK]
                EXPORT n37_IRQHandler    [WEAK]
                EXPORT n38_IRQHandler    [WEAK]
                EXPORT n39_IRQHandler    [WEAK]
                EXPORT n40_IRQHandler    [WEAK]
                EXPORT n41_IRQHandler    [WEAK]
                EXPORT n42_IRQHandler    [WEAK]
                EXPORT n43_IRQHandler    [WEAK]
                EXPORT n44_IRQHandler    [WEAK]
                EXPORT n45_IRQHandler    [WEAK]
                EXPORT n46_IRQHandler    [WEAK]

n00_IRQHandler   
n01_IRQHandler   
n02_IRQHandler
n03_IRQHandler
n04_IRQHandler  
n05_IRQHandler  
n06_IRQHandler  
n07_IRQHandler   
n08_IRQHandler 
n09_IRQHandler  
n10_IRQHandler   
n11_IRQHandler   
n12_IRQHandler   
n13_IRQHandler  
n14_IRQHandler  
n15_IRQHandler  
n16_IRQHandler  
n17_IRQHandler  
n18_IRQHandler   
n19_IRQHandler   
n20_IRQHandler   
n21_IRQHandler   
n22_IRQHandler  
n23_IRQHandler  
n24_IRQHandler 
n25_IRQHandler  
n26_IRQHandler   
n27_IRQHandler  
n28_IRQHandler  
n29_IRQHandler  
n30_IRQHandler  
n31_IRQHandler  
n32_IRQHandler  
n33_IRQHandler  
n34_IRQHandler  
n35_IRQHandler  
n36_IRQHandler  
n37_IRQHandler  
n38_IRQHandler  
n39_IRQHandler  
n40_IRQHandler  
n41_IRQHandler  
n42_IRQHandler  
n43_IRQHandler  
n44_IRQHandler  
n45_IRQHandler  
n46_IRQHandler  
                B       .

                ENDP


                ALIGN


; User Initial Stack & Heap

                IF      :DEF:__MICROLIB

                EXPORT  __initial_sp
                EXPORT  __heap_base
                EXPORT  __heap_limit

                ELSE

                IMPORT  __use_two_region_memory
                EXPORT  __user_initial_stackheap

__user_initial_stackheap PROC
                LDR     R0, =  Heap_Mem
                LDR     R1, =(Stack_Mem + Stack_Size)
                LDR     R2, = (Heap_Mem +  Heap_Size)
                LDR     R3, = Stack_Mem
                BX      LR
                ENDP

                ALIGN

                ENDIF


                END
