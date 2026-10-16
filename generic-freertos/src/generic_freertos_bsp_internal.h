/*
 * File:  generic_freertos_bsp_internal.h
 *
 * Purpose:
 *   Internal state of the generic FreeRTOS BSP, shared by bsp_start.c, which
 *   defines it, and bsp_console.c.
 */

#ifndef GENERIC_FREERTOS_BSP_INTERNAL_H
#define GENERIC_FREERTOS_BSP_INTERNAL_H

#include <stdbool.h>

#include "FreeRTOS.h"
#include "semphr.h"

#include "osapi-common.h"
#include "bsp-impl.h"

/*
 * BSP types
 */
typedef struct
{
    /*
     * Console output may be locked/unlocked from any FreeRTOS task, so a
     * statically-allocated mutex is used (avoids relying on the heap, and is
     * always valid even if referenced before the scheduler is started).
     */
    StaticSemaphore_t ConsoleMutexBuffer;
    SemaphoreHandle_t ConsoleMutex;

    /*
     * Set by OS_BSP_StartupTask, which only runs once the scheduler is up. If
     * vTaskStartScheduler() returns with this still false, the scheduler never
     * started.
     */
    volatile bool SchedulerRan;

    /* Set by the first fatal error, see OS_BSP_BeginHalt */
    volatile bool Halting;
} OS_BSP_GenericFreeRTOSGlobalData_t;

/*
 * Global Data object
 */
extern OS_BSP_GenericFreeRTOSGlobalData_t OS_BSP_GenericFreeRTOSGlobal;

#endif /* GENERIC_FREERTOS_BSP_INTERNAL_H */
