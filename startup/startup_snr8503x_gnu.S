/*
 * Startup file for SNR8503M (Cortex-M0) — GNU as syntax
 * Converted from armasm V5 source startup_snr8503x.s
 * 2026-05-12: phase 1.4 of GCC migration
 */

    .syntax unified
    .cpu cortex-m0
    .fpu softvfp
    .thumb

/* ============================================================================
 *  Vector table — placed at start of Flash by linker.ld
 * ============================================================================ */
    .section .isr_vector, "a", %progbits
    .type   g_pfnVectors, %object
    .size   g_pfnVectors, .-g_pfnVectors
    .global g_pfnVectors
    .global __Vectors
    .global __Vectors_End

g_pfnVectors:
__Vectors:
    .word   _stack_top                 /* 0  Top of Stack (from linker)        */
    .word   Reset_Handler              /* 1  Reset Handler                     */
    .word   NMI_Handler                /* 2  NMI Handler                       */
    .word   HardFault_Handler          /* 3  Hard Fault Handler                */
    .word   0                          /* 4  Reserved                          */
    .word   0                          /* 5  Reserved                          */
    .word   0                          /* 6  Reserved                          */
    .word   0                          /* 7  Reserved                          */
    .word   0                          /* 8  Reserved                          */
    .word   0                          /* 9  Reserved                          */
    .word   0                          /* 10 Reserved                          */
    .word   SVC_Handler                /* 11 SVCall                            */
    .word   0                          /* 12 Reserved                          */
    .word   0                          /* 13 Reserved                          */
    .word   PendSV_Handler             /* 14 PendSV                            */
    .word   SysTick_Handler            /* 15 SysTick                           */

    /* External interrupts 0-3 */
    .word   TIMER0_IRQHandler
    .word   TIMER1_IRQHandler
    .word   MCPWM0_IRQHandler
    .word   MCPWM1_IRQHandler
    /* 4-7 */
    .word   I2C_IRQHandler
    .word   SPI_IRQHandler
    .word   GPIO_IRQHandler
    .word   HALL_IRQHandler
    /* 8-11 */
    .word   UART_IRQHandler
    .word   CMP_IRQHandler
    .word   ADC_IRQHandler
    .word   DMA_IRQHandler
    /* 12-15 */
    .word   WAKE_IRQHandler
    .word   SW_IRQHandler
    .word   0
    .word   0
    /* 16-31 reserved */
    .rept 16
    .word   0
    .endr
__Vectors_End:
    .equ    __Vectors_Size, __Vectors_End - __Vectors

/* ============================================================================
 *  Reset_Handler — copy .data, zero .bss, SystemInit, main
 * ============================================================================ */
    .section .text.Reset_Handler, "ax", %progbits
    .weak   Reset_Handler
    .type   Reset_Handler, %function
Reset_Handler:
    /* SP already loaded by hardware from vector[0]; reload defensively */
    ldr     r0, =_stack_top
    mov     sp, r0

    /* Copy .data from Flash (load addr) to RAM (run addr).
     * FIX-001: original loop incremented both r0 (dest pointer) and r3
     * (offset) — effective store address ran at 2x the intended rate,
     * corrupting every initialised global after the first word. */
    ldr     r0, =_sdata
    ldr     r1, =_edata
    ldr     r2, =_sidata
.Lcopy_data:
    cmp     r0, r1
    bge     .Lcopy_done
    ldr     r4, [r2]
    str     r4, [r0]
    adds    r2, #4
    adds    r0, #4
    b       .Lcopy_data
.Lcopy_done:

    /* Zero .bss */
    ldr     r0, =_sbss
    ldr     r1, =_ebss
    movs    r2, #0
.Lzero_bss:
    cmp     r0, r1
    bge     .Lzero_done
    str     r2, [r0]
    adds    r0, #4
    b       .Lzero_bss
.Lzero_done:

    /* Call SystemInit if linked (provided by core_cm0.c / sys file) */
    bl      SystemInit

    /* Jump to main */
    bl      main

    /* If main returns, spin */
.Lhang:
    b       .Lhang
    .size   Reset_Handler, .-Reset_Handler

/* ============================================================================
 *  Default exception handlers — all spin; weak so user code can override
 * ============================================================================ */
    .section .text.Default_Handler, "ax", %progbits
    .type   Default_Handler, %function
Default_Handler:
    b       .
    .size   Default_Handler, .-Default_Handler

    .macro  def_irq_default handler_name
    .weak   \handler_name
    .thumb_set \handler_name, Default_Handler
    .endm

    def_irq_default NMI_Handler
    def_irq_default HardFault_Handler
    def_irq_default SVC_Handler
    def_irq_default PendSV_Handler
    def_irq_default SysTick_Handler

    def_irq_default TIMER0_IRQHandler
    def_irq_default TIMER1_IRQHandler
    def_irq_default MCPWM0_IRQHandler
    def_irq_default MCPWM1_IRQHandler
    def_irq_default I2C_IRQHandler
    def_irq_default SPI_IRQHandler
    def_irq_default GPIO_IRQHandler
    def_irq_default HALL_IRQHandler
    def_irq_default UART_IRQHandler
    def_irq_default CMP_IRQHandler
    def_irq_default ADC_IRQHandler
    def_irq_default DMA_IRQHandler
    def_irq_default WAKE_IRQHandler
    def_irq_default SW_IRQHandler

/* Provide a weak no-op SystemInit so projects without one still link */
    .section .text.SystemInit, "ax", %progbits
    .weak   SystemInit
    .type   SystemInit, %function
SystemInit:
    bx      lr
    .size   SystemInit, .-SystemInit

    .end
