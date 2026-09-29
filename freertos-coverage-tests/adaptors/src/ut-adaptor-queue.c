/*
 * File: ut-adaptor-queue.c
 *
 * Purpose:
 *   Implements the test-only accessors declared in ut-adaptor-queue.h,
 *   against the real OS_impl_queue_table type from the port's own
 *   freertos/inc/os-impl-queue.h.
 */

#include "osconfig.h"

#include "ut-adaptor-queue.h"

#include "os-impl-queue.h"

void *const  UT_Ref_OS_impl_queue_table      = OS_impl_queue_table;
size_t const UT_Ref_OS_impl_queue_table_SIZE = sizeof(OS_impl_queue_table);

void UT_QueueTest_SetImplQueueId(osal_index_t local_id, QueueHandle_t QueueId)
{
    OS_impl_queue_table[local_id].id = QueueId;
}

QueueHandle_t UT_QueueTest_GetImplQueueId(osal_index_t local_id)
{
    return OS_impl_queue_table[local_id].id;
}
