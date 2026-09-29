#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "os-shared-task.h"
#include "os-shared-idmap.h"

#include "osconfig.h"

#include "os-impl-tasks.h"

OS_impl_task_internal_record_t OS_impl_task_table[OS_MAX_TASKS];

/*
 * Index into the per-task thread-local storage array (see
 * configNUM_THREAD_LOCAL_STORAGE_POINTERS in FreeRTOSConfig.h) used to stash
 * the OSAL task ID so OS_TaskGetId_Impl() can look it up for the calling task.
 */
#define OS_FREERTOS_TASK_ID_TLS_INDEX 0

/*
 *********************************************************************************
 *          TASK API
 *********************************************************************************
 */

/*----------------------------------------------------------------
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *
 *           unwraps the OSAL task ID and hands off to OS_TaskEntryPoint().
 *
 *-----------------------------------------------------------------*/
static void OS_FreeRTOSTaskEntry(void *pvParameters)
{
    OS_VoidPtrValueWrapper_t local_arg;

    memset(&local_arg, 0, sizeof(local_arg));
    local_arg.opaque_arg = pvParameters;
    OS_TaskEntryPoint(local_arg.id); /* Never returns */
}

/*----------------------------------------------------------------
 *
 *  Purpose: Port-internal helper routine, not part of OSAL API.
 *           Remaps the OSAL priority (0=highest .. OS_MAX_TASK_PRIORITY=lowest)
 *           into a FreeRTOS priority (0=lowest .. configMAX_PRIORITIES-1=highest).
 *           See prototype in os-impl-tasks.h.
 *
 *-----------------------------------------------------------------*/
UBaseType_t OS_FreeRTOS_PriorityRemap(osal_priority_t priority)
{
    return (UBaseType_t)(configMAX_PRIORITIES - 1)
           - (((UBaseType_t)priority * (configMAX_PRIORITIES - 1)) / OS_MAX_TASK_PRIORITY);
}

/*----------------------------------------------------------------
 *
 *  Purpose: Port-internal helper routine, not part of OSAL API.
 *           Called through portCLEAN_UP_TCB once the kernel is finished
 *           with a deleted task: frees the stack OS_TaskCreate_Impl
 *           allocated for it, if any, and marks the slot free for reuse.
 *           Runs in the idle task for a task that deleted itself, so it
 *           must not block. See prototype in os-impl-tasks.h.
 *
 *-----------------------------------------------------------------*/
void OS_FreeRTOS_TaskCleanup(const void *tcb)
{
    OS_impl_task_internal_record_t *impl;
    StackType_t                    *stack;

    for (uint32 i = 0; i < OS_MAX_TASKS; ++i)
    {
        impl = &OS_impl_task_table[i];

        if (tcb == &impl->tcb_buffer)
        {
            stack              = impl->stack_buffer;
            impl->stack_buffer = NULL;

            if (stack != NULL)
            {
                vPortFree(stack);
            }

            impl->id = NULL;
            break;
        }
    }
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_TaskMatch_Impl(const OS_object_token_t *token)
{
    const OS_impl_task_internal_record_t *impl;

    impl = OS_OBJECT_TABLE_GET(OS_impl_task_table, *token);

    if (xTaskGetCurrentTaskHandle() != impl->id)
    {
        return OS_ERROR;
    }

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_TaskCreate_Impl(const OS_object_token_t *token, uint32 flags)
{
    OS_VoidPtrValueWrapper_t        arg;
    OS_impl_task_internal_record_t *impl;
    OS_task_internal_record_t      *task;
    configSTACK_DEPTH_TYPE          stack_depth;

    memset(&arg, 0, sizeof(arg));

    arg.id = OS_ObjectIdFromToken(token);

    task = OS_OBJECT_TABLE_GET(OS_task_table, *token);
    impl = OS_OBJECT_TABLE_GET(OS_impl_task_table, *token);

    if (impl->id != NULL)
    {
        return OS_ERROR;
    }

    stack_depth = task->stack_size / sizeof(StackType_t);

    if (task->stack_pointer == OSAL_TASK_STACK_ALLOCATE)
    {
        impl->stack_buffer = pvPortMalloc(stack_depth * sizeof(StackType_t));
        if (impl->stack_buffer == NULL)
        {
            return OS_ERROR;
        }
    }
    else
    {
        impl->stack_buffer = NULL;
    }

    impl->id = xTaskCreateStatic(OS_FreeRTOSTaskEntry,
                                 task->task_name,
                                 stack_depth,
                                 arg.opaque_arg,
                                 OS_FreeRTOS_PriorityRemap(task->priority),
                                 (impl->stack_buffer != NULL) ? impl->stack_buffer : (StackType_t *)task->stack_pointer,
                                 &impl->tcb_buffer);

    if (impl->id == NULL)
    {
        if (impl->stack_buffer != NULL)
        {
            vPortFree(impl->stack_buffer);
            impl->stack_buffer = NULL;
        }

        return OS_ERROR;
    }

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_TaskDetach_Impl(const OS_object_token_t *token)
{
    /* FreeRTOS tasks have no join/detach concept, so this is a no-op. */
    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_TaskDelete_Impl(const OS_object_token_t *token)
{
    OS_impl_task_internal_record_t *impl;

    impl = OS_OBJECT_TABLE_GET(OS_impl_task_table, *token);

    vTaskDelete(impl->id);

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
void OS_TaskExit_Impl(void)
{
    /* NULL tells FreeRTOS to delete the calling task */
    vTaskDelete(NULL);
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_TaskDelay_Impl(uint32 millisecond)
{
    vTaskDelay(pdMS_TO_TICKS(millisecond));

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_TaskSetPriority_Impl(const OS_object_token_t *token, osal_priority_t new_priority)
{
    OS_impl_task_internal_record_t *impl;

    impl = OS_OBJECT_TABLE_GET(OS_impl_task_table, *token);

    vTaskPrioritySet(impl->id, OS_FreeRTOS_PriorityRemap(new_priority));

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
osal_id_t OS_TaskGetId_Impl(void)
{
    OS_VoidPtrValueWrapper_t arg;

    memset(&arg, 0, sizeof(arg));

    arg.opaque_arg = pvTaskGetThreadLocalStoragePointer(NULL, OS_FREERTOS_TASK_ID_TLS_INDEX);

    return arg.id;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
// task_prop must stay non-const to match the os-shared-task.h prototype
// cppcheck-suppress constParameterPointer
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
    OS_VoidPtrValueWrapper_t arg;

    memset(&arg, 0, sizeof(arg));
    arg.id = global_task_id;

    vTaskSetThreadLocalStoragePointer(NULL, OS_FREERTOS_TASK_ID_TLS_INDEX, arg.opaque_arg);

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
// ref must stay non-const to match the os-shared-task.h prototype
// cppcheck-suppress constParameterPointer
bool OS_TaskIdMatchSystemData_Impl(void *ref, const OS_object_token_t *token, const OS_common_record_t *obj)
{
    const TaskHandle_t                   *target = (const TaskHandle_t *)ref;
    const OS_impl_task_internal_record_t *impl;

    impl = OS_OBJECT_TABLE_GET(OS_impl_task_table, *token);

    return (*target == impl->id);
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_TaskValidateSystemData_Impl(const void *sysdata, size_t sysdata_size)
{
    if (sysdata == NULL || sysdata_size != sizeof(TaskHandle_t))
    {
        return OS_INVALID_POINTER;
    }

    return OS_SUCCESS;
}
