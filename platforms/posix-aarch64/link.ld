/*
 * Linker script slot (OSAL_FREERTOS_LINKER_SCRIPT) for the GCC_POSIX build for
 * aarch64 Linux.
 *
 * It is applied with -T, which replaces the toolchain's default script unless
 * the script ends in INSERT. This one adds an empty SECTIONS command with
 * INSERT, so the toolchain's own script is kept and a program still links.
 */

SECTIONS
{
}
INSERT AFTER .text;
