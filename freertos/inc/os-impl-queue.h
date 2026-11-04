/*
 * File: os-impl-queue.h
 *
 * Purpose:
 *   Internal queue table definitions for the FreeRTOS OSAL port.
 *
 *   These live in a header rather than staying file-local to os-impl-queue.c
 *   so the coverage-test adaptor can reach the table through the real type
 *   instead of a duplicate declaration that could silently drift out of sync
 *   with it - the same reason OSAL's own ports expose theirs this way (see
 *   third_party/osal/src/os/vxworks/inc/os-impl-queues.h).
 */

#ifndef OS_IMPL_QUEUE_H
#define OS_IMPL_QUEUE_H

#include "osconfig.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/*
 * One per task blocked in OS_QueueGet_Impl, living on that task's stack. The
 * list lets OS_QueueDelete_Impl wake the task before it frees the queue.
 */
typedef struct OS_impl_queue_waiter
{
    TaskHandle_t                 task;
    // cppcheck-suppress unusedStructMember ; the list is walked in os-impl-queue.c
    struct OS_impl_queue_waiter *next;
} OS_impl_queue_waiter_t;

/*
 * OS_QueueGet() and OS_QueuePut() look the queue up with OS_LOCK_MODE_NONE,
 * which does not raise the refcount that OS_QueueDelete() waits on, so the
 * delete has to learn about them here. id is NULL once a delete has started;
 * users counts the Gets and Puts that took the id before that, and so still
 * hold the handle. Both are only changed inside a critical section.
 */
typedef struct
{
    QueueHandle_t           id;
    uint32                  users;
    OS_impl_queue_waiter_t *waiters;
} OS_impl_queue_internal_record_t;

/* Table where the OS object information is stored */
extern OS_impl_queue_internal_record_t OS_impl_queue_table[OS_MAX_QUEUES];

#endif /* OS_IMPL_QUEUE_H */
