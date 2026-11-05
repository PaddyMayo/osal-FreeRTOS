/*
 * File: os-impl-condvar.h
 *
 * Purpose:
 *   Internal condition variable table definitions for the FreeRTOS OSAL port.
 *
 *   These live in a header rather than staying file-local to
 *   os-impl-condvar.c so the coverage-test adaptor can reach the table through
 *   the real type instead of a duplicate declaration that could silently drift
 *   out of sync with it - the same reason OSAL's own ports expose theirs this
 *   way (see third_party/osal/src/os/posix/inc/os-impl-condvar.h).
 */

#ifndef OS_IMPL_CONDVAR_H
#define OS_IMPL_CONDVAR_H

#include "osconfig.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include "os-freertos.h"

/*
 * The task notification index a waiting task blocks on. Index 0 is left to
 * application code, so that a notification meant for it can never be taken for
 * a signal here, nor a signal for it.
 */
#define OS_FREERTOS_CONDVAR_NOTIFY_INDEX 1

#if defined(configTASK_NOTIFICATION_ARRAY_ENTRIES) \
    && (OS_FREERTOS_CONDVAR_NOTIFY_INDEX >= configTASK_NOTIFICATION_ARRAY_ENTRIES)
#error "FreeRTOSConfig.h must define configTASK_NOTIFICATION_ARRAY_ENTRIES above OS_FREERTOS_CONDVAR_NOTIFY_INDEX"
#endif

/*
 * The longest TimedWait that is waited out, in milliseconds. A longer one ends at
 * this: it keeps the tick count that OS_FreeRTOS_MsToTicks() works out from
 * wrapping round to a short wait. About 24.8 days.
 */
#define OS_FREERTOS_CONDVAR_MAX_WAIT_MS 2147483647

/*
 * One per task blocked in OS_CondVarWait() or OS_CondVarTimedWait(), living on
 * the stack of that task, in the wait list of the condition variable. A signal
 * takes the node off the list, sets signaled and notifies the task, all in one
 * critical section; the task, once it stops waiting, takes itself off the list
 * if no signal did, in another. That is how a signal is never lost to a task
 * that gave up at the same moment, and never reaches one that has gone.
 */
typedef struct OS_impl_condvar_waiter
{
    TaskHandle_t                   task;
    // cppcheck-suppress unusedStructMember ; the list is walked in os-impl-condvar.c
    struct OS_impl_condvar_waiter *next;
    // cppcheck-suppress unusedStructMember ; set and read in os-impl-condvar.c
    bool                           signaled;
} OS_impl_condvar_waiter_t;

/*
 * OS_CondVarLock(), Unlock(), Signal(), Broadcast(), Wait() and TimedWait()
 * look the condition variable up with OS_LOCK_MODE_NONE, which does not raise
 * the refcount that OS_CondVarDelete() waits on, so they pass through the gate,
 * which the delete closes and waits on (os-freertos.h).
 *
 * The mutex is not recursive, as POSIX's default one is not: Wait gives it up
 * once, and would leave it held if it had been taken twice.
 */
typedef struct
{
    SemaphoreHandle_t         mutex;
    OS_impl_gate_t            gate;
    OS_impl_condvar_waiter_t *waiters;
} OS_impl_condvar_internal_record_t;

/* Table where the OS object information is stored */
extern OS_impl_condvar_internal_record_t OS_impl_condvar_table[OS_MAX_CONDVARS];

#endif /* OS_IMPL_CONDVAR_H */
