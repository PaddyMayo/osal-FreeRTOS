/**
 * \file
 *
 * Purpose: OSAL condition variable implementation on top of FreeRTOS task
 *          notifications, which FreeRTOS offers in place of a native one.
 *
 *          Each condition variable is a FreeRTOS mutex and a list of the tasks
 *          waiting on it. A task in OS_CondVarWait() puts a node for itself on
 *          the list, gives the mutex up and blocks on its notification, then
 *          takes the mutex back. A notification stays pending until it is taken,
 *          so a signal that lands between the task giving up the mutex and
 *          blocking is not lost: that is the window that the atomic release of
 *          the mutex is there to close.
 *
 *          Signal wakes the highest priority waiter (the longest waiting one
 *          among equals) and Broadcast all of them. A signal is done under the
 *          same critical section as the waiter's unlinking of itself, so each
 *          wait ends either signaled or not, never both: a wait that times out
 *          just as it is signaled counts as signaled, and the signal is not
 *          lost (see os-impl-condvar.h).
 *
 *          Notification index 1 is reserved for this (index 0 is left to
 *          application code), and every wait drains it before it returns, so a
 *          wake can never be left over for the next wait.
 *
 *          The calls reach the condition variable without raising the refcount
 *          that OS_CondVarDelete() waits on, so they pass through its gate
 *          (see os-freertos.h). Delete closes the gate, wakes the tasks that are
 *          blocked, which then return OS_ERR_INVALID_ID, and frees the mutex
 *          only once every user has left. A Delete refuses a condition variable
 *          whose mutex a live task holds, as OS_MutSemDelete() does.
 *
 *          A task deleted while it waits leaves a node on a stack that goes, and
 *          one deleted while it holds the mutex can never give it back, so
 *          OS_FreeRTOS_CondVarTaskDied(), which OS_FreeRTOS_CondVarInit() registers
 *          for task death (see os-freertos.h), drops the node and orphans the
 *          mutex.
 *
 *          The mutex is not recursive, and the calls cannot be made from an
 *          interrupt: the gate is a critical section.
 */

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "os-shared-condvar.h"
#include "os-shared-idmap.h"

#include "os-freertos.h"
#include "os-impl-condvar.h"

OS_impl_condvar_internal_record_t OS_impl_condvar_table[OS_MAX_CONDVARS];

static void OS_FreeRTOS_CondVarTaskDied(TaskHandle_t task);

/*----------------------------------------------------------------
 *
 *  Purpose: Port-internal helper routine, not part of OSAL API.
 *           Registers OS_FreeRTOS_CondVarTaskDied() for task death. See
 *           prototype in os-freertos.h.
 *
 *-----------------------------------------------------------------*/
int32 OS_FreeRTOS_CondVarInit(void)
{
    return OS_FreeRTOS_OnTaskDied(OS_FreeRTOS_CondVarTaskDied);
}

/*----------------------------------------------------------------
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *           Wakes a waiter that has just been taken off the list. Must be
 *           called in the critical section it was taken off the list in: it
 *           is that which keeps the node, on the stack of the waiter, there.
 *
 *-----------------------------------------------------------------*/
static void OS_FreeRTOS_CondVarWake(OS_impl_condvar_waiter_t *waiter)
{
    waiter->next     = NULL;
    waiter->signaled = true;

    (void)xTaskNotifyGiveIndexed(waiter->task, OS_FREERTOS_CONDVAR_NOTIFY_INDEX);
}

/*----------------------------------------------------------------
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *           The wait of Wait and TimedWait, which differ only in how long
 *           they wait and in what the end of the wait means when nothing
 *           signaled: expired is what to return then.
 *
 *           The mutex is given up and the task blocks, and when the wait is
 *           over the mutex is taken again, however it ended - a caller can
 *           rely on holding it - unless the condition variable is deleted.
 *
 *-----------------------------------------------------------------*/
