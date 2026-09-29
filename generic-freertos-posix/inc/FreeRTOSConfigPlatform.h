/*
 * File: FreeRTOSConfigPlatform.h
 *
 * Purpose:
 *   POSIX simulator platform setup for the shared FreeRTOSConfig.h - see
 *   generic-freertos/inc/FreeRTOSPlatformContract.h for the macros required.
 */

#ifndef FREERTOS_CONFIG_PLATFORM_H
#define FREERTOS_CONFIG_PLATFORM_H

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define OSAL_FREERTOS_PLATFORM_TICK_RATE_HZ             100
#define OSAL_FREERTOS_PLATFORM_OPTIMISED_TASK_SELECTION 0
#define OSAL_FREERTOS_PLATFORM_MINIMAL_STACK_SIZE       (PTHREAD_STACK_MIN)
/* Sized for PTHREAD_STACK_MIN-based task stacks: the OSAL startup task alone needs 4 MiB on aarch64 */
#define OSAL_FREERTOS_PLATFORM_TOTAL_HEAP_SIZE          ((size_t)(16 * 1024 * 1024))

#define OSAL_FREERTOS_PLATFORM_ASSERT_FAILED(file, line)                      \
    do                                                                        \
    {                                                                         \
        fprintf(stderr, "configASSERT failed: %s:%d\n", (file), (int)(line)); \
        abort();                                                              \
    } while (0)

/* The POSIX port's portmacro.h defines portCLEAN_UP_TCB to cancel the task's pthread */
#define OSAL_FREERTOS_PLATFORM_PORT_CLEAN_UP_TCB(pxTCB) vPortCancelThread(pxTCB)

#endif /* FREERTOS_CONFIG_PLATFORM_H */
