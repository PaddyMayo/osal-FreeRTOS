/**
 * \file
 *
 * Purpose: OSAL mutex implementation on top of FreeRTOS recursive mutexes.
 *
 *          OSAL mutexes are recursive (the POSIX port sets
 *          PTHREAD_MUTEX_RECURSIVE, and OSAL's mutex-test takes one twice), so
 *          each maps to a FreeRTOS recursive mutex and must be taken and given
 *          with the *Recursive calls, never xSemaphoreTake()/xSemaphoreGive().
 *          FreeRTOS recursive mutexes also inherit priority, as the POSIX port
 *          asks of its mutexes with PTHREAD_PRIO_INHERIT.
 *
 *          Ownership is not checked here: OS_MutSemGive() in the shared layer
 *          warns when the wrong task gives, and FreeRTOS itself refuses a give
 *          from a task that is not the holder.
 *
 *          Take and Give reach the mutex without raising the refcount that
 *          OS_MutSemDelete() waits on, so they pass through the mutex's gate
 *          (see os-freertos.h). Delete refuses a mutex that a live task holds,
 *          as POSIX does: FreeRTOS counts the mutexes each task holds, and
 *          freeing one that is held leaves that count, and any priority the
 *          holder inherited, stuck. A mutex that is not locked has no blocked
 *          Takes to wake, so Delete only closes the gate and waits for the
 *          Takes and Gives that are already in.
 *
 *          A task deleted while it holds a mutex cannot give it back (only the
 *          holder can), so OS_FreeRTOS_MutexOwnerDied() orphans it: the gate is
 *          closed, the Takes blocked on it are woken with an error, and Delete,
 *          which finds it held but closed, frees it.
 */

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "os-shared-mutex.h"
#include "os-shared-idmap.h"

#include "os-freertos.h"
#include "os-impl-mutex.h"

