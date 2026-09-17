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

void *const  UT_Ref_OS_impl_task_table      = OS_impl_task_table;
size_t const UT_Ref_OS_impl_task_table_SIZE = sizeof(OS_impl_task_table);

void UT_TaskTest_SetImplTaskId(osal_index_t local_id, TaskHandle_t TaskId)
{
    OS_impl_task_table[local_id].id = TaskId;
}
