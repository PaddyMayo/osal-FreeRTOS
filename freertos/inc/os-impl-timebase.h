/*
 * File: os-impl-timebase.h
 *
 * Purpose:
 *   Internal time base table definitions for the FreeRTOS OSAL port.
 *
 *   These live in a header rather than staying file-local to os-impl-timebase.c
 *   so the coverage-test adaptor can reach the table through the real type
 *   instead of a duplicate declaration that could silently drift out of sync
 *   with it - the same reason OSAL's own ports expose theirs this way (see
 *   third_party/osal/src/os/vxworks/inc/os-impl-timebase.h).
 */

#ifndef OS_IMPL_TIMEBASE_H
#define OS_IMPL_TIMEBASE_H

#include "osconfig.h"

#include "FreeRTOS.h"
#include "task.h"

/*
 * The task notification index a time base's servicing task waits for its tick
 * on. Index 0 is left to application code, whose timer callbacks run in this
 * task, and index 1 belongs to the condition variables, so a wake meant for
 * the tick cannot be taken by either (see FreeRTOSConfig.h).
 */
#define OS_FREERTOS_TIMEBASE_NOTIFY_INDEX 2

/*
 * The servicing task runs the application's timer callbacks, on the stack it
 * has here, so it gets what OSAL gives its own utility tasks.
 */
#define OS_FREERTOS_TIMEBASE_STACK_WORDS (OS_UTILITYTASK_STACK_SIZE / sizeof(StackType_t))

/*
 * A time base has a servicing task, which is created with it and runs
 * OS_TimeBase_CallbackThread() (os-shared-timebase.h). Its TCB and stack are
 * static, one pair per table slot, so creating a time base allocates nothing.
 *
 * The servicing task and OS_TimeBaseSet_Impl() both use armed, deadline,
 * interval and last_tick, so they read and write them in a critical section.
 */
typedef struct
{
    StaticTask_t task;
    StackType_t  stack[OS_FREERTOS_TIMEBASE_STACK_WORDS];
    TaskHandle_t handle;    /* the servicing task */
    TickType_t   last_tick; /* the kernel tick count the elapsed time is counted from: set when the task is created
                               and armed, and each time the wait returns */
    bool       armed; /* a tick is due at deadline; false until OS_TimeBaseSet_Impl() arms one, and after a one-shot */
    TickType_t deadline; /* the kernel tick count of the next tick */
    TickType_t interval; /* ticks between ticks; zero for a one-shot */
} OS_impl_timebase_internal_record_t;

/* Table where the OS object information is stored */
extern OS_impl_timebase_internal_record_t OS_impl_timebase_table[OS_MAX_TIMEBASES];

#endif /* OS_IMPL_TIMEBASE_H */
