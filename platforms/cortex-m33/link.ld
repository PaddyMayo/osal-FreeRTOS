/*
 * Linker script (OSAL_FREERTOS_LINKER_SCRIPT) for the GCC_ARM_CM33_NTZ_NONSECURE
 * build, laid out for the Arm MPS2 with the AN505 image (Cortex-M33).
 *
 * The board has a 4 MB code RAM, at 0x0 for Non-secure code and at 0x10000000
 * for Secure code (the same memory), and a 2 MB data RAM, at 0x28000000 and
 * 0x38000000. The Secure start-up stub (secure_boot.c) takes the first 4 KB of
 * the code RAM and the last 4 KB of the data RAM for its vector table, code and
 * stack. The firmware is the Non-secure code and uses the rest, see
 * ../common/cortex-m.ld for its sections.
 */

MEMORY
{
    SFLASH (rx)  : ORIGIN = 0x10000000, LENGTH = 4K
    FLASH  (rx)  : ORIGIN = 0x00001000, LENGTH = 1020K
    RAM    (rwx) : ORIGIN = 0x28000000, LENGTH = 2044K
}

_secure_estack = 0x38000000 + 2M;

/* Nothing in the firmware calls the stub, so it is pulled out of its archive by name */
EXTERN(OSAL_SecureVectorTable)

SECTIONS
{
    .secure_text :
    {
        KEEP(*(.secure_isr_vector))
        *(.secure_text*)
        *(.secure_rodata*)
    } > SFLASH
}

INCLUDE cortex-m.ld
