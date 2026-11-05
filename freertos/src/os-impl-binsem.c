/**
 * \file
 *
 * Purpose: OSAL binary semaphore implementation on top of FreeRTOS binary
 *          semaphores.
 *
 *          Each OSAL binary semaphore maps to one FreeRTOS binary semaphore,
 *          which holds 0 or 1 just as OSAL's does: giving a full one leaves it
 *          full, and any initial value above 0 creates it full, as the POSIX
 *          port treats a value above 1 as 1. Like POSIX's, these cannot be
 *          given from an interrupt: the gate below is a critical section, not
 *          an ISR-safe call.
 *
 *          Give, Flush, Take and TimedWait reach the semaphore without raising
 *          the refcount that OS_BinSemDelete() waits on, so they pass through
 *          the semaphore's gate (see os-freertos.h). OS_BinSemDelete_Impl()
 *          closes the gate, wakes the Takes that are blocked, which then
 *          return OS_ERR_INVALID_ID, and frees the semaphore only once every
 *          user has left - vSemaphoreDelete() would otherwise free a semaphore
 *          that tasks are still waiting on.
 *
 *          A flush has to release the tasks that are waiting without giving
 *          them the semaphore, which FreeRTOS has no call for. It adds one to
 *          the semaphore's flush_count and wakes the gate's blocked users, the
 *          way a delete does; a Take that comes back from a failed wait with
 *          the count changed, and the gate still open, was flushed, and
 *          succeeds without having taken anything.
 */

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "os-shared-binsem.h"
#include "os-shared-idmap.h"

#include "os-freertos.h"
#include "os-impl-binsem.h"

OS_impl_binsem_internal_record_t OS_impl_bin_sem_table[OS_MAX_BIN_SEMAPHORES];

/*----------------------------------------------------------------
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *           Takes the semaphore for Take and TimedWait, which differ only in
 *           how long they wait and in what an unexplained end of the wait
 *           means: expired is what to return when the wait ended with the
 *           semaphore not taken, and neither flushed nor deleted.
 *
 *           Why a wait ended is worked out while the caller is still in the
 *           gate: the delete cannot finish until it leaves, so the record is
 *           still this semaphore's. The flush count is read there for that
 *           reason, and not after the gate is left.
 *
 *-----------------------------------------------------------------*/
