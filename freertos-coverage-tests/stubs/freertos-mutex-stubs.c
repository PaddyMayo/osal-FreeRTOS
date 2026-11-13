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
#include "task.h"
#include "semphr.h"

#include "osapi-error.h"

#include "os-freertos.h"

/*
 * The task death registry lives in os-impl-common.c and is covered there. The
 * hook os-impl-mutex.c registers is kept, so a test can run it; a negative
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

/* Handle returned by a successful xSemaphoreCreateRecursiveMutex() */
SemaphoreHandle_t UT_Stub_CreatedMutexHandle = (SemaphoreHandle_t)0x3000;

SemaphoreHandle_t UT_Stub_LastDeletedMutex;

SemaphoreHandle_t UT_Stub_LastTakeMutex;
TickType_t        UT_Stub_LastTakeMutexTicks;

SemaphoreHandle_t UT_Stub_LastGiveMutex;

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

/*
 * The gate functions and the task API that os-impl-mutex.c uses around them.
 * The gate lives in os-impl-common.c and is covered there, and this runner
 * links only the mutex module, so it gets stand-ins that record their
 * arguments. A test makes the gate refuse (OS_FreeRTOS_GateEnter fails with a
 * negative retcode) or report it closed (a non-zero default return from
 * OS_FreeRTOS_GateIsClosed).
 */
TickType_t                 UT_Stub_MutexLastDelayTicks;
const OS_impl_gate_t      *UT_Stub_LastGate;
OS_impl_gate_user_t       *UT_Stub_LastEnterUser;
const OS_impl_gate_user_t *UT_Stub_LastLeaveUser;

// cppcheck-suppress constParameterCallback
bool OS_FreeRTOS_GateEnter(OS_impl_gate_t *gate, OS_impl_gate_user_t *user)
{
    int32 status;

    UT_Stub_LastGate      = gate;
    UT_Stub_LastEnterUser = user;

    status = UT_DEFAULT_IMPL(OS_FreeRTOS_GateEnter);

    return (status >= 0);
}

bool OS_FreeRTOS_GateLeave(const OS_impl_gate_t *gate, const OS_impl_gate_user_t *user)
{
    int32 status;

    UT_Stub_LastGate      = gate;
    UT_Stub_LastLeaveUser = user;

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
    UT_Stub_MutexLastDelayTicks = xTicksToDelay;

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
