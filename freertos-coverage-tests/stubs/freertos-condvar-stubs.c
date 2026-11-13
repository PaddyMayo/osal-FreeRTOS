/*
 * File: freertos-condvar-stubs.c
 *
 * Purpose:
 *   UT-controllable stub implementations of the fake FreeRTOS mutex and task
 *   notification API (fake-inc/semphr.h, fake-inc/task.h), used to white-box
 *   test freertos/src/os-impl-condvar.c without a real FreeRTOS kernel. Follows
 *   the same pattern as freertos-mutex-stubs.c: UT_DEFAULT_IMPL() lets a test
 *   script a failure via UT_SetDeferredRetcode() (a negative value fails a
 *   take or a give), otherwise success is the default. Arguments worth
 *   asserting on are captured into the extern globals below.
 */

#include "utstubs.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "osapi-error.h"
#include "osapi-clock.h"

#include "os-freertos.h"

/*
 * The task death registry lives in os-impl-common.c and is covered there. The
 * hook os-impl-condvar.c registers is kept, so a test can run it; a negative
 * retcode makes the registration fail, as a full registry does.
 */
OS_FreeRTOS_TaskDiedHook_t UT_Stub_TaskDiedHook;

int32 OS_FreeRTOS_OnTaskDied(OS_FreeRTOS_TaskDiedHook_t hook)
{
    int32 status;

    UT_Stub_TaskDiedHook = hook;

    status = UT_DEFAULT_IMPL(OS_FreeRTOS_OnTaskDied);

    if (status < 0)
    {
        return OS_ERROR;
    }

    return OS_SUCCESS;
}

/* Handle returned by a successful xSemaphoreCreateMutex() */
SemaphoreHandle_t UT_Stub_CreatedCondVarMutex = (SemaphoreHandle_t)0x3300;

SemaphoreHandle_t UT_Stub_LastDeletedMutex;

SemaphoreHandle_t UT_Stub_LastTakeMutex;
TickType_t        UT_Stub_LastTakeMutexTicks;

SemaphoreHandle_t UT_Stub_LastGiveMutex;

/* The task that is running, as xTaskGetCurrentTaskHandle() reports it */
TaskHandle_t UT_Stub_CurrentTask = (TaskHandle_t)0x7001;

TaskHandle_t xTaskGetCurrentTaskHandle(void)
{
    UT_DEFAULT_IMPL(xTaskGetCurrentTaskHandle);

    return UT_Stub_CurrentTask;
}

/* The task holding each mutex, as xSemaphoreGetMutexHolder() reports it; a mutex not listed is free */
#define UT_STUB_MAX_HOLDERS 4

SemaphoreHandle_t UT_Stub_LastHolderQueried;

static SemaphoreHandle_t UT_Stub_HolderHandles[UT_STUB_MAX_HOLDERS];
static TaskHandle_t      UT_Stub_HolderTasks[UT_STUB_MAX_HOLDERS];

void UT_Stub_SetMutexHolder(SemaphoreHandle_t handle, TaskHandle_t holder)
{
    uint32 i;

    for (i = 0; i < UT_STUB_MAX_HOLDERS - 1; ++i)
    {
        if (UT_Stub_HolderHandles[i] == handle || UT_Stub_HolderHandles[i] == NULL)
        {
            break;
        }
    }

    UT_Stub_HolderHandles[i] = handle;
    UT_Stub_HolderTasks[i]   = holder;
}

void UT_Stub_ResetMutexHolders(void)
{
    uint32 i;

    for (i = 0; i < UT_STUB_MAX_HOLDERS; ++i)
    {
        UT_Stub_HolderHandles[i] = NULL;
        UT_Stub_HolderTasks[i]   = NULL;
    }
}

SemaphoreHandle_t xSemaphoreCreateMutex(void)
{
    int32 status;

    status = UT_DEFAULT_IMPL(xSemaphoreCreateMutex);

    if (status < 0)
    {
        return NULL;
    }

    return UT_Stub_CreatedCondVarMutex;
}

