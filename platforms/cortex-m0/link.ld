/*
 * Linker script (OSAL_FREERTOS_LINKER_SCRIPT) for the GCC_ARM_CM0 build, laid out
 * for the BBC micro:bit (Cortex-M0, 16 KB of RAM).
 * The sections are shared by every Cortex-M platform, see ../common/cortex-m.ld.
 */

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 256K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 16K
}

INCLUDE cortex-m.ld
