/*
 * File: osal-shared-stubs.c
 *
 * Purpose:
 *   Minimal stand-ins for the pieces of OSAL's OS-agnostic shared layer
 *   (normally osapi-common.c, osapi-task.c, osapi-queue.c) that the freertos/src/os-impl-*.c
 *   modules reference but these white-box tests do not otherwise link - just
 *   enough to satisfy the linker, not a functional reimplementation.
 */

#include "os-shared-common.h"
#include "os-shared-task.h"
#include "os-shared-queue.h"

/* Normally defined in osapi-common.c */
OS_SharedGlobalVars_t OS_SharedGlobalVars;

/*
 * Real storage normally lives in osapi-task.c, which this test does not
 * link (it exercises os-impl-tasks.c in isolation).
 */
OS_task_internal_record_t OS_task_table[OS_MAX_TASKS];

/* Likewise, normally defined in osapi-queue.c */
OS_queue_internal_record_t OS_queue_table[OS_MAX_QUEUES];

/*
 * Normally reached from a running FreeRTOS scheduler calling into
 * OS_FreeRTOSTaskEntry(); coverage tests call that trampoline directly
 * instead, and check the id it unwrapped via UT_Stub_LastEntryPointId.
 * Unlike the real one, this returns.
 */
osal_id_t UT_Stub_LastEntryPointId;

void OS_TaskEntryPoint(osal_id_t global_task_id)
{
    UT_Stub_LastEntryPointId = global_task_id;
}