void vSemaphoreDelete(SemaphoreHandle_t xSemaphore)
{
    UT_Stub_LastDeletedMutex = xSemaphore;

    UT_DEFAULT_IMPL(vSemaphoreDelete);
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xBlockTime)
{
    int32 status;

    UT_Stub_LastTakeMutex      = xSemaphore;
    UT_Stub_LastTakeMutexTicks = xBlockTime;

    status = UT_DEFAULT_IMPL(xSemaphoreTake);

    if (status < 0)
    {
        return pdFALSE;
    }

    return pdTRUE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore)
{
    int32 status;

    UT_Stub_LastGiveMutex = xSemaphore;

    status = UT_DEFAULT_IMPL(xSemaphoreGive);

    if (status < 0)
    {
        return pdFALSE;
    }

    return pdTRUE;
}

TaskHandle_t xSemaphoreGetMutexHolder(SemaphoreHandle_t xSemaphore)
{
    uint32 i;

    UT_Stub_LastHolderQueried = xSemaphore;

    UT_DEFAULT_IMPL(xSemaphoreGetMutexHolder);

    for (i = 0; i < UT_STUB_MAX_HOLDERS; ++i)
    {
        if (UT_Stub_HolderHandles[i] == xSemaphore)
        {
            return UT_Stub_HolderTasks[i];
        }
    }

    return NULL;
}

/* The priority of each task, as uxTaskPriorityGet() reports it; a task not listed has priority 0 */
#define UT_STUB_MAX_PRIORITIES 8

static TaskHandle_t UT_Stub_PriorityTasks[UT_STUB_MAX_PRIORITIES];
static UBaseType_t  UT_Stub_Priorities[UT_STUB_MAX_PRIORITIES];

void UT_Stub_SetTaskPriority(TaskHandle_t task, UBaseType_t priority)
{
    uint32 i;

    for (i = 0; i < UT_STUB_MAX_PRIORITIES - 1; ++i)
    {
        if (UT_Stub_PriorityTasks[i] == task || UT_Stub_PriorityTasks[i] == NULL)
        {
            break;
        }
    }

    UT_Stub_PriorityTasks[i] = task;
    UT_Stub_Priorities[i]    = priority;
}

void UT_Stub_ResetTaskPriorities(void)
{
    uint32 i;

    for (i = 0; i < UT_STUB_MAX_PRIORITIES; ++i)
    {
        UT_Stub_PriorityTasks[i] = NULL;
        UT_Stub_Priorities[i]    = 0;
    }
}

UBaseType_t uxTaskPriorityGet(const TaskHandle_t xTask)
{
    uint32 i;

    UT_DEFAULT_IMPL(uxTaskPriorityGet);

    for (i = 0; i < UT_STUB_MAX_PRIORITIES; ++i)
    {
        if (UT_Stub_PriorityTasks[i] == xTask)
        {
            return UT_Stub_Priorities[i];
        }
    }

    return 0;
}

/*
 * The wait: what it was asked for is captured, and it returns the notification
 * count that a test scripts with UT_SetDefaultReturnValue() or a hook, 0 (the
 * wait timed out) by default.
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

/* Every task notified, in order, and the index of the last */
#define UT_STUB_MAX_NOTIFIED 8

TaskHandle_t UT_Stub_NotifiedTasks[UT_STUB_MAX_NOTIFIED];
uint32       UT_Stub_NotifiedCount;
UBaseType_t  UT_Stub_LastNotifyIndex;

void UT_Stub_ResetNotifyLog(void)
{
    uint32 i;

    for (i = 0; i < UT_STUB_MAX_NOTIFIED; ++i)
    {
        UT_Stub_NotifiedTasks[i] = NULL;
    }

    UT_Stub_NotifiedCount   = 0;
    UT_Stub_LastNotifyIndex = 0;
}

BaseType_t xTaskNotifyGiveIndexed(TaskHandle_t xTaskToNotify, UBaseType_t uxIndexToNotify)
{
    if (UT_Stub_NotifiedCount < UT_STUB_MAX_NOTIFIED)
    {
        UT_Stub_NotifiedTasks[UT_Stub_NotifiedCount] = xTaskToNotify;
    }

    ++UT_Stub_NotifiedCount;
    UT_Stub_LastNotifyIndex = uxIndexToNotify;

    UT_DEFAULT_IMPL(xTaskNotifyGiveIndexed);

    return pdPASS;
}

