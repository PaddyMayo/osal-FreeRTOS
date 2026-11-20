/*
 * File: freertos-timebase-stubs.c
 *
 * Purpose:
 *   UT-controllable stub implementations of the fake FreeRTOS task API
 *   (fake-inc/task.h), used to white-box test freertos/src/os-impl-timebase.c
 *   without a real FreeRTOS kernel. Follows the same pattern as
 *   freertos-tasks-stubs.c: UT_DEFAULT_IMPL() lets a test script a failure via
 *   UT_SetDeferredRetcode(), otherwise success is the default. Arguments worth
 *   asserting on are captured into the extern globals below.
 *
 *   The shared layer's tables and functions come from OSAL's own stub libraries
 *   (see FREERTOS_OSAL_STUB_MODULES in ../CMakeLists.txt).
 */

#include "utstubs.h"

#include "FreeRTOS.h"
#include "task.h"

#include "os-impl-tasks.h"

TaskFunction_t         UT_Stub_LastCreateTaskCode;
const char            *UT_Stub_LastCreateName;
configSTACK_DEPTH_TYPE UT_Stub_LastCreateStackDepth;
void                  *UT_Stub_LastCreateParameters;
UBaseType_t            UT_Stub_LastCreatePriority;
StackType_t           *UT_Stub_LastCreateStackBuffer;
StaticTask_t          *UT_Stub_LastCreateTaskBuffer;

TaskHandle_t xTaskCreateStatic(TaskFunction_t               pxTaskCode,
                               const char *const            pcName,
                               const configSTACK_DEPTH_TYPE ulStackDepth,
                               void *const                  pvParameters,
                               UBaseType_t                  uxPriority,
                               StackType_t *const           puxStackBuffer,
                               StaticTask_t *const          pxTaskBuffer)
{
    int32 status;

    UT_Stub_LastCreateTaskCode    = pxTaskCode;
    UT_Stub_LastCreateName        = pcName;
    UT_Stub_LastCreateStackDepth  = ulStackDepth;
    UT_Stub_LastCreateParameters  = pvParameters;
    UT_Stub_LastCreatePriority    = uxPriority;
    UT_Stub_LastCreateStackBuffer = puxStackBuffer;
    UT_Stub_LastCreateTaskBuffer  = pxTaskBuffer;

    status = UT_DEFAULT_IMPL(xTaskCreateStatic);

    if (status < 0)
    {
        return NULL;
    }

    /* Real FreeRTOS also derives the handle from the caller-supplied TCB buffer */
    return (TaskHandle_t)pxTaskBuffer;
}

TaskHandle_t UT_Stub_LastDeletedHandle;

void vTaskDelete(TaskHandle_t xTaskToDelete)
{
    UT_Stub_LastDeletedHandle = xTaskToDelete;

    UT_DEFAULT_IMPL(vTaskDelete);
}

/* The kernel tick count, which a test sets to move time on */
TickType_t UT_Stub_TickCount;

TickType_t xTaskGetTickCount(void)
{
    UT_DEFAULT_IMPL(xTaskGetTickCount);

    return UT_Stub_TickCount;
}

/*
 * The wait on the task's notification: what it was asked for is captured, and it returns at once, with
 * the count that a test scripts with UT_SetDeferredRetcode() or a hook: zero, a wait that ran out of time, by default
 */
UBaseType_t UT_Stub_LastNotifyTakeIndex;
BaseType_t  UT_Stub_LastNotifyTakeClear;
TickType_t  UT_Stub_LastNotifyTakeTicks;

unsigned long
ulTaskNotifyTakeIndexed(UBaseType_t uxIndexToWaitOn, BaseType_t xClearCountOnExit, TickType_t xTicksToWait)
{
    UT_Stub_LastNotifyTakeIndex = uxIndexToWaitOn;
    UT_Stub_LastNotifyTakeClear = xClearCountOnExit;
    UT_Stub_LastNotifyTakeTicks = xTicksToWait;

    return (unsigned long)UT_DEFAULT_IMPL(ulTaskNotifyTakeIndexed);
}

/* The notification given to wake a task: which task, and on which index */
TaskHandle_t UT_Stub_LastNotifiedTask;
UBaseType_t  UT_Stub_LastNotifyGiveIndex;

BaseType_t xTaskNotifyGiveIndexed(TaskHandle_t xTaskToNotify, UBaseType_t uxIndexToNotify)
{
    UT_Stub_LastNotifiedTask    = xTaskToNotify;
    UT_Stub_LastNotifyGiveIndex = uxIndexToNotify;

    UT_DEFAULT_IMPL(xTaskNotifyGiveIndexed);

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
 * os-impl-timebase.c gives its task the highest priority an OSAL task has, by
 * remapping OSAL priority 0 with this, which lives in os-impl-tasks.c and is
 * covered there. This runner links only the timebase module, so it gets a
 * stand-in that records its input and returns a marker, letting the test check
 * that the call is made and its result is used.
 */
osal_priority_t UT_Stub_LastRemapInput;
UBaseType_t     UT_Stub_RemapResult = 6;

UBaseType_t OS_FreeRTOS_PriorityRemap(osal_priority_t priority)
{
    UT_Stub_LastRemapInput = priority;

    UT_DEFAULT_IMPL(OS_FreeRTOS_PriorityRemap);

    return UT_Stub_RemapResult;
}