OS_impl_mutex_internal_record_t OS_impl_mutex_table[OS_MAX_MUTEXES];

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_MutSemCreate_Impl(const OS_object_token_t *token, uint32 options)
{
    OS_impl_mutex_internal_record_t *impl;

    impl = OS_OBJECT_TABLE_GET(OS_impl_mutex_table, *token);

    impl->id = xSemaphoreCreateRecursiveMutex();
    if (impl->id == NULL)
    {
        return OS_SEM_FAILURE;
    }

    OS_FreeRTOS_GateOpen(&impl->gate);

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_MutSemDelete_Impl(const OS_object_token_t *token)
{
    OS_impl_mutex_internal_record_t *impl;
    TaskHandle_t                     holder;
    bool                             refuse;

    impl = OS_OBJECT_TABLE_GET(OS_impl_mutex_table, *token);

    /*
     * Look at the holder and close the gate in one step: nobody can take the
     * mutex between the two, so a mutex found free stays free. A mutex that is
     * held while its gate is closed is orphaned (its holder was deleted), and
     * nothing is left to protect, so it is freed like a free one.
     */
    taskENTER_CRITICAL();
    holder = xSemaphoreGetMutexHolder(impl->id);
    refuse = (holder != NULL) && !OS_FreeRTOS_GateIsClosed(&impl->gate);
    if (!refuse)
    {
        OS_FreeRTOS_GateClose(&impl->gate);
    }
    taskEXIT_CRITICAL();

    if (refuse)
    {
        return OS_SEM_FAILURE;
    }

    /*
     * Wait for the Takes and Gives already in. A Take that got in before the
     * gate closed and acquires the mutex afterwards gives it straight back
     * (see OS_MutSemTake_Impl), so it is free when it is deleted.
     */
    while (OS_FreeRTOS_GateWake(&impl->gate) > 0)
    {
        vTaskDelay(1);
    }

    vSemaphoreDelete(impl->id);
    impl->id = NULL;

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_MutSemGive_Impl(const OS_object_token_t *token)
{
    OS_impl_mutex_internal_record_t *impl;
    OS_impl_gate_user_t              user;
    BaseType_t                       given;

    impl = OS_OBJECT_TABLE_GET(OS_impl_mutex_table, *token);

    if (!OS_FreeRTOS_GateEnter(&impl->gate, &user))
    {
        return OS_ERR_INVALID_ID;
    }

    given = xSemaphoreGiveRecursive(impl->id);
    (void)OS_FreeRTOS_GateLeave(&impl->gate, &user);

    if (given != pdTRUE)
    {
        return OS_SEM_FAILURE;
    }

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_MutSemTake_Impl(const OS_object_token_t *token)
{
    OS_impl_mutex_internal_record_t *impl;
    OS_impl_gate_user_t              user;
    int32                            status;

    impl = OS_OBJECT_TABLE_GET(OS_impl_mutex_table, *token);

    if (!OS_FreeRTOS_GateEnter(&impl->gate, &user))
    {
        return OS_ERR_INVALID_ID;
    }

    if (xSemaphoreTakeRecursive(impl->id, portMAX_DELAY) != pdTRUE)
    {
        /*
         * A wait without a timeout only fails when it is woken: the gate was
         * closed, because the mutex is being deleted or its holder was.
         */
        if (OS_FreeRTOS_GateIsClosed(&impl->gate))
        {
            status = OS_ERR_INVALID_ID;
        }
        else
        {
            status = OS_SEM_FAILURE;
        }
    }
    else if (OS_FreeRTOS_GateIsClosed(&impl->gate))
    {
        /*
         * The mutex is being deleted and this Take got in before that. Hand
         * the mutex back before leaving, while the delete still waits, so it
         * is free when freed; that also lets any Take queued behind this one
         * through, to find the gate closed.
         */
        (void)xSemaphoreGiveRecursive(impl->id);
        status = OS_ERR_INVALID_ID;
    }
    else
    {
        status = OS_SUCCESS;
    }

    (void)OS_FreeRTOS_GateLeave(&impl->gate, &user);

    return status;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Port-internal helper routine, not part of OSAL API.
 *           Orphans the mutexes a task about to be deleted holds. See
 *           prototype in os-freertos.h.
 *
 *           The task is suspended (or is the caller), so the set of mutexes
 *           it holds cannot change meanwhile. The gates are closed in one
 *           critical section; each is then woken and waited on until nobody
 *           is in. That has to finish before the task is deleted: a Take woken
 *           from a mutex runs the kernel's priority disinheritance on the
 *           holder's TCB, and once the task is deleted the TCB may be reused.
 *           Which mutexes to wait on is remembered, not looked up again: a
 *           delete running at the same time may already have freed one.
 *
 *-----------------------------------------------------------------*/
void OS_FreeRTOS_MutexOwnerDied(TaskHandle_t task)
{
    bool   orphaned[OS_MAX_MUTEXES];
    uint32 i;

    taskENTER_CRITICAL();
    for (i = 0; i < OS_MAX_MUTEXES; ++i)
    {
        orphaned[i] =
            (OS_impl_mutex_table[i].id != NULL) && (xSemaphoreGetMutexHolder(OS_impl_mutex_table[i].id) == task);

        if (orphaned[i])
        {
            OS_FreeRTOS_GateClose(&OS_impl_mutex_table[i].gate);
        }
    }
    taskEXIT_CRITICAL();

    for (i = 0; i < OS_MAX_MUTEXES; ++i)
    {
        if (orphaned[i])
        {
            while (OS_FreeRTOS_GateWake(&OS_impl_mutex_table[i].gate) > 0)
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
// mut_prop must stay non-const to match the os-shared-mutex.h prototype
// cppcheck-suppress constParameterPointer
int32 OS_MutSemGetInfo_Impl(const OS_object_token_t *token, OS_mut_sem_prop_t *mut_prop)
{
    /* FreeRTOS has nothing to add to the name and creator the shared layer fills in */
    return OS_SUCCESS;
}
