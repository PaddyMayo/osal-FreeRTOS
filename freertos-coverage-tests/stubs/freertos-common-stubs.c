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
#include "task.h"
#include "semphr.h"

#include "osapi-error.h"

#include "os-freertos.h"

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

/*
 * The task API the gate functions in os-impl-common.c use to list and wake the
 * tasks blocked on a gated object, and to keep their lists consistent.
 */
TaskHandle_t UT_Stub_CommonCurrentTask = (TaskHandle_t)0x7000;
TaskHandle_t UT_Stub_LastAbortedTask;

TaskHandle_t xTaskGetCurrentTaskHandle(void)
{
    UT_DEFAULT_IMPL(xTaskGetCurrentTaskHandle);

    return UT_Stub_CommonCurrentTask;
}

BaseType_t xTaskAbortDelay(TaskHandle_t xTask)
{
    UT_Stub_LastAbortedTask = xTask;

    UT_DEFAULT_IMPL(xTaskAbortDelay);

    return pdPASS;
}

void vTaskEnterCritical(void)
{
    UT_DEFAULT_IMPL(vTaskEnterCritical);
}

void vTaskExitCritical(void)
{
    UT_DEFAULT_IMPL(vTaskExitCritical);
}

/*
 * os-impl-common.c calls this for the task object type, but it lives in
 * os-impl-tasks.c and is covered there. This runner links only the common
 * module, so it gets a stand-in that can be made to fail.
 */
int32 OS_FreeRTOS_TaskInit(void)
{
    int32 status;

    status = UT_DEFAULT_IMPL(OS_FreeRTOS_TaskInit);

    if (status < 0)
    {
        return OS_ERROR;
    }

    return OS_SUCCESS;
}

/*
 * os-impl-common.c also calls these for the mutex and condition variable object
 * types; they live in os-impl-mutex.c and os-impl-condvar.c and are covered
 * there.
 */
int32 OS_FreeRTOS_MutexInit(void)
{
    int32 status;

    status = UT_DEFAULT_IMPL(OS_FreeRTOS_MutexInit);

    if (status < 0)
    {
        return OS_ERROR;
    }

    return OS_SUCCESS;
}

int32 OS_FreeRTOS_CondVarInit(void)
{
    int32 status;

    status = UT_DEFAULT_IMPL(OS_FreeRTOS_CondVarInit);

    if (status < 0)
    {
        return OS_ERROR;
    }

    return OS_SUCCESS;
}

/*
 * os-impl-common.c also calls these for the stream and file system object
 * types; they live in os-impl-files.c and os-impl-filesys.c and are covered
 * there.
 */
int32 OS_FreeRTOS_FileInit(void)
{
    int32 status;

    status = UT_DEFAULT_IMPL(OS_FreeRTOS_FileInit);

    if (status < 0)
    {
        return OS_ERROR;
    }

    return OS_SUCCESS;
}

int32 OS_FreeRTOS_FileSysInit(void)
{
    int32 status;

    status = UT_DEFAULT_IMPL(OS_FreeRTOS_FileSysInit);

    if (status < 0)
    {
        return OS_ERROR;
    }

    return OS_SUCCESS;
}
