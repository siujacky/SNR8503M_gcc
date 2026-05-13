/*
 * linker.ld — SNR8503M (LKS32MC03x) Cortex-M0
 * 256 KB Flash @ 0x00000000, 128 KB RAM @ 0x20000000
 */

ENTRY(Reset_Handler)

MEMORY
{
    /* Phase 8.1: last 4 KB of Flash reserved for runtime config block.
     * .text/.rodata fit in the first 252 KB; .flash_config sector lives at 0x3F000. */
    FLASH        (rx)  : ORIGIN = 0x00000000, LENGTH = 252K
    FLASH_CONFIG (r)   : ORIGIN = 0x0003F000, LENGTH = 4K
    RAM          (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

/* Symbol consumed by flash_config.c (FLASH_CONFIG_ADDR macro) */
__flash_config_start = ORIGIN(FLASH_CONFIG);
__flash_config_end   = ORIGIN(FLASH_CONFIG) + LENGTH(FLASH_CONFIG);

/* Top of stack — bottom is determined by main() via SP at reset */
_stack_top  = ORIGIN(RAM) + LENGTH(RAM);

/* Minimum stack & heap reservation (kept for diagnostics; not strictly enforced) */
_Min_Heap_Size  = 0x200;
_Min_Stack_Size = 0x80;

SECTIONS
{
    /* Vector table at start of Flash (vector[0] = initial SP, vector[1] = Reset_Handler) */
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    /* Program code + read-only data */
    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.glue_7)
        *(.glue_7t)
        *(.eh_frame)

        KEEP(*(.init))
        KEEP(*(.fini))

        . = ALIGN(4);
        _etext = .;
    } > FLASH

    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > FLASH

    .ARM :
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > FLASH

    /* Constructors / destructors (newlib startup) */
    .preinit_array :
    {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } > FLASH

    .init_array :
    {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN(__init_array_end = .);
    } > FLASH

    .fini_array :
    {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } > FLASH

    /* .data — initialised in Flash, runs from RAM. Reset_Handler copies. */
    _sidata = LOADADDR(.data);
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH

    /* .bss — zero-init in Reset_Handler */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } > RAM

    /* FIX-020: .ram3functions section dropped — was a trap (placed AT FLASH but
     * Reset_Handler doesn't copy it to RAM). Re-add with a copy loop if any
     * function uses __attribute__((section("ram3functions"))). */

    /* Stack & heap reservations (diagnostic only — actual stack lives at _stack_top) */
    ._user_heap_stack :
    {
        . = ALIGN(8);
        PROVIDE(end = .);
        PROVIDE(_end = .);
        . = . + _Min_Heap_Size;
        . = . + _Min_Stack_Size;
        . = ALIGN(8);
    } > RAM

    /* Discard unwind info etc. */
    /DISCARD/ :
    {
        libc.a (*)
        libm.a (*)
        libgcc.a (*)
    }

    .ARM.attributes 0 : { *(.ARM.attributes) }
}
