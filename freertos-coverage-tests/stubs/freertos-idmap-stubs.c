/*
 * File: freertos-idmap-stubs.c
 *
 * Purpose:
 *   UT-controllable stand-ins for the FreeRTOS task API, and for
 *   the port functions in other modules, that freertos/src/os-impl-idmap.c
 *   calls, used to white-box test it without a real FreeRTOS kernel. Each
 *   follows the standard ut_assert pattern: UT_DEFAULT_IMPL() lets a test
 *   script a failure through UT_SetDeferredRetcode(), otherwise success is the
 *   default. Arguments worth asserting on are captured into the extern globals
 *   below.
 */

#include "utstubs.h"

#include "FreeRTOS.h"
#include "task.h"

#include "osapi-error.h"

#include "os-freertos.h"

#define UT_STUB_MAX_ABORTS 8

TaskHandle_t UT_Stub_IdmapCurrentTask = (TaskHandle_t)0x7000;

/* Every task xTaskAbortDelay() was called with, in order */
TaskHandle_t UT_Stub_IdmapAborted[UT_STUB_MAX_ABORTS];
uint32       UT_Stub_IdmapAbortedCount;

TickType_t UT_Stub_IdmapLastDelayTicks;

/* The hook OS_FreeRTOS_OnTaskDied() was last given */
OS_FreeRTOS_TaskDiedHook_t UT_Stub_IdmapDiedHook;

/*
 * The milliseconds OS_FreeRTOS_MsToTicks() was last asked for, and the marker it
 * returns, so a test can tell the delay uses its result.
 */
uint32     UT_Stub_IdmapLastMs;
TickType_t UT_Stub_IdmapMsToTicksResult = 0x1234;

TaskHandle_t xTaskGetCurrentTaskHandle(void)
{
    UT_DEFAULT_IMPL(xTaskGetCurrentTaskHandle);

    return UT_Stub_IdmapCurrentTask;
}

BaseType_t xTaskAbortDelay(TaskHandle_t xTask)
{
    UT_Stub_IdmapAborted[UT_Stub_IdmapAbortedCount % UT_STUB_MAX_ABORTS] = xTask;
    ++UT_Stub_IdmapAbortedCount;

    UT_DEFAULT_IMPL(xTaskAbortDelay);

    return pdPASS;
}

void vTaskDelay(TickType_t xTicksToDelay)
{
    UT_Stub_IdmapLastDelayTicks = xTicksToDelay;

    UT_DEFAULT_IMPL(vTaskDelay);
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
 * os-impl-idmap.c calls these, but they live in os-impl-tasks.c and
 * os-impl-common.c and are covered there. This runner links only the id map
 * module, so each gets a stand-in.
 */
TickType_t OS_FreeRTOS_MsToTicks(uint32 millisecond)
{
    UT_Stub_IdmapLastMs = millisecond;

    UT_DEFAULT_IMPL(OS_FreeRTOS_MsToTicks);

    return UT_Stub_IdmapMsToTicksResult;
}

void OS_FreeRTOS_Enter(void)
{
    UT_DEFAULT_IMPL(OS_FreeRTOS_Enter);
}

int32 OS_FreeRTOS_OnTaskDied(OS_FreeRTOS_TaskDiedHook_t hook)
{
    int32 status;

    UT_Stub_IdmapDiedHook = hook;

    status = UT_DEFAULT_IMPL(OS_FreeRTOS_OnTaskDied);

    if (status < 0)
    {
        return OS_ERROR;
    }

    return OS_SUCCESS;
}
