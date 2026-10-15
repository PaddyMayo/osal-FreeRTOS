/**
 * \file
 *
 * Purpose: Startup code for the GCC_POSIX build for aarch64 Linux.
 *
 * A Linux program has no vector table or reset handler: libc's crt starts the
 * program and calls main() in generic-freertos/src/bsp_start.c. This file only
 * fills the startup slot (OSAL_FREERTOS_SOURCES) every platform names.
 */

typedef int osal_freertos_posix_aarch64_startup_placeholder;
