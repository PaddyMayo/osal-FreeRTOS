/*
 * File: os-impl-tasks.h
 *
 * Purpose:
 *   Internal task table definitions, and the port-internal priority mapping
 *   helper, for the FreeRTOS OSAL port.
 *
 *   These live in a header rather than staying file-local to os-impl-tasks.c
 *   so the coverage-test adaptor can reach the table through the real type
 *   instead of a duplicate declaration that could silently drift out of sync
 *   with it - the same reason OSAL's own ports expose theirs this way (see
 *   third_party/osal/src/os/vxworks/inc/os-impl-tasks.h).
 */

#ifndef OS_IMPL_TASKS_H
#define OS_IMPL_TASKS_H

#include "osconfig.h"
#include "osapi-task.h"

#include "FreeRTOS.h"
#include "task.h"

typedef struct
{
    TaskHandle_t id;
    StaticTask_t tcb_buffer;
    StackType_t *stack_buffer;
} OS_impl_task_internal_record_t;

/* Table where the OS object information is stored */
extern OS_impl_task_internal_record_t OS_impl_task_table[OS_MAX_TASKS];

/*
 * Remaps an OSAL priority (0=highest .. OS_MAX_TASK_PRIORITY=lowest) into a
 * FreeRTOS priority (0=lowest .. configMAX_PRIORITIES-1=highest).
 *
 * Exposed to the rest of the port (not just OS_TaskCreate_Impl) because other
 * modules also spawn FreeRTOS tasks from an OSAL priority - the console writer
 * task at OS_UTILITYTASK_PRIORITY, for one - and they must land on the same
 * FreeRTOS priority scale as application tasks.
 */
UBaseType_t OS_FreeRTOS_PriorityRemap(osal_priority_t priority);

void OS_FreeRTOS_TaskCleanup(const void *tcb);

#endif /* OS_IMPL_TASKS_H */