/*
 * os-impl-condvar.c converts the time left of a timed wait through
 * OS_FreeRTOS_MsToTicks(), which lives in os-impl-tasks.c and is covered there.
 * This runner links only the condvar module, so it gets a stand-in that records
 * the input and returns a marker, letting the test check the call is made and
 * its result is used.
 */
uint32     UT_Stub_LastMsToTicksInput;
TickType_t UT_Stub_MsToTicksResult = 0x1234;

TickType_t OS_FreeRTOS_MsToTicks(uint32 millisecond)
{
    UT_Stub_LastMsToTicksInput = millisecond;

    UT_DEFAULT_IMPL(OS_FreeRTOS_MsToTicks);

    return UT_Stub_MsToTicksResult;
}

/*
 * The clock a timed wait reads, which is OSAL's own: the port has no clock of
 * its own yet. A test sets UT_Stub_LocalTime, or makes the read fail with a
 * negative retcode.
 */
OS_time_t UT_Stub_LocalTime;

int32 OS_GetLocalTime(OS_time_t *time_struct)
{
    int32 status;

    status = UT_DEFAULT_IMPL(OS_GetLocalTime);

    if (status < 0)
    {
        return OS_ERROR;
    }

    *time_struct = UT_Stub_LocalTime;

    return OS_SUCCESS;
}

/*
 * The gate functions and the task API that os-impl-condvar.c uses around them.
 * The gate lives in os-impl-common.c and is covered there, and this runner
 * links only the condvar module, so it gets stand-ins that record their
 * arguments. A test makes the gate refuse (OS_FreeRTOS_GateEnter fails with a
 * negative retcode) or report it closed (a non-zero default return from
 * OS_FreeRTOS_GateIsClosed).
 */
TickType_t            UT_Stub_CondVarLastDelayTicks;
const OS_impl_gate_t *UT_Stub_LastGate;

// cppcheck-suppress constParameterCallback
bool OS_FreeRTOS_GateEnter(OS_impl_gate_t *gate, OS_impl_gate_user_t *user)
{
    int32 status;

    (void)user;

    UT_Stub_LastGate = gate;

    status = UT_DEFAULT_IMPL(OS_FreeRTOS_GateEnter);

    return (status >= 0);
}

bool OS_FreeRTOS_GateLeave(const OS_impl_gate_t *gate, const OS_impl_gate_user_t *user)
{
    int32 status;

    (void)user;

    UT_Stub_LastGate = gate;

    status = UT_DEFAULT_IMPL(OS_FreeRTOS_GateLeave);

    return (status != 0);
}

// cppcheck-suppress constParameterCallback
void OS_FreeRTOS_GateOpen(OS_impl_gate_t *gate)
{
    UT_Stub_LastGate = gate;

    UT_DEFAULT_IMPL(OS_FreeRTOS_GateOpen);
}

// cppcheck-suppress constParameterCallback
void OS_FreeRTOS_GateClose(OS_impl_gate_t *gate)
{
    UT_Stub_LastGate = gate;

    UT_DEFAULT_IMPL(OS_FreeRTOS_GateClose);
}

bool OS_FreeRTOS_GateIsClosed(const OS_impl_gate_t *gate)
{
    int32 status;

    UT_Stub_LastGate = gate;

    status = UT_DEFAULT_IMPL(OS_FreeRTOS_GateIsClosed);

    return (status != 0);
}

uint32 OS_FreeRTOS_GateWake(const OS_impl_gate_t *gate)
{
    int32 status;

    UT_Stub_LastGate = gate;

    status = UT_DEFAULT_IMPL(OS_FreeRTOS_GateWake);

    return (uint32)status;
}

void vTaskDelay(TickType_t xTicksToDelay)
{
    UT_Stub_CondVarLastDelayTicks = xTicksToDelay;

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
