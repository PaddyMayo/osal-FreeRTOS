/**
 * \file
 *
 * Purpose: OSAL timebase implementation on top of FreeRTOS tasks.
 *
 *          A time base is the tick that OSAL timers are counted in. The shared
 *          layer (os-shared-timebase.h) does the counting and calls the timers'
 *          callbacks, in OS_TimeBase_CallbackThread(); that function needs a task
 *          to run in, and a function that blocks until the next tick and returns
 *          the time that went by, in microseconds. Application callbacks run in
 *          the task, and may block, so it is a task of the port's own and not
 *          the FreeRTOS timer service task, which must never block.
 *
 *          OS_TimeBaseCreate_Impl() creates that task, from a static TCB and stack
 *          in the time base's table slot, and, for a time base without a BSP sync
 *          function, supplies the wait. Setting the tick period, deleting the time
 *          base, its lock and its information are not implemented yet: those
 *          functions return OS_ERR_NOT_IMPLEMENTED (or do nothing) for now.
 */

#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "os-shared-common.h"
#include "os-shared-timebase.h"
#include "os-shared-idmap.h"

#include "os-impl-tasks.h"
#include "os-impl-timebase.h"

OS_impl_timebase_internal_record_t OS_impl_timebase_table[OS_MAX_TIMEBASES];

/*----------------------------------------------------------------
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *           The entry point of a time base's servicing task: hands off to
 *           OS_TimeBase_CallbackThread(), which returns once the time base
 *           is deleted, and then ends the task, as a FreeRTOS task must not
 *           return.
 *
 *-----------------------------------------------------------------*/
static void OS_FreeRTOS_TimeBaseTaskEntry(void *pvParameters)
{
    OS_VoidPtrValueWrapper_t local_arg;

    memset(&local_arg, 0, sizeof(local_arg));
    local_arg.opaque_arg = pvParameters;
    OS_TimeBase_CallbackThread(local_arg.id);

    /* NULL tells FreeRTOS to delete the calling task */
    vTaskDelete(NULL);
}

/*----------------------------------------------------------------
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *           Tells whether the kernel tick count has reached a deadline. The
 *           count wraps, so the difference is what is looked at: it is zero
 *           on the tick, and falls in the upper half of the range once the
 *           deadline is behind.
 *
 *-----------------------------------------------------------------*/
static bool OS_FreeRTOS_TimeBaseIsDue(TickType_t deadline, TickType_t now)
{
    TickType_t remaining;

    remaining = deadline - now;

    return (remaining == 0) || (remaining > (portMAX_DELAY / 2));
}

/*----------------------------------------------------------------
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *           Converts a time in microseconds to kernel ticks, rounding up so
 *           the wait is never shorter than asked, the way
 *           OS_FreeRTOS_MsToTicks() does for milliseconds. Zero stays zero.
 *           The callers have checked the time is below 1000000000, so
 *           the sum cannot overflow.
 *
 *-----------------------------------------------------------------*/
static TickType_t OS_FreeRTOS_TimeBaseUsecToTicks(uint32 usec)
{
    return (TickType_t)((usec + OS_SharedGlobalVars.MicroSecPerTick - 1) / OS_SharedGlobalVars.MicroSecPerTick);
}

/*----------------------------------------------------------------
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *           The sync function of a time base that has none of its own: blocks
 *           the servicing task until the next tick is due, or until it is
 *           notified, and returns how many microseconds went by since it last
 *           returned, counted in kernel ticks. The task sleeps for the time
 *           left until the deadline, or for good while no tick is armed. A
 *           wake by notification, which OS_TimeBaseSet_Impl() sends when it
 *           arms a different tick, returns zero, which
 *           OS_TimeBase_CallbackThread() takes as "call again". A time base
 *           that is gone returns zero without waiting, and the caller sees that
 *           its id is no longer the active one.
 *
 *-----------------------------------------------------------------*/
