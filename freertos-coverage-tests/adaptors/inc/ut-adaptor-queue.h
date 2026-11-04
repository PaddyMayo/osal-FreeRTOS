/*
 * File: ut-adaptor-queue.h
 *
 * Purpose:
 *   Test-only accessors into os-impl-queue.c's internal state, for coverage
 *   test cases that need to set up (or inspect) a precondition the module's
 *   public _Impl API cannot reach directly. See ut-adaptor-tasks.h for the
 *   rationale behind keeping these in a dedicated adaptor.
 */

#ifndef UT_ADAPTOR_QUEUE_H
#define UT_ADAPTOR_QUEUE_H

#include "common_types.h"

#include "FreeRTOS.h"
#include "queue.h"

/* Opaque handle to OS_impl_queue_table, for resetting it between test cases */
extern void *const  UT_Ref_OS_impl_queue_table;
extern size_t const UT_Ref_OS_impl_queue_table_SIZE;

void          UT_QueueTest_SetImplQueueId(osal_index_t local_id, QueueHandle_t QueueId);
QueueHandle_t UT_QueueTest_GetImplQueueId(osal_index_t local_id);

/* The Gets and Puts a delete has to wait for: how many, and the tasks blocked in a Get */
uint32       UT_QueueTest_GetImplUsers(osal_index_t local_id);
TaskHandle_t UT_QueueTest_GetImplWaiterTask(osal_index_t local_id); /* newest waiter's task, NULL if none */

/* Registers a Get blocked in `task`, as OS_QueueGet_Impl() does (at most 4 at a time) */
void UT_QueueTest_PushWaiter(osal_index_t local_id, TaskHandle_t task);

/* Drops every registered Get, as if those tasks had all been woken and left */
void UT_QueueTest_ReleaseWaiters(osal_index_t local_id);

#endif /* UT_ADAPTOR_QUEUE_H */
