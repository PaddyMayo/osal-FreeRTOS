/*
 * File: ut-adaptor-tasks.c
 *
 * Purpose:
 *   Implements the test-only accessors declared in ut-adaptor-tasks.h,
 *   against the real OS_impl_task_table type from the port's own
 *   freertos/inc/os-impl-tasks.h.
 */

#include "osconfig.h"

#include "ut-adaptor-tasks.h"

#include "os-impl-tasks.h"

void *const  UT_Ref_OS_impl_task_table         = OS_impl_task_table;
size_t const UT_Ref_OS_impl_task_table_SIZE    = sizeof(OS_impl_task_table);
void *const  UT_Ref_OS_impl_task_tcb_pool      = OS_impl_task_tcb_pool;
size_t const UT_Ref_OS_impl_task_tcb_pool_SIZE = sizeof(OS_impl_task_tcb_pool);

void UT_TaskTest_SetImplTaskId(osal_index_t local_id, TaskHandle_t TaskId)
{
    OS_impl_task_table[local_id].id = TaskId;
}

TaskHandle_t UT_TaskTest_GetImplTaskId(osal_index_t local_id)
{
    return OS_impl_task_table[local_id].id;
}

void *UT_TaskTest_GetPoolTcb(uint32 pool_idx)
{
    return &OS_impl_task_tcb_pool[pool_idx].tcb;
}

StackType_t *UT_TaskTest_GetPoolStack(uint32 pool_idx)
{
    return OS_impl_task_tcb_pool[pool_idx].stack;
}

void UT_TaskTest_SetPoolStack(uint32 pool_idx, StackType_t *stack)
{
    OS_impl_task_tcb_pool[pool_idx].stack = stack;
}

bool UT_TaskTest_GetPoolInUse(uint32 pool_idx)
{
    return OS_impl_task_tcb_pool[pool_idx].in_use;
}

void UT_TaskTest_SetPoolInUse(uint32 pool_idx, bool in_use)
{
    OS_impl_task_tcb_pool[pool_idx].in_use = in_use;
}

void UT_TaskTest_SetAllPoolInUse(void)
{
    for (uint32 i = 0; i < OS_MAX_TASKS; ++i)
    {
        OS_impl_task_tcb_pool[i].in_use = true;
    }
}
