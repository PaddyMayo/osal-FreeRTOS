/*
 * Linker script (OSAL_FREERTOS_LINKER_SCRIPT) for the GCC_ARM_CM7 build, laid out
 * for the Arm MPS2 with the AN500 image (Cortex-M7).
 * The sections are shared by every Cortex-M platform, see ../common/cortex-m.ld.
 */

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 1M
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 1M
}

INCLUDE cortex-m.ld
