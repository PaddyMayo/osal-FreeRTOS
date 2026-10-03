/*
 * File: freertos-common-stubs.c
 *
 * Purpose:
 *   UT-controllable stub implementations of the fake FreeRTOS semaphore API
 *   (fake-inc/semphr.h), used to white-box test freertos/src/os-impl-common.c
 *   without a real FreeRTOS kernel. Each stub follows the standard
 *   ut_assert pattern: UT_DEFAULT_IMPL() lets a test script failure via
 *   UT_SetDeferredRetcode(), otherwise a reasonable default (success)
 *   behavior is used. Arguments worth asserting on are captured into the
 *   extern globals below.
 */

#include "utstubs.h"

#include "FreeRTOS.h"
#include "semphr.h"

StaticSemaphore_t *UT_Stub_LastCreateSemBuffer;
SemaphoreHandle_t  UT_Stub_LastTakeSem;
TickType_t         UT_Stub_LastTakeTicks;
SemaphoreHandle_t  UT_Stub_LastGiveSem;

SemaphoreHandle_t xSemaphoreCreateBinaryStatic(StaticSemaphore_t *pxSemaphoreBuffer)
{
    int32 status;

    UT_Stub_LastCreateSemBuffer = pxSemaphoreBuffer;

    status = UT_DEFAULT_IMPL(xSemaphoreCreateBinaryStatic);

    if (status < 0)
    {
        return NULL;
    }

    /* Real FreeRTOS also derives the handle from the caller-supplied buffer,
     * so returning it here is a faithful-enough stand-in. */
    return (SemaphoreHandle_t)pxSemaphoreBuffer;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xBlockTime)
{
    UT_Stub_LastTakeSem   = xSemaphore;
    UT_Stub_LastTakeTicks = xBlockTime;

    UT_DEFAULT_IMPL(xSemaphoreTake);

    return pdTRUE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore)
{
    UT_Stub_LastGiveSem = xSemaphore;

    UT_DEFAULT_IMPL(xSemaphoreGive);

    return pdTRUE;
}
