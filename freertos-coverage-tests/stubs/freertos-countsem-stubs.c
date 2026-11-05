/*
 * File: freertos-countsem-stubs.c
 *
 * Purpose:
 *   UT-controllable stub implementations of the fake FreeRTOS counting
 *   semaphore API (fake-inc/semphr.h), used to white-box test
 *   freertos/src/os-impl-countsem.c without a real FreeRTOS kernel. Follows
 *   the same pattern as freertos-binsem-stubs.c: UT_DEFAULT_IMPL() lets a test
 *   script a failure via UT_SetDeferredRetcode() (a negative value fails the
 *   take or give), otherwise success is the default. Arguments worth asserting
 *   on are captured into the extern globals below.
 */

#include "utstubs.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "os-freertos.h"

/* Handle returned by a successful xSemaphoreCreateCounting() */
SemaphoreHandle_t UT_Stub_CreatedCountSemHandle = (SemaphoreHandle_t)0x3200;

UBaseType_t UT_Stub_LastCreateMaxCount;
UBaseType_t UT_Stub_LastCreateInitialCount;

SemaphoreHandle_t UT_Stub_LastDeletedCountSem;

SemaphoreHandle_t UT_Stub_LastTakeCountSem;
TickType_t        UT_Stub_LastTakeCountSemTicks;

SemaphoreHandle_t UT_Stub_LastGiveCountSem;

SemaphoreHandle_t UT_Stub_LastCountCountSem;

/*
 * os-impl-countsem.c converts its timeout through OS_FreeRTOS_MsToTicks(),
 * which lives in os-impl-tasks.c and is covered there. This runner links only
 * the countsem module, so it gets a stand-in that records the input and
 * returns a marker, letting the test check the call is made and its result is
 * used.
 */
uint32     UT_Stub_LastMsToTicksInput;
TickType_t UT_Stub_MsToTicksResult = 0x1234;

TickType_t OS_FreeRTOS_MsToTicks(uint32 millisecond)
{
    UT_Stub_LastMsToTicksInput = millisecond;

    UT_DEFAULT_IMPL(OS_FreeRTOS_MsToTicks);

    return UT_Stub_MsToTicksResult;
}

SemaphoreHandle_t xSemaphoreCreateCounting(UBaseType_t uxMaxCount, UBaseType_t uxInitialCount)
{
    int32 status;

    UT_Stub_LastCreateMaxCount     = uxMaxCount;
    UT_Stub_LastCreateInitialCount = uxInitialCount;

    status = UT_DEFAULT_IMPL(xSemaphoreCreateCounting);

    if (status < 0)
    {
        return NULL;
    }

    return UT_Stub_CreatedCountSemHandle;
}

void vSemaphoreDelete(SemaphoreHandle_t xSemaphore)
{
    UT_Stub_LastDeletedCountSem = xSemaphore;

    UT_DEFAULT_IMPL(vSemaphoreDelete);
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xBlockTime)
{
    int32 status;

    UT_Stub_LastTakeCountSem      = xSemaphore;
    UT_Stub_LastTakeCountSemTicks = xBlockTime;

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

    UT_Stub_LastGiveCountSem = xSemaphore;

    status = UT_DEFAULT_IMPL(xSemaphoreGive);

    if (status < 0)
    {
        return pdFALSE;
    }

    return pdTRUE;
}

/* The count a test scripts with UT_SetDefaultReturnValue() */
UBaseType_t uxSemaphoreGetCount(SemaphoreHandle_t xSemaphore)
{
    UT_Stub_LastCountCountSem = xSemaphore;

    return (UBaseType_t)UT_DEFAULT_IMPL(uxSemaphoreGetCount);
}

/*
 * The gate functions and the task API that os-impl-countsem.c uses around
 * them. The gate lives in os-impl-common.c and is covered there, and this
 * runner links only the countsem module, so it gets stand-ins that record
 * their arguments. A test makes the gate refuse (OS_FreeRTOS_GateEnter fails
 * with a negative retcode) or report it closed when left (a non-zero default
 * return from OS_FreeRTOS_GateLeave).
 */
TickType_t                 UT_Stub_CountSemLastDelayTicks;
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

uint32 OS_FreeRTOS_GateWake(const OS_impl_gate_t *gate)
{
    int32 status;

    UT_Stub_LastGate = gate;

    status = UT_DEFAULT_IMPL(OS_FreeRTOS_GateWake);

    return (uint32)status;
}

void vTaskDelay(TickType_t xTicksToDelay)
{
    UT_Stub_CountSemLastDelayTicks = xTicksToDelay;

    UT_DEFAULT_IMPL(vTaskDelay);
}
