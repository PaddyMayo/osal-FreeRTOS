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
#include "queue.h"

#include "os-freertos.h"

/*
 * OS_QueueGet() and OS_QueuePut() look the queue up with OS_LOCK_MODE_NONE,
 * which does not raise the refcount that OS_QueueDelete() waits on, so they
 * pass through the gate, which the delete closes and waits on (os-freertos.h).
 */
typedef struct
{
    QueueHandle_t  id;
    OS_impl_gate_t gate;
} OS_impl_queue_internal_record_t;

/* Table where the OS object information is stored */
extern OS_impl_queue_internal_record_t OS_impl_queue_table[OS_MAX_QUEUES];

#endif /* OS_IMPL_QUEUE_H */
