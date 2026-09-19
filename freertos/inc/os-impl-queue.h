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

typedef struct
{
    QueueHandle_t id;
} OS_impl_queue_internal_record_t;

/* Table where the OS object information is stored */
extern OS_impl_queue_internal_record_t OS_impl_queue_table[OS_MAX_QUEUES];

#endif /* OS_IMPL_QUEUE_H */
