/*
 * Linker script (OSAL_FREERTOS_LINKER_SCRIPT) for the GCC_ARM_CM3 build, laid
 * out for the Arm MPS2 AN385 board: code RAM at 0x0, data RAM at 0x20000000.
 * The sections are shared by every Cortex-M platform, see ../common/cortex-m.ld.
 */

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 1M
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 1M
}

INCLUDE cortex-m.ld