static uint32 OS_FreeRTOS_TimeBaseWait(osal_id_t timebase_id)
{
    OS_object_token_t                   token;
    OS_impl_timebase_internal_record_t *impl;
    TickType_t                          now;
    TickType_t                          deadline;
    TickType_t                          timeout;
    bool                                armed;
    uint32                              elapsed;

    if (OS_ObjectIdGetById(OS_LOCK_MODE_NONE, OS_OBJECT_TYPE_OS_TIMEBASE, timebase_id, &token) != OS_SUCCESS)
    {
        return 0;
    }

    impl = OS_OBJECT_TABLE_GET(OS_impl_timebase_table, token);

    taskENTER_CRITICAL();
    armed    = impl->armed;
    deadline = impl->deadline;
    now      = xTaskGetTickCount();
    taskEXIT_CRITICAL();

    if (!armed)
    {
        timeout = portMAX_DELAY;
    }
    else if (OS_FreeRTOS_TimeBaseIsDue(deadline, now))
    {
        timeout = 0;
    }
    else
    {
        timeout = deadline - now;
    }

    /* A notification wakes it early, with a count; running out of time, with none, is the tick */
    if (ulTaskNotifyTakeIndexed(OS_FREERTOS_TIMEBASE_NOTIFY_INDEX, pdTRUE, timeout) != 0)
    {
        return 0;
    }

    /*
     * The time is counted whether or not a tick was due: it is the same time
     * that went by either way. A tick that is due is passed, and the next one
     * set, a whole number of intervals on from the last, so the ticks do not
     * drift; any that were missed are not made up, their time is in the count.
     * Another Set may have armed a different tick since the deadline was read, in which case
     * its deadline is not due and is left alone.
     */
    taskENTER_CRITICAL();
    now             = xTaskGetTickCount();
    elapsed         = (uint32)(now - impl->last_tick) * OS_SharedGlobalVars.MicroSecPerTick;
    impl->last_tick = now;

    if (impl->armed && OS_FreeRTOS_TimeBaseIsDue(impl->deadline, now))
    {
        if (impl->interval == 0)
        {
            impl->armed = false;
        }
        else
        {
            do
            {
                impl->deadline += impl->interval;
            } while (OS_FreeRTOS_TimeBaseIsDue(impl->deadline, now));
        }
    }
    taskEXIT_CRITICAL();

    return elapsed;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *           Runs under the global lock of the time base table, which is what
 *           holds the new task back: its first act is to take that lock, and it
 *           runs at the highest priority an OSAL task has, so it is the first
 *           thing to run once the task exists, and waits there until the caller
 *           has finished creating the time base. The sync function is put in
 *           place before the task exists, so it never sees a time base without
 *           one.
 *
 *-----------------------------------------------------------------*/
int32 OS_TimeBaseCreate_Impl(const OS_object_token_t *token)
{
    OS_impl_timebase_internal_record_t *impl;
    OS_timebase_internal_record_t      *timebase;
    OS_VoidPtrValueWrapper_t            arg;
    bool                                simulated;

    impl     = OS_OBJECT_TABLE_GET(OS_impl_timebase_table, *token);
    timebase = OS_OBJECT_TABLE_GET(OS_timebase_table, *token);

    memset(&arg, 0, sizeof(arg));
    arg.id = OS_ObjectIdFromToken(token);

    /* Without a sync function from the BSP the tick is simulated, by the one in this file */
    simulated = (timebase->external_sync == NULL);
    if (simulated)
    {
        timebase->external_sync = OS_FreeRTOS_TimeBaseWait;
    }

    impl->last_tick = xTaskGetTickCount();

    impl->handle = xTaskCreateStatic(OS_FreeRTOS_TimeBaseTaskEntry,
                                     timebase->timebase_name,
                                     OS_FREERTOS_TIMEBASE_STACK_WORDS,
                                     arg.opaque_arg,
                                     OS_FreeRTOS_PriorityRemap(0),
                                     impl->stack,
                                     &impl->task);

    if (impl->handle == NULL)
    {
        if (simulated)
        {
            timebase->external_sync = NULL;
        }

        return OS_TIMER_ERR_UNAVAILABLE;
    }

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_TimeBaseSet_Impl(const OS_object_token_t *token, uint32 start_time, uint32 interval_time)
{
    OS_impl_timebase_internal_record_t *impl;
    OS_timebase_internal_record_t      *timebase;
    TickType_t                          start_ticks;
    TickType_t                          interval_ticks;

    impl     = OS_OBJECT_TABLE_GET(OS_impl_timebase_table, *token);
    timebase = OS_OBJECT_TABLE_GET(OS_timebase_table, *token);

    /* There is only something to do here if the tick is the simulated one: a BSP sync function brings its own */
    if (timebase->external_sync != OS_FreeRTOS_TimeBaseWait)
    {
        return OS_SUCCESS;
    }

    start_ticks    = OS_FreeRTOS_TimeBaseUsecToTicks(start_time);
    interval_ticks = OS_FreeRTOS_TimeBaseUsecToTicks(interval_time);

    /*
     * A start time of zero disarms it, whatever the interval, as it does on
     * POSIX. Otherwise the first tick is due start_ticks from now, and the
     * time the task reports is counted from now, so a time base that sat
     * unarmed does not report that time as its first tick.
     */
    taskENTER_CRITICAL();
    impl->last_tick = xTaskGetTickCount();
    impl->armed     = (start_ticks != 0);
    impl->deadline  = impl->last_tick + start_ticks;
    impl->interval  = interval_ticks;
    taskEXIT_CRITICAL();

    if (impl->armed)
    {
        /* The ticks cannot come any closer than this: the period asked for, rounded up to the kernel tick */
        timebase->accuracy_usec =
            ((interval_ticks != 0) ? interval_ticks : start_ticks) * OS_SharedGlobalVars.MicroSecPerTick;
    }

    /* The task may be asleep until the old deadline, or for good: have it look again */
    (void)xTaskNotifyGiveIndexed(impl->handle, OS_FREERTOS_TIMEBASE_NOTIFY_INDEX);

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_TimeBaseDelete_Impl(const OS_object_token_t *token)
{
    /* TODO: runs under the exclusive lock. Wake the helper task and wait for
     * it to leave before freeing anything it uses. */
    (void)token;
    return OS_ERR_NOT_IMPLEMENTED;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
void OS_TimeBaseLock_Impl(const OS_object_token_t *token)
{
    /* TODO: take the per-timebase mutex, which the helper task holds while it
     * processes a tick. */
    (void)token;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
void OS_TimeBaseUnlock_Impl(const OS_object_token_t *token)
{
    /* TODO: give the per-timebase mutex taken by OS_TimeBaseLock_Impl(). */
    (void)token;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
// timer_prop must stay non-const to match the os-shared-timebase.h prototype
// cppcheck-suppress constParameterPointer
int32 OS_TimeBaseGetInfo_Impl(const OS_object_token_t *token, OS_timebase_prop_t *timer_prop)
{
    /* Nothing OS-specific to fill in; the shared layer fills the rest. */
    return OS_SUCCESS;
}