static int32 OS_FreeRTOS_CondVarBlock(const OS_object_token_t *token, TickType_t ticks, int32 expired)
{
    OS_impl_condvar_internal_record_t *impl;
    OS_impl_condvar_waiter_t           waiter;
    OS_impl_condvar_waiter_t         **link;
    OS_impl_gate_user_t                user;
    TaskHandle_t                       self;
    uint32                             notified;
    BaseType_t                         relocked;
    bool                               signaled;
    bool                               closed;
    int32                              status;

    impl = OS_OBJECT_TABLE_GET(OS_impl_condvar_table, *token);
    self = xTaskGetCurrentTaskHandle();

    if (!OS_FreeRTOS_GateEnter(&impl->gate, &user))
    {
        return OS_ERR_INVALID_ID;
    }

    /* Giving a mutex up that the caller does not hold is not something FreeRTOS survives, so check first */
    if (xSemaphoreGetMutexHolder(impl->mutex) != self)
    {
        (void)OS_FreeRTOS_GateLeave(&impl->gate, &user);
        return OS_ERROR;
    }

    waiter.task     = self;
    waiter.next     = NULL;
    waiter.signaled = false;

    taskENTER_CRITICAL();
    link = &impl->waiters;
    while (*link != NULL)
    {
        link = &(*link)->next;
    }
    *link = &waiter;
    taskEXIT_CRITICAL();

    /* On the list before the mutex goes, so a signal made once it is free finds this task */
    (void)xSemaphoreGive(impl->mutex);

    notified = ulTaskNotifyTakeIndexed(OS_FREERTOS_CONDVAR_NOTIFY_INDEX, pdTRUE, ticks);

    /*
     * Only a signal or a broadcast takes a node off the list, and a node of a
     * task that is running is not taken off by the death of a task, so one that
     * is not signaled is still on the list.
     */
    taskENTER_CRITICAL();
    signaled = waiter.signaled;
    if (!signaled)
    {
        link = &impl->waiters;
        while (*link != &waiter)
        {
            link = &(*link)->next;
        }
        *link = waiter.next;
    }
    taskEXIT_CRITICAL();

    if (signaled && (notified == 0))
    {
        /* The signal came after the wait gave up: its notification is still pending, so take it here, not later */
        (void)ulTaskNotifyTakeIndexed(OS_FREERTOS_CONDVAR_NOTIFY_INDEX, pdTRUE, 0);
    }

    /*
     * Take the mutex back, in the gate so that a delete can wake this. The only
     * thing that wakes a wait for the mutex early is the delete, which closes the
     * gate, so a failed take with the gate open is tried again.
     */
    do
    {
        relocked = xSemaphoreTake(impl->mutex, portMAX_DELAY);
        closed   = OS_FreeRTOS_GateIsClosed(&impl->gate);
    } while ((relocked != pdTRUE) && !closed);

    if (closed)
    {
        if (relocked == pdTRUE)
        {
            /* The delete is waiting for this task to leave, and cannot free a mutex that is held */
            (void)xSemaphoreGive(impl->mutex);
        }

        status = OS_ERR_INVALID_ID;
    }
    else if (signaled)
    {
        status = OS_SUCCESS;
    }
    else
    {
        status = expired;
    }

    (void)OS_FreeRTOS_GateLeave(&impl->gate, &user);

    return status;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_CondVarCreate_Impl(const OS_object_token_t *token, uint32 options)
{
    OS_impl_condvar_internal_record_t *impl;

    impl = OS_OBJECT_TABLE_GET(OS_impl_condvar_table, *token);

    impl->mutex = xSemaphoreCreateMutex();
    if (impl->mutex == NULL)
    {
        return OS_ERROR;
    }

    impl->waiters = NULL;
    OS_FreeRTOS_GateOpen(&impl->gate);

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_CondVarDelete_Impl(const OS_object_token_t *token)
{
    OS_impl_condvar_internal_record_t *impl;
    TaskHandle_t                       holder;
    bool                               refuse;

    impl = OS_OBJECT_TABLE_GET(OS_impl_condvar_table, *token);

    /*
     * Look at the holder and close the gate in one step, as OS_MutSemDelete_Impl()
     * does: a mutex found free stays free, and one held while its gate is closed
     * was orphaned by the death of its holder, so there is nothing left to protect.
     */
    taskENTER_CRITICAL();
    holder = xSemaphoreGetMutexHolder(impl->mutex);
    refuse = (holder != NULL) && !OS_FreeRTOS_GateIsClosed(&impl->gate);
    if (!refuse)
    {
        OS_FreeRTOS_GateClose(&impl->gate);
    }
    taskEXIT_CRITICAL();

    if (refuse)
    {
        return OS_ERROR;
    }

    /*
     * Wake the tasks that are blocked, in a wait or in a lock, and wait for every
     * user to leave. A user that was not yet inside the kernel call is woken by a
     * later pass, and the delay lets tasks of a lower priority than this one run
     * and leave.
     */
    while (OS_FreeRTOS_GateWake(&impl->gate) > 0)
    {
        vTaskDelay(1);
    }

    vSemaphoreDelete(impl->mutex);
    impl->mutex = NULL;

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *           A task that locks it twice would wait for itself for ever, and a
 *           task that does so on a board has no way to say what went wrong, so
 *           that is an error here.
 *
 *-----------------------------------------------------------------*/
int32 OS_CondVarLock_Impl(const OS_object_token_t *token)
{
    OS_impl_condvar_internal_record_t *impl;
    OS_impl_gate_user_t                user;
    int32                              status;

    impl = OS_OBJECT_TABLE_GET(OS_impl_condvar_table, *token);

    if (!OS_FreeRTOS_GateEnter(&impl->gate, &user))
    {
        return OS_ERR_INVALID_ID;
    }

    if (xSemaphoreGetMutexHolder(impl->mutex) == xTaskGetCurrentTaskHandle())
    {
        status = OS_ERROR;
    }
    else
    {
        BaseType_t taken  = xSemaphoreTake(impl->mutex, portMAX_DELAY);
        bool       closed = OS_FreeRTOS_GateIsClosed(&impl->gate);

        if (closed)
        {
            /* Deleted while this waited: hand the mutex back while the delete still waits, so it is free to free */
            if (taken == pdTRUE)
            {
                (void)xSemaphoreGive(impl->mutex);
            }

            status = OS_ERR_INVALID_ID;
        }
        else if (taken != pdTRUE)
        {
            status = OS_ERROR;
        }
        else
        {
            status = OS_SUCCESS;
        }
    }

    (void)OS_FreeRTOS_GateLeave(&impl->gate, &user);

    return status;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *           FreeRTOS does not refuse a mutex given back by a task that is not
 *           its holder: it fails an assertion, or corrupts the priorities of
 *           the holder, so the caller is checked here.
 *
 *-----------------------------------------------------------------*/
int32 OS_CondVarUnlock_Impl(const OS_object_token_t *token)
{
    OS_impl_condvar_internal_record_t *impl;
    OS_impl_gate_user_t                user;
    int32                              status;

    impl = OS_OBJECT_TABLE_GET(OS_impl_condvar_table, *token);

    if (!OS_FreeRTOS_GateEnter(&impl->gate, &user))
    {
        return OS_ERR_INVALID_ID;
    }

    if (xSemaphoreGetMutexHolder(impl->mutex) != xTaskGetCurrentTaskHandle())
    {
        status = OS_ERROR;
    }
    else
    {
        /* Cannot fail: the caller holds it */
        (void)xSemaphoreGive(impl->mutex);
        status = OS_SUCCESS;
    }

    (void)OS_FreeRTOS_GateLeave(&impl->gate, &user);

    return status;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *           Does not need the mutex held, and is not kept for a wait that
 *           comes later: with nobody waiting it does nothing.
 *
 *-----------------------------------------------------------------*/
int32 OS_CondVarSignal_Impl(const OS_object_token_t *token)
{
    OS_impl_condvar_internal_record_t *impl;
    OS_impl_condvar_waiter_t         **link;
    OS_impl_condvar_waiter_t         **best;
    OS_impl_condvar_waiter_t          *waiter;
    OS_impl_gate_user_t                user;

    impl = OS_OBJECT_TABLE_GET(OS_impl_condvar_table, *token);

    if (!OS_FreeRTOS_GateEnter(&impl->gate, &user))
    {
        return OS_ERR_INVALID_ID;
    }

    /*
     * The highest priority waiter, the first of them on the list among equals: only
     * a higher priority than the best so far replaces it. The list is walked and
     * the waiter woken in one critical section, which is what makes the waiter's
     * node safe to touch (see OS_FreeRTOS_CondVarWake()).
     */
    taskENTER_CRITICAL();
    best = NULL;
    for (link = &impl->waiters; *link != NULL; link = &(*link)->next)
    {
        if ((best == NULL) || (uxTaskPriorityGet((*link)->task) > uxTaskPriorityGet((*best)->task)))
        {
            best = link;
        }
    }

    if (best != NULL)
    {
        waiter = *best;
        *best  = waiter->next;
        OS_FreeRTOS_CondVarWake(waiter);
    }
    taskEXIT_CRITICAL();

    (void)OS_FreeRTOS_GateLeave(&impl->gate, &user);

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *           Wakes the tasks that wait now, and none that come after. Those it
 *           wakes take the mutex back one at a time.
 *
 *-----------------------------------------------------------------*/
int32 OS_CondVarBroadcast_Impl(const OS_object_token_t *token)
{
    OS_impl_condvar_internal_record_t *impl;
    OS_impl_condvar_waiter_t          *waiter;
    OS_impl_gate_user_t                user;

    impl = OS_OBJECT_TABLE_GET(OS_impl_condvar_table, *token);

    if (!OS_FreeRTOS_GateEnter(&impl->gate, &user))
    {
        return OS_ERR_INVALID_ID;
    }

    taskENTER_CRITICAL();
    while (impl->waiters != NULL)
    {
        waiter        = impl->waiters;
        impl->waiters = waiter->next;
        OS_FreeRTOS_CondVarWake(waiter);
    }
    taskEXIT_CRITICAL();

    (void)OS_FreeRTOS_GateLeave(&impl->gate, &user);

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *           Returns with the mutex held, as OSAL's callers rely on. Waking
 *           without a signal is allowed of a condition variable, and the
 *           callers test their condition again, so a wake that nothing
 *           explains is a success.
 *
 *-----------------------------------------------------------------*/
int32 OS_CondVarWait_Impl(const OS_object_token_t *token)
{
    return OS_FreeRTOS_CondVarBlock(token, portMAX_DELAY, OS_SUCCESS);
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *           The time is absolute, on the clock OS_GetLocalTime() reads, so the
 *           time left is worked out from that clock and then waited for in
 *           ticks. It is worked out upwards, to the next whole millisecond,
 *           and OS_FreeRTOS_MsToTicks() rounds up again, so the wait is never
 *           shorter than asked for. Time already past is a wait of no ticks:
 *           the same protocol, ending at once, so that a signal that has
 *           come in meanwhile is not lost.
 *
 *-----------------------------------------------------------------*/
int32 OS_CondVarTimedWait_Impl(const OS_object_token_t *token, const OS_time_t *abs_wakeup_time)
{
    OS_time_t  now;
    int64      msecs;
    TickType_t ticks;

    if (OS_GetLocalTime(&now) != OS_SUCCESS)
    {
        return OS_ERROR;
    }

    /* Adding just under a millisecond before the division turns its rounding down into rounding up */
    msecs = OS_TimeGetTotalMilliseconds(
        OS_TimeAdd(OS_TimeSubtract(*abs_wakeup_time, now), OS_TimeFromTotalNanoseconds(999999)));

    if (msecs <= 0)
    {
        ticks = 0;
    }
    else
    {
        if (msecs > OS_FREERTOS_CONDVAR_MAX_WAIT_MS)
        {
            msecs = OS_FREERTOS_CONDVAR_MAX_WAIT_MS;
        }

        ticks = OS_FreeRTOS_MsToTicks((uint32)msecs);
    }

    return OS_FreeRTOS_CondVarBlock(token, ticks, OS_ERROR_TIMEOUT);
}

/*----------------------------------------------------------------
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *           The task death hook (see os-freertos.h): drops the wait nodes of
 *           a task about to be deleted and orphans the mutexes it holds. A
 *           task blocked in OS_CondVarWait() has a node on its stack in the
 *           variable's wait list, which a later signal would follow into the
 *           freed stack and notify a freed TCB: the node is unlinked. A mutex
 *           the task holds is orphaned like any other: the gate is closed, the
 *           calls blocked on it are woken with an error, and the call returns
 *           once none is left in. Blocks, so not for interrupt context.
 *
 *           The task is suspended (or is the caller), so neither the nodes it
 *           has on the lists nor the mutexes it holds change meanwhile. Its
 *           nodes are only read through the lists, while its stack is still
 *           there. The gates are closed in one critical section; each is then
 *           woken and waited on until nobody is in. That has to finish before
 *           the task is deleted: a Lock woken from a mutex runs the kernel's
 *           priority disinheritance on the holder's TCB, which may be reused
 *           once it is deleted. Which mutexes to wait on is remembered, not
 *           looked up again: a delete running at the same time may already have
 *           freed one.
 *
 *-----------------------------------------------------------------*/
static void OS_FreeRTOS_CondVarTaskDied(TaskHandle_t task)
{
    bool   orphaned[OS_MAX_CONDVARS];
    uint32 i;

    taskENTER_CRITICAL();
    for (i = 0; i < OS_MAX_CONDVARS; ++i)
    {
        OS_impl_condvar_waiter_t **link = &OS_impl_condvar_table[i].waiters;

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

        orphaned[i] = (OS_impl_condvar_table[i].mutex != NULL)
                      && (xSemaphoreGetMutexHolder(OS_impl_condvar_table[i].mutex) == task);

        if (orphaned[i])
        {
            OS_FreeRTOS_GateClose(&OS_impl_condvar_table[i].gate);
        }
    }
    taskEXIT_CRITICAL();

    for (i = 0; i < OS_MAX_CONDVARS; ++i)
    {
        if (orphaned[i])
        {
            while (OS_FreeRTOS_GateWake(&OS_impl_condvar_table[i].gate) > 0)
            {
                vTaskDelay(1);
            }
        }
    }
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
// condvar_prop must stay non-const to match the os-shared-condvar.h prototype
// cppcheck-suppress constParameterPointer
int32 OS_CondVarGetInfo_Impl(const OS_object_token_t *token, OS_condvar_prop_t *condvar_prop)
{
    /* FreeRTOS has nothing to add to the name and creator the shared layer fills in */
    return OS_SUCCESS;
}
