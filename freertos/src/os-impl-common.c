/**
 * \file
 *
 * Purpose: OSAL common/global implementation on top of FreeRTOS.
 *
 *          OS_API_Impl_Init() must return OS_SUCCESS for every object type:
 *          OS_API_Init() (third_party/osal/src/os/shared/src/osapi-common.c)
 *          stops its init loop at the first non-success, so anything else
 *          leaves every later subsystem uninitialized. Modules that are still
 *          stubs fail cleanly at their own _Impl calls instead.
 *
 *          The idle loop blocks on a statically allocated binary semaphore
 *          that OS_ApplicationShutdown_Impl() gives. A semaphore (rather than
 *          a task notification) keeps the wakeup even when shutdown runs
 *          before the idle task has blocked, so no wakeup can be lost.
 */

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "os-shared-common.h"
#include "os-shared-idmap.h"

#include "os-freertos.h"

static StaticSemaphore_t OS_FreeRTOS_IdleSemBuffer;
static SemaphoreHandle_t OS_FreeRTOS_IdleSem;

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_API_Impl_Init(osal_objtype_t idtype)
{
    if (idtype == OS_OBJECT_TYPE_OS_TASK)
    {
        return OS_FreeRTOS_TaskInit();
    }

    if (idtype == OS_OBJECT_TYPE_OS_STREAM)
    {
        return OS_FreeRTOS_FileInit();
    }

    if (idtype == OS_OBJECT_TYPE_OS_FILESYS)
    {
        return OS_FreeRTOS_FileSysInit();
    }

    if (idtype != OS_OBJECT_TYPE_UNDEFINED)
    {
        return OS_SUCCESS;
    }

    OS_SharedGlobalVars.TicksPerSecond  = configTICK_RATE_HZ;
    OS_SharedGlobalVars.MicroSecPerTick = 1000000 / configTICK_RATE_HZ;

    OS_FreeRTOS_IdleSem = xSemaphoreCreateBinaryStatic(&OS_FreeRTOS_IdleSemBuffer);
    if (OS_FreeRTOS_IdleSem == NULL)
    {
        return OS_ERROR;
    }

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
void OS_IdleLoop_Impl(void)
{
    (void)xSemaphoreTake(OS_FreeRTOS_IdleSem, portMAX_DELAY);
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
void OS_ApplicationShutdown_Impl(void)
{
    (void)xSemaphoreGive(OS_FreeRTOS_IdleSem);
}

/*----------------------------------------------------------------
 *
 *  Purpose: Port-internal helper routine, not part of OSAL API.
 *           Counts a caller among the users of a gated object. See
 *           prototype in os-freertos.h.
 *
 *-----------------------------------------------------------------*/
bool OS_FreeRTOS_GateEnter(OS_impl_gate_t *gate, OS_impl_gate_waiter_t *waiter)
{
    bool admitted;

    taskENTER_CRITICAL();
    admitted = !gate->closed;
    if (admitted)
    {
        ++gate->users;

        if (waiter != NULL)
        {
            waiter->task  = xTaskGetCurrentTaskHandle();
            waiter->next  = gate->waiters;
            gate->waiters = waiter;
        }
    }
    taskEXIT_CRITICAL();

    return admitted;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Port-internal helper routine, not part of OSAL API.
 *           Undoes OS_FreeRTOS_GateEnter(). See prototype in
 *           os-freertos.h.
 *
 *-----------------------------------------------------------------*/
bool OS_FreeRTOS_GateLeave(OS_impl_gate_t *gate, const OS_impl_gate_waiter_t *waiter)
{
    bool closed;

    taskENTER_CRITICAL();
    if (waiter != NULL)
    {
        /* The list is singly linked: find the slot that points at the node, so the head needs no special case */
        OS_impl_gate_waiter_t **link = &gate->waiters;

        while (*link != waiter)
        {
            link = &(*link)->next;
        }

        *link = waiter->next;
    }

    --gate->users;
    closed = gate->closed;
    taskEXIT_CRITICAL();

    return closed;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Port-internal helper routine, not part of OSAL API.
 *           See prototype in os-freertos.h.
 *
 *-----------------------------------------------------------------*/
void OS_FreeRTOS_GateOpen(OS_impl_gate_t *gate)
{
    taskENTER_CRITICAL();
    gate->closed = false;
    taskEXIT_CRITICAL();
}

/*----------------------------------------------------------------
 *
 *  Purpose: Port-internal helper routine, not part of OSAL API.
 *           See prototype in os-freertos.h.
 *
 *-----------------------------------------------------------------*/
void OS_FreeRTOS_GateClose(OS_impl_gate_t *gate)
{
    taskENTER_CRITICAL();
    gate->closed = true;
    taskEXIT_CRITICAL();
}

/*----------------------------------------------------------------
 *
 *  Purpose: Port-internal helper routine, not part of OSAL API.
 *           See prototype in os-freertos.h.
 *
 *-----------------------------------------------------------------*/
bool OS_FreeRTOS_GateIsClosed(const OS_impl_gate_t *gate)
{
    bool closed;

    taskENTER_CRITICAL();
    closed = gate->closed;
    taskEXIT_CRITICAL();

    return closed;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Port-internal helper routine, not part of OSAL API.
 *           Wakes the tasks blocked on a gated object. xTaskAbortDelay()
 *           only unblocks a task, it never blocks, so it is safe inside the
 *           critical section that keeps the waiter nodes (which live on
 *           those tasks' stacks) from vanishing. See prototype in
 *           os-freertos.h.
 *
 *-----------------------------------------------------------------*/
uint32 OS_FreeRTOS_GateWake(const OS_impl_gate_t *gate)
{
    const OS_impl_gate_waiter_t *waiter;
    uint32                       users;

    taskENTER_CRITICAL();
    for (waiter = gate->waiters; waiter != NULL; waiter = waiter->next)
    {
        (void)xTaskAbortDelay(waiter->task);
    }

    users = gate->users;
    taskEXIT_CRITICAL();

    return users;
}
