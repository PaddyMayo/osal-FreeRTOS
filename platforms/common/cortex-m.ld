/*
 * Sections shared by every Cortex-M platform. A platform's link.ld defines the
 * FLASH and RAM regions in MEMORY, then includes this file (the platform's
 * config adds this directory to the linker search path with -L).
 *
 * The initial stack pointer is the top of RAM. The FreeRTOS heap is a static
 * array in .bss, so RAM needs no heap region of its own.
 */

ENTRY(Reset_Handler)

SECTIONS
{
    .isr_vector :
    {
        KEEP(*(.isr_vector))
    } > FLASH

    .text :
    {
        *(.text*)
        *(.rodata*)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
    } > FLASH

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > FLASH

    .ARM.exidx :
    {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    } > FLASH

    _sidata = LOADADDR(.data);

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH

    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM

    _estack = ORIGIN(RAM) + LENGTH(RAM);

    /* The main stack runs the interrupt handlers and the code before the scheduler starts */
    ASSERT(_ebss + 0x400 <= _estack, "RAM too small: .data and .bss leave under 1 KB for the main stack")
}
