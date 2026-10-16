/*
 * File: osal-shared-stubs.c
 *
 * Purpose:
 *   Minimal stand-ins for the pieces of OSAL's OS-agnostic shared layer
 *   (normally osapi-common.c, osapi-task.c) that the freertos/src/os-impl-*.c
 *   modules reference but these white-box tests do not otherwise link - just
 *   enough to satisfy the linker, not a functional reimplementation.
 *   Modules listed in FREERTOS_OSAL_STUB_MODULES (../CMakeLists.txt) skip this
 *   file and link OSAL's own ut_osapi_*_stubs instead.
 */

#include "os-shared-common.h"
#include "os-shared-idmap.h"
#include "os-shared-task.h"

/* Normally defined in osapi-common.c */
OS_SharedGlobalVars_t OS_SharedGlobalVars;

/*
 * Real storage normally lives in osapi-task.c, which this test does not
 * link (it exercises os-impl-tasks.c in isolation).
 */
OS_task_internal_record_t OS_task_table[OS_MAX_TASKS];

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

/*
 * Normally in osapi-idmap.c. OS_TaskDelete_Impl() calls it directly when a
 * task deletes itself, since OS_TaskDelete() never regains control then. Only
 * the call is recorded; tests that care whether it happened before another
 * step read the count from inside that step.
 */
uint32 UT_Stub_FinalizeDeleteCount;
int32  UT_Stub_LastFinalizeDeleteStatus;
uint32 UT_Stub_LastFinalizeDeleteIndex;

// token must stay non-const to match the os-shared-idmap.h prototype
// cppcheck-suppress constParameterPointer
int32 OS_ObjectIdFinalizeDelete(int32 operation_status, OS_object_token_t *token)
{
    ++UT_Stub_FinalizeDeleteCount;
    UT_Stub_LastFinalizeDeleteStatus = operation_status;
    UT_Stub_LastFinalizeDeleteIndex  = token->obj_idx;

    return operation_status;
}