static int32 OS_FreeRTOS_BinSemWait(const OS_object_token_t *token, TickType_t ticks, int32 expired)
{
    OS_impl_binsem_internal_record_t *impl;
    OS_impl_gate_user_t               user;
    uint32                            flushes;
    BaseType_t                        taken;
    bool                              flushed;
    bool                              deleted;

    impl = OS_OBJECT_TABLE_GET(OS_impl_bin_sem_table, *token);

    if (!OS_FreeRTOS_GateEnter(&impl->gate, &user))
    {
        return OS_ERR_INVALID_ID;
    }

    flushes = impl->flush_count;
    taken   = xSemaphoreTake(impl->id, ticks);
    flushed = (impl->flush_count != flushes);
    deleted = OS_FreeRTOS_GateLeave(&impl->gate, &user);

    if (taken == pdTRUE)
    {
        return OS_SUCCESS;
    }

    /* The delete woke this Take, it did not time out */
    if (deleted)
    {
        return OS_ERR_INVALID_ID;
    }

    /* The flush woke this Take, and flushed Takes succeed without the semaphore */
    if (flushed)
    {
        return OS_SUCCESS;
    }

    return expired;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_BinSemCreate_Impl(const OS_object_token_t *token, uint32 sem_initial_value, uint32 options)
{
    OS_impl_binsem_internal_record_t *impl;

    impl = OS_OBJECT_TABLE_GET(OS_impl_bin_sem_table, *token);

    /* A FreeRTOS binary semaphore is created empty */
    impl->id = xSemaphoreCreateBinary();
    if (impl->id == NULL)
    {
        return OS_SEM_FAILURE;
    }

    if (sem_initial_value != 0)
    {
        /* Cannot fail: the semaphore is new, and empty */
        (void)xSemaphoreGive(impl->id);
    }

    impl->flush_count = 0;
    OS_FreeRTOS_GateOpen(&impl->gate);

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_BinSemDelete_Impl(const OS_object_token_t *token)
{
    OS_impl_binsem_internal_record_t *impl;

    impl = OS_OBJECT_TABLE_GET(OS_impl_bin_sem_table, *token);

    /* From here on no Give, Flush, Take or TimedWait can start */
    OS_FreeRTOS_GateClose(&impl->gate);

    /*
     * Wake the blocked Takes and wait for every user to leave. A user that was
     * not yet inside the kernel call is woken by a later pass, and the delay
     * lets tasks of a lower priority than this one run and leave.
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
 *           The wake does not touch the semaphore: the tasks it releases
 *           return without having taken it, and its value is left as it was.
 *
 *-----------------------------------------------------------------*/
int32 OS_BinSemFlush_Impl(const OS_object_token_t *token)
{
    OS_impl_binsem_internal_record_t *impl;
    OS_impl_gate_user_t               user;

    impl = OS_OBJECT_TABLE_GET(OS_impl_bin_sem_table, *token);

    if (!OS_FreeRTOS_GateEnter(&impl->gate, &user))
    {
        return OS_ERR_INVALID_ID;
    }

    /* Read-modify-write: two flushes at once must not add one between them */
    taskENTER_CRITICAL();
    ++impl->flush_count;
    taskEXIT_CRITICAL();

    /*
     * The count is changed before the wake, so a Take that is woken sees it. A
     * Take in the gate that has not blocked yet is not woken: it began its
     * wait after the flush, as far as anyone can tell.
     */
    (void)OS_FreeRTOS_GateWake(&impl->gate);
    (void)OS_FreeRTOS_GateLeave(&impl->gate, &user);

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_BinSemGive_Impl(const OS_object_token_t *token)
{
    OS_impl_binsem_internal_record_t *impl;
    OS_impl_gate_user_t               user;

    impl = OS_OBJECT_TABLE_GET(OS_impl_bin_sem_table, *token);

    if (!OS_FreeRTOS_GateEnter(&impl->gate, &user))
    {
        return OS_ERR_INVALID_ID;
    }

    /*
     * The only way a give fails is that the semaphore is full already, and
     * giving a full binary semaphore is not an error in OSAL: it stays full.
     */
    (void)xSemaphoreGive(impl->id);
    (void)OS_FreeRTOS_GateLeave(&impl->gate, &user);

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_BinSemTake_Impl(const OS_object_token_t *token)
{
    /* A wait without a timeout only ends unexplained if something else woke the task */
    return OS_FreeRTOS_BinSemWait(token, portMAX_DELAY, OS_SEM_FAILURE);
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_BinSemTimedWait_Impl(const OS_object_token_t *token, uint32 msecs)
{
    return OS_FreeRTOS_BinSemWait(token, OS_FreeRTOS_MsToTicks(msecs), OS_SEM_TIMEOUT);
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *           Needs no gate: OS_BinSemGetInfo() looks the semaphore up with
 *           OS_LOCK_MODE_GLOBAL, which holds off a delete until it is done.
 *
 *-----------------------------------------------------------------*/
int32 OS_BinSemGetInfo_Impl(const OS_object_token_t *token, OS_bin_sem_prop_t *bin_prop)
{
    const OS_impl_binsem_internal_record_t *impl;

    impl = OS_OBJECT_TABLE_GET(OS_impl_bin_sem_table, *token);

    /* A binary semaphore's count is its value: 0 or 1 */
    bin_prop->value = (int32)uxSemaphoreGetCount(impl->id);

    return OS_SUCCESS;
}
