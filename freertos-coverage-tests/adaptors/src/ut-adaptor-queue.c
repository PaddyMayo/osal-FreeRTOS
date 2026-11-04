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

/* Waiter nodes for UT_QueueTest_PushWaiter(), handed out in turn */
#define UT_WAITER_POOL_SIZE 4

static OS_impl_queue_waiter_t UT_WaiterPool[UT_WAITER_POOL_SIZE];
static uint32                 UT_WaiterNext;

uint32 UT_QueueTest_GetImplUsers(osal_index_t local_id)
{
    return OS_impl_queue_table[local_id].users;
}

TaskHandle_t UT_QueueTest_GetImplWaiterTask(osal_index_t local_id)
{
    const OS_impl_queue_waiter_t *waiter = OS_impl_queue_table[local_id].waiters;

    return (waiter != NULL) ? waiter->task : NULL;
}

void UT_QueueTest_PushWaiter(osal_index_t local_id, TaskHandle_t task)
{
    OS_impl_queue_internal_record_t *impl   = &OS_impl_queue_table[local_id];
    OS_impl_queue_waiter_t          *waiter = &UT_WaiterPool[UT_WaiterNext % UT_WAITER_POOL_SIZE];

    ++UT_WaiterNext;

    waiter->task  = task;
    waiter->next  = impl->waiters;
    impl->waiters = waiter;
    ++impl->users;
}

void UT_QueueTest_ReleaseWaiters(osal_index_t local_id)
{
    OS_impl_queue_internal_record_t *impl = &OS_impl_queue_table[local_id];

    while (impl->waiters != NULL)
    {
        impl->waiters = impl->waiters->next;
        --impl->users;
    }
}
