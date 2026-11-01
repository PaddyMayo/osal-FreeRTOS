/**
 * \file
 *
 * Purpose: Startup code for the GCC_POSIX host build.
 *
 * A host build has no vector table or reset handler: libc's crt starts the
 * program and calls main() in generic-freertos/src/bsp_start.c. This file only
 * fills the startup slot (OSAL_FREERTOS_SOURCES) every platform names.
 */

// An empty translation unit is not valid C, and this type is never used
// cppcheck-suppress misra-c2012-2.3
typedef int osal_freertos_posix_startup_placeholder;
