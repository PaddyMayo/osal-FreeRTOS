#include "FreeRTOS.h"
#include "task.h"

#include "os-shared-task.h"
#include "os-shared-idmap.h"

/*
 * TODO: Extra per-task bookkeeping needed by this port (e.g. the FreeRTOS
 * TaskHandle_t associated with each OSAL task) should be defined here or in
 * a dedicated "os-impl-tasks.h" header, following the OS_impl_task_internal_record_t
 * pattern used by the other OSAL ports.
 */

/*
 *********************************************************************************
 *          TASK API
 *********************************************************************************
 */

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_TaskMatch_Impl(const OS_object_token_t *token)
{
    /* TODO: compare the calling task's FreeRTOS handle (xTaskGetCurrentTaskHandle)
     * against the handle stored for this token, return OS_SUCCESS on match.
     */
    return OS_ERR_NOT_IMPLEMENTED;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_TaskCreate_Impl(const OS_object_token_t *token, uint32 flags)
{
    /* TODO: create the task via xTaskCreate()/xTaskCreateStatic(), using the
     * OS_task_internal_record_t fields (entry_function_pointer, stack_size,
     * priority, stack_pointer, entry_arg) obtained via OS_OBJECT_TABLE_GET.
     * The task entry point must call OS_TaskEntryPoint() with the OSAL task ID.
     */
    return OS_ERR_NOT_IMPLEMENTED;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_TaskDetach_Impl(const OS_object_token_t *token)
{
    /* TODO: FreeRTOS tasks have no join/detach concept - determine whether
     * this should simply be a no-op returning OS_SUCCESS.
     */
    return OS_ERR_NOT_IMPLEMENTED;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_TaskDelete_Impl(const OS_object_token_t *token)
{
    /* TODO: delete the task via vTaskDelete(), using the handle stored for
     * this token.
     */
    return OS_ERR_NOT_IMPLEMENTED;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
void OS_TaskExit_Impl(void)
{
    /* TODO: call vTaskDelete(NULL) to delete the calling task. Does not return. */
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_TaskDelay_Impl(uint32 millisecond)
{
    /* TODO: block via vTaskDelay(pdMS_TO_TICKS(millisecond)). */
    return OS_ERR_NOT_IMPLEMENTED;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_TaskSetPriority_Impl(const OS_object_token_t *token, osal_priority_t new_priority)
{
    /* TODO: remap the OSAL priority (0=highest .. OS_MAX_TASK_PRIORITY=lowest)
     * to a FreeRTOS priority and apply it via vTaskPrioritySet().
     */
    return OS_ERR_NOT_IMPLEMENTED;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
osal_id_t OS_TaskGetId_Impl(void)
{
    /* TODO: retrieve the OSAL task ID associated with the calling task, e.g.
     * via a FreeRTOS thread-local pointer set in OS_TaskRegister_Impl
     * (pvTaskGetThreadLocalStoragePointer) or a lookup keyed by
     * xTaskGetCurrentTaskHandle().
     */
    return OS_OBJECT_ID_UNDEFINED;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_TaskGetInfo_Impl(const OS_object_token_t *token, OS_task_prop_t *task_prop)
{
    /* TODO: populate any OS-specific fields of task_prop. */
    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_TaskRegister_Impl(osal_id_t global_task_id)
{
    /* TODO: store global_task_id somewhere retrievable from OS_TaskGetId_Impl,
     * e.g. via vTaskSetThreadLocalStoragePointer().
     */
    return OS_ERR_NOT_IMPLEMENTED;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
bool OS_TaskIdMatchSystemData_Impl(void *ref, const OS_object_token_t *token, const OS_common_record_t *obj)
{
    /* TODO: compare the supplied system data (a TaskHandle_t) against the
     * handle stored for this token.
     */
    return false;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_TaskValidateSystemData_Impl(const void *sysdata, size_t sysdata_size)
{
    /* TODO: verify sysdata/sysdata_size are consistent with a TaskHandle_t. */
    if (sysdata == NULL || sysdata_size != sizeof(TaskHandle_t))
    {
        return OS_INVALID_POINTER;
    }

    return OS_SUCCESS;
}
