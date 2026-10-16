/*
 * File: ut-adaptor-tasks.h
 *
 * Purpose:
 *   Test-only accessors into os-impl-tasks.c's internal state, for coverage
 *   test cases that need to set up a precondition the module's public _Impl
 *   API cannot reach directly.
 *
 *   Keeping these here (rather than in the test case file) confines all
 *   knowledge of the internal table to one place, and mirrors the adaptor
 *   layout OSAL's own coverage tests use - see
 *   third_party/osal/src/unit-test-coverage/vxworks/adaptors/.
 */

#ifndef UT_ADAPTOR_TASKS_H
#define UT_ADAPTOR_TASKS_H

#include "common_types.h"

#include "FreeRTOS.h"
#include "task.h"

/* Opaque handles to OS_impl_task_table and OS_impl_task_tcb_pool, for resetting them between test cases */
extern void *const  UT_Ref_OS_impl_task_table;
extern size_t const UT_Ref_OS_impl_task_table_SIZE;
extern void *const  UT_Ref_OS_impl_task_tcb_pool;
extern size_t const UT_Ref_OS_impl_task_tcb_pool_SIZE;

void         UT_TaskTest_SetImplTaskId(osal_index_t local_id, TaskHandle_t TaskId);
TaskHandle_t UT_TaskTest_GetImplTaskId(osal_index_t local_id);

/* TCB pool entries, by pool index (not task table slot) */
void        *UT_TaskTest_GetPoolTcb(uint32 pool_idx);
StackType_t *UT_TaskTest_GetPoolStack(uint32 pool_idx);
void         UT_TaskTest_SetPoolStack(uint32 pool_idx, StackType_t *stack);
bool         UT_TaskTest_GetPoolInUse(uint32 pool_idx);
void         UT_TaskTest_SetPoolInUse(uint32 pool_idx, bool in_use);
void         UT_TaskTest_SetAllPoolInUse(void);

#endif /* UT_ADAPTOR_TASKS_H */
