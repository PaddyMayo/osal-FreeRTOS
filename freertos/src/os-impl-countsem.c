/**
 * \file
 *
 * Purpose: OSAL counting semaphore implementation on top of FreeRTOS counting
 *          semaphores.
 *
 *          Each OSAL counting semaphore maps to one FreeRTOS counting
 *          semaphore, made with OS_FREERTOS_SEM_VALUE_MAX as its limit. A
 *          create above that limit is refused with OS_INVALID_SEM_VALUE, and
 *          a give at the limit fails with OS_SEM_FAILURE, as POSIX's does when
 *          sem_post() overflows.
 *
 *          Unlike POSIX's, these cannot be given from an interrupt: Give passes
 *          through the gate below, which is a critical section and not an
 *          ISR-safe call. An interrupt that needs to wake a task has to use
 *          something else.
 *
 *          Give, Take and TimedWait reach the semaphore without raising the
 *          refcount that OS_CountSemDelete() waits on, so they pass through
 *          the semaphore's gate (see os-freertos.h). OS_CountSemDelete_Impl()
 *          closes the gate, wakes the Takes that are blocked, which then
 *          return OS_ERR_INVALID_ID, and frees the semaphore only once every
 *          user has left - vSemaphoreDelete() would otherwise free a semaphore
 *          that tasks are still waiting on.
 */

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "os-shared-countsem.h"
#include "os-shared-idmap.h"

#include "os-freertos.h"
#include "os-impl-countsem.h"

OS_impl_countsem_internal_record_t OS_impl_count_sem_table[OS_MAX_COUNT_SEMAPHORES];

/*----------------------------------------------------------------
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *           Takes the semaphore for Take and TimedWait, which differ only in
 *           how long they wait and in what an unexplained end of the wait
 *           means: expired is what to return when the wait ended with the
 *           semaphore not taken, and the semaphore not deleted.
 *
 *-----------------------------------------------------------------*/
static int32 OS_FreeRTOS_CountSemWait(const OS_object_token_t *token, TickType_t ticks, int32 expired)
{
    OS_impl_countsem_internal_record_t *impl;
    OS_impl_gate_user_t                 user;
    BaseType_t                          taken;
    bool                                deleted;

    impl = OS_OBJECT_TABLE_GET(OS_impl_count_sem_table, *token);

    if (!OS_FreeRTOS_GateEnter(&impl->gate, &user))
    {
        return OS_ERR_INVALID_ID;
    }

    taken   = xSemaphoreTake(impl->id, ticks);
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

    return expired;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_CountSemCreate_Impl(const OS_object_token_t *token, uint32 sem_initial_value, uint32 options)
{
    OS_impl_countsem_internal_record_t *impl;

    impl = OS_OBJECT_TABLE_GET(OS_impl_count_sem_table, *token);

    if (sem_initial_value > OS_FREERTOS_SEM_VALUE_MAX)
    {
        return OS_INVALID_SEM_VALUE;
    }

    impl->id = xSemaphoreCreateCounting(OS_FREERTOS_SEM_VALUE_MAX, sem_initial_value);
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
int32 OS_CountSemDelete_Impl(const OS_object_token_t *token)
{
    OS_impl_countsem_internal_record_t *impl;

    impl = OS_OBJECT_TABLE_GET(OS_impl_count_sem_table, *token);

    /* From here on no Give, Take or TimedWait can start */
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
 *-----------------------------------------------------------------*/
int32 OS_CountSemGive_Impl(const OS_object_token_t *token)
{
    OS_impl_countsem_internal_record_t *impl;
    OS_impl_gate_user_t                 user;
    BaseType_t                          given;

    impl = OS_OBJECT_TABLE_GET(OS_impl_count_sem_table, *token);

    if (!OS_FreeRTOS_GateEnter(&impl->gate, &user))
    {
        return OS_ERR_INVALID_ID;
    }

    given = xSemaphoreGive(impl->id);
    (void)OS_FreeRTOS_GateLeave(&impl->gate, &user);

    /* The only way a give fails is that the count is at its limit already */
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
int32 OS_CountSemTake_Impl(const OS_object_token_t *token)
{
    /* A wait without a timeout only ends unexplained if something else woke the task */
    return OS_FreeRTOS_CountSemWait(token, portMAX_DELAY, OS_SEM_FAILURE);
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_CountSemTimedWait_Impl(const OS_object_token_t *token, uint32 msecs)
{
    return OS_FreeRTOS_CountSemWait(token, OS_FreeRTOS_MsToTicks(msecs), OS_SEM_TIMEOUT);
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *           Needs no gate: OS_CountSemGetInfo() looks the semaphore up with
 *           OS_LOCK_MODE_GLOBAL, which holds off a delete until it is done.
 *
 *-----------------------------------------------------------------*/
int32 OS_CountSemGetInfo_Impl(const OS_object_token_t *token, OS_count_sem_prop_t *count_prop)
{
    const OS_impl_countsem_internal_record_t *impl;

    impl = OS_OBJECT_TABLE_GET(OS_impl_count_sem_table, *token);

    /* The count never exceeds OS_FREERTOS_SEM_VALUE_MAX, so it fits an int32 */
    count_prop->value = (int32)uxSemaphoreGetCount(impl->id);

    return OS_SUCCESS;
}
