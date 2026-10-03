/*
 * File: console/posix.h
 *
 * Purpose:
 *   Console backend for the FreeRTOS POSIX simulator (CONSOLE posix): the
 *   console is the host process's stdout. Included only by
 *   generic-freertos/src/bsp_console.c, which defines the interface below.
 */

#ifndef OSAL_FREERTOS_CONSOLE_POSIX_H
#define OSAL_FREERTOS_CONSOLE_POSIX_H

#include <stdlib.h>
#include <unistd.h>

static inline size_t OS_FreeRTOS_ConsoleWrite(const char *Str, size_t Len)
{
    /* writes the raw data directly to STDOUT_FILENO (unbuffered) */
    ssize_t WriteLen = write(STDOUT_FILENO, Str, Len);

    return (WriteLen > 0) ? (size_t)WriteLen : 0U;
}

static inline void OS_FreeRTOS_PlatformHalt(void)
{
    abort();
}

#endif /* OSAL_FREERTOS_CONSOLE_POSIX_H */
