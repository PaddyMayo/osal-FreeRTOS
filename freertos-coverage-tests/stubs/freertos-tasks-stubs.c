/*
 * File: freertos-task-stubs.c
 *
 * Purpose:
 *   UT-controllable stub implementations of the fake FreeRTOS task API
 *   (fake-inc/task.h), used to white-box test freertos/src/os-impl-tasks.c
 *   without a real FreeRTOS kernel. Each stub follows the standard
 *   ut_assert pattern: UT_DEFAULT_IMPL()/UT_DEFAULT_IMPL_RC() lets a test
 *   script failure via UT_SetDeferredRetcode()/UT_SetDefaultReturnValue(),
 *   otherwise a reasonable default (success) behavior is used. Arguments
 *   worth asserting on are captured into the extern globals below.
 */

#include <stdlib.h>
#include <string.h>

#include "utstubs.h"

#include "FreeRTOS.h"
#include "task.h"

#define UT_STUB_TLS_SLOTS 4

TaskHandle_t UT_Stub_CurrentTaskHandle = (TaskHandle_t)0x1000;

static void *UT_Stub_TLSArray[UT_STUB_TLS_SLOTS];

TaskHandle_t UT_Stub_LastDeletedHandle;
TaskHandle_t UT_Stub_LastPriorityHandle;
UBaseType_t  UT_Stub_LastPriorityValue;
TickType_t   UT_Stub_LastDelayTicks;

TaskFunction_t         UT_Stub_LastCreateTaskCode;
void                  *UT_Stub_LastCreateParameters;
const char            *UT_Stub_LastCreateName;
UBaseType_t            UT_Stub_LastCreatePriority;
configSTACK_DEPTH_TYPE UT_Stub_LastCreateStackDepth;

TaskHandle_t xTaskGetCurrentTaskHandle(void)
{
    int32 status;

    status = UT_DEFAULT_IMPL(xTaskGetCurrentTaskHandle);

    if (status < 0)
    {
        return NULL;
    }

    return UT_Stub_CurrentTaskHandle;
}

TaskHandle_t xTaskCreateStatic(TaskFunction_t               pxTaskCode,
                               const char *const            pcName,
                               const configSTACK_DEPTH_TYPE ulStackDepth,
                               void *const                  pvParameters,
                               UBaseType_t                  uxPriority,
                               StackType_t *const           puxStackBuffer,
                               StaticTask_t *const          pxTaskBuffer)
{
    int32 status;

    (void)puxStackBuffer;

    UT_Stub_LastCreateTaskCode   = pxTaskCode;
    UT_Stub_LastCreateParameters = pvParameters;
    UT_Stub_LastCreateName       = pcName;
    UT_Stub_LastCreatePriority   = uxPriority;
    UT_Stub_LastCreateStackDepth = ulStackDepth;

    status = UT_DEFAULT_IMPL(xTaskCreateStatic);

    if (status < 0)
    {
        return NULL;
    }

    /* Real FreeRTOS also derives the handle from the caller-supplied TCB
     * buffer, so returning it here is a faithful-enough stand-in. */
    return (TaskHandle_t)pxTaskBuffer;
}

void vTaskDelete(TaskHandle_t xTaskToDelete)
{
    UT_Stub_LastDeletedHandle = xTaskToDelete;

    UT_DEFAULT_IMPL(vTaskDelete);
}

void vTaskDelay(TickType_t xTicksToDelay)
{
    UT_Stub_LastDelayTicks = xTicksToDelay;

    UT_DEFAULT_IMPL(vTaskDelay);
}

void vTaskPrioritySet(TaskHandle_t xTask, UBaseType_t uxNewPriority)
{
    UT_Stub_LastPriorityHandle = xTask;
    UT_Stub_LastPriorityValue  = uxNewPriority;

    UT_DEFAULT_IMPL(vTaskPrioritySet);
}

void *pvTaskGetThreadLocalStoragePointer(TaskHandle_t xTaskToQuery, int xIndex)
{
    (void)xTaskToQuery;

    UT_DEFAULT_IMPL(pvTaskGetThreadLocalStoragePointer);

    if (xIndex < 0 || xIndex >= UT_STUB_TLS_SLOTS)
    {
        return NULL;
    }

    return UT_Stub_TLSArray[xIndex];
}

void vTaskSetThreadLocalStoragePointer(TaskHandle_t xTaskToSet, int xIndex, void *pvValue)
{
    (void)xTaskToSet;

    UT_DEFAULT_IMPL(vTaskSetThreadLocalStoragePointer);

    if (xIndex >= 0 && xIndex < UT_STUB_TLS_SLOTS)
    {
        UT_Stub_TLSArray[xIndex] = pvValue;
    }
}

void *pvPortMalloc(size_t xSize)
{
    int32 status;

    status = UT_DEFAULT_IMPL(pvPortMalloc);

    if (status < 0)
    {
        return NULL;
    }

    return malloc(xSize);
}

void vPortFree(void *pv)
{
    UT_DEFAULT_IMPL(vPortFree);

    free(pv);
}

/*
 * Test-only helper (not a fake of any real FreeRTOS API) - clears the fake
 * thread-local storage array between test cases, since it is otherwise
 * ordinary file-static state that UT_ResetState() knows nothing about.
 */
void UT_Stub_ResetTLS(void)
{
    memset(UT_Stub_TLSArray, 0, sizeof(UT_Stub_TLSArray));
}
