/*
 * File: freertos-mutex-stubs.c
 *
 * Purpose:
 *   UT-controllable stub implementations of the fake FreeRTOS recursive mutex
 *   API (fake-inc/semphr.h), used to white-box test
 *   freertos/src/os-impl-mutex.c without a real FreeRTOS kernel. Follows the
 *   same pattern as freertos-queue-stubs.c: UT_DEFAULT_IMPL() lets a test
 *   script failure via UT_SetDeferredRetcode(), otherwise success is the
 *   default. Arguments worth asserting on are captured into the extern globals
 *   below.
 */

#include "utstubs.h"

#include "FreeRTOS.h"
#include "semphr.h"

/* Handle returned by a successful xSemaphoreCreateRecursiveMutex() */
SemaphoreHandle_t UT_Stub_CreatedMutexHandle = (SemaphoreHandle_t)0x3000;

SemaphoreHandle_t UT_Stub_LastDeletedMutex;

SemaphoreHandle_t UT_Stub_LastTakeMutex;
TickType_t        UT_Stub_LastTakeMutexTicks;

SemaphoreHandle_t UT_Stub_LastGiveMutex;

SemaphoreHandle_t xSemaphoreCreateRecursiveMutex(void)
{
    int32 status;

    status = UT_DEFAULT_IMPL(xSemaphoreCreateRecursiveMutex);

    if (status < 0)
    {
        return NULL;
    }

    return UT_Stub_CreatedMutexHandle;
}

void vSemaphoreDelete(SemaphoreHandle_t xSemaphore)
{
    UT_Stub_LastDeletedMutex = xSemaphore;

    UT_DEFAULT_IMPL(vSemaphoreDelete);
}

BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t xMutex, TickType_t xBlockTime)
{
    int32 status;

    UT_Stub_LastTakeMutex      = xMutex;
    UT_Stub_LastTakeMutexTicks = xBlockTime;

    status = UT_DEFAULT_IMPL(xSemaphoreTakeRecursive);

    if (status < 0)
    {
        return pdFALSE;
    }

    return pdTRUE;
}

BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t xMutex)
{
    int32 status;

    UT_Stub_LastGiveMutex = xMutex;

    status = UT_DEFAULT_IMPL(xSemaphoreGiveRecursive);

    if (status < 0)
    {
        return pdFALSE;
    }

    return pdTRUE;
}
