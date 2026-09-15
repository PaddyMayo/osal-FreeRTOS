/*
 * File: osal-shared-stubs.c
 *
 * Purpose:
 *   Minimal stand-ins for the pieces of OSAL's OS-agnostic shared layer
 *   (normally osapi-task.c) that freertos/src/os-impl-tasks.c references but
 *   this white-box test does not otherwise link - just enough to satisfy
 *   the linker, not a functional reimplementation.
 */

#include "os-shared-task.h"

/*
 * Real storage normally lives in osapi-task.c, which this test does not
 * link (it exercises os-impl-tasks.c in isolation).
 */
OS_task_internal_record_t OS_task_table[OS_MAX_TASKS];

/*
 * OS_TaskEntryPoint() is only ever invoked by a running FreeRTOS scheduler
 * calling into OS_FreeRTOSTaskEntry(), which this stub-based environment
 * does not simulate - this definition exists only to satisfy the linker.
 */
void OS_TaskEntryPoint(osal_id_t global_task_id)
{
    (void)global_task_id;
}
