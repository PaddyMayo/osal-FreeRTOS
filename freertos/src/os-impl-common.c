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

/* The task death hooks and the entry hooks, each in registration order; see os-freertos.h */
static OS_FreeRTOS_TaskDiedHook_t OS_FreeRTOS_TaskDiedHooks[OS_FREERTOS_TASK_DIED_HOOKS];
static uint32                     OS_FreeRTOS_TaskDiedHookCount;
static OS_FreeRTOS_EnterHook_t    OS_FreeRTOS_EnterHooks[OS_FREERTOS_ENTER_HOOKS];
static uint32                     OS_FreeRTOS_EnterHookCount;

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

    if (idtype == OS_OBJECT_TYPE_OS_MUTEX)
    {
        return OS_FreeRTOS_MutexInit();
    }

    if (idtype == OS_OBJECT_TYPE_OS_CONDVAR)
    {
        return OS_FreeRTOS_CondVarInit();
    }

    if (idtype != OS_OBJECT_TYPE_UNDEFINED)
    {
        return OS_SUCCESS;
    }

    /* OS_API_Init() calls this type first, so the hooks the later types register survive */
    OS_FreeRTOS_TaskDiedHookCount = 0;
    OS_FreeRTOS_EnterHookCount    = 0;

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

/* Every call in progress on a gated object, newest first; see os-freertos.h */
static OS_impl_gate_user_t *OS_FreeRTOS_GateUsers;

/*----------------------------------------------------------------
 *
 *  Purpose: Port-internal helper routine, not part of OSAL API.
 *           Lists a caller among the users of a gated object. See
 *           prototype in os-freertos.h.
 *
 *-----------------------------------------------------------------*/
bool OS_FreeRTOS_GateEnter(OS_impl_gate_t *gate, OS_impl_gate_user_t *user)
{
    bool admitted;

    /* Every wait on a kernel object starts here: where the modules' entry hooks run */
    OS_FreeRTOS_Enter();

    taskENTER_CRITICAL();
    admitted = !gate->closed;
    if (admitted)
    {
        user->task            = xTaskGetCurrentTaskHandle();
        user->gate            = gate;
        user->next            = OS_FreeRTOS_GateUsers;
        OS_FreeRTOS_GateUsers = user;
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
bool OS_FreeRTOS_GateLeave(const OS_impl_gate_t *gate, const OS_impl_gate_user_t *user)
{
    bool closed;

    taskENTER_CRITICAL();
    {
        /* The list is singly linked: find the slot that points at the node, so the head needs no special case */
        OS_impl_gate_user_t **link = &OS_FreeRTOS_GateUsers;

        while (*link != user)
        {
            link = &(*link)->next;
        }

        *link = user->next;
    }

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
 *           critical section that keeps the nodes (which live on those
 *           tasks' stacks) from vanishing; it does nothing to a task that
 *           is not blocked. See prototype in os-freertos.h.
 *
 *-----------------------------------------------------------------*/
uint32 OS_FreeRTOS_GateWake(const OS_impl_gate_t *gate)
{
    const OS_impl_gate_user_t *user;
    uint32                     users;

    users = 0;

    taskENTER_CRITICAL();
    for (user = OS_FreeRTOS_GateUsers; user != NULL; user = user->next)
    {
        if (user->gate == gate)
        {
            (void)xTaskAbortDelay(user->task);
            ++users;
        }
    }
    taskEXIT_CRITICAL();

    return users;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *           Drops the nodes of a task about to be deleted, from every gate;
 *           the first thing OS_FreeRTOS_TaskDied() does.
 *
 *-----------------------------------------------------------------*/
static void OS_FreeRTOS_GateReleaseTask(TaskHandle_t task)
{
    OS_impl_gate_user_t **link;

    taskENTER_CRITICAL();
    link = &OS_FreeRTOS_GateUsers;
    while (*link != NULL)
    {
        if ((*link)->task == task)
        {
            *link = (*link)->next;
        }
        else
        {
            link = &(*link)->next;
        }
    }
    taskEXIT_CRITICAL();
}

/*----------------------------------------------------------------
 *
 *  Purpose: Port-internal helper routine, not part of OSAL API.
 *           See prototype in os-freertos.h.
 *
 *-----------------------------------------------------------------*/
int32 OS_FreeRTOS_OnTaskDied(OS_FreeRTOS_TaskDiedHook_t hook)
{
    if (OS_FreeRTOS_TaskDiedHookCount >= OS_FREERTOS_TASK_DIED_HOOKS)
    {
        return OS_ERROR;
    }

    OS_FreeRTOS_TaskDiedHooks[OS_FreeRTOS_TaskDiedHookCount] = hook;
    ++OS_FreeRTOS_TaskDiedHookCount;

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Port-internal helper routine, not part of OSAL API.
 *           See prototype in os-freertos.h.
 *
 *-----------------------------------------------------------------*/
void OS_FreeRTOS_TaskDied(TaskHandle_t task)
{
    uint32 i;

    OS_FreeRTOS_GateReleaseTask(task);

    for (i = 0; i < OS_FreeRTOS_TaskDiedHookCount; ++i)
    {
        OS_FreeRTOS_TaskDiedHooks[i](task);
    }
}

/*----------------------------------------------------------------
 *
 *  Purpose: Port-internal helper routine, not part of OSAL API.
 *           See prototype in os-freertos.h.
 *
 *-----------------------------------------------------------------*/
int32 OS_FreeRTOS_OnEnter(OS_FreeRTOS_EnterHook_t hook)
{
    if (OS_FreeRTOS_EnterHookCount >= OS_FREERTOS_ENTER_HOOKS)
    {
        return OS_ERROR;
    }

    OS_FreeRTOS_EnterHooks[OS_FreeRTOS_EnterHookCount] = hook;
    ++OS_FreeRTOS_EnterHookCount;

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Port-internal helper routine, not part of OSAL API.
 *           See prototype in os-freertos.h.
 *
 *-----------------------------------------------------------------*/
void OS_FreeRTOS_Enter(void)
{
    uint32 i;

    for (i = 0; i < OS_FreeRTOS_EnterHookCount; ++i)
    {
        OS_FreeRTOS_EnterHooks[i]();
    }
}
