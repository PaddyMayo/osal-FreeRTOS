#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "timers.h"

#if defined(OSAL_FREERTOS_PORT_POSIX)
/* The POSIX port's own function, declared in its portmacro.h */
void vPortThreadDying(void *pxTaskToDelete, volatile BaseType_t *pxPendYield);
#endif

#include "os-shared-task.h"
#include "os-shared-idmap.h"

#include "osconfig.h"

#include "os-impl-tasks.h"

OS_impl_task_internal_record_t OS_impl_task_table[OS_MAX_TASKS];
OS_impl_task_tcb_t             OS_impl_task_tcb_pool[OS_MAX_TASKS];

/* Counts the pool entries not in use; see OS_TaskCreate_Impl() */
static StaticSemaphore_t OS_FreeRTOS_TcbSemBuffer;
static SemaphoreHandle_t OS_FreeRTOS_TcbSem;

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
 *           Converts a wait in milliseconds to ticks such that the wait lasts
 *           at least that long. pdMS_TO_TICKS() truncates, so with a coarse
 *           tick (the POSIX platforms run at 100 Hz, i.e. 10 ms) a wait
 *           shorter than one tick would become zero ticks: vTaskDelay(0) only
 *           yields and a zero queue timeout polls. Zero stays zero (no wait);
 *           anything else gets one tick on top of the truncated value, which
 *           covers the sub-tick remainder and the partly elapsed tick the
 *           caller is already in. See prototype in os-impl-tasks.h.
 *
 *-----------------------------------------------------------------*/
TickType_t OS_FreeRTOS_MsToTicks(uint32 millisecond)
{
    if (millisecond == 0)
    {
        return 0;
    }

    return pdMS_TO_TICKS(millisecond) + 1;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Port-internal helper routine, not part of OSAL API.
 *           Creates the semaphore that counts the TCB pool entries not in
 *           use. See prototype in os-impl-tasks.h.
 *
 *-----------------------------------------------------------------*/
int32 OS_FreeRTOS_TaskInit(void)
{
    OS_FreeRTOS_TcbSem = xSemaphoreCreateCountingStatic(OS_MAX_TASKS, OS_MAX_TASKS, &OS_FreeRTOS_TcbSemBuffer);
    if (OS_FreeRTOS_TcbSem == NULL)
    {
        return OS_ERROR;
    }

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *           Frees the heap stack a pool entry holds, if any, then hands the
 *           entry back: marks it free and gives the semaphore that
 *           OS_TaskCreate_Impl waits on. Never blocks.
 *
 *-----------------------------------------------------------------*/
static void OS_FreeRTOS_TcbRelease(OS_impl_task_tcb_t *entry)
{
    if (entry->stack != NULL)
    {
        vPortFree(entry->stack);
        entry->stack = NULL;
    }

    taskENTER_CRITICAL();
    entry->in_use = false;
    taskEXIT_CRITICAL();

    (void)xSemaphoreGive(OS_FreeRTOS_TcbSem);
}

/*----------------------------------------------------------------
 *
 *  Purpose: Port-internal helper routine, not part of OSAL API.
 *           Called through portCLEAN_UP_TCB once the kernel is finished
 *           with a deleted task: releases the TCB pool entry the task held,
 *           and the heap stack OS_TaskCreate_Impl allocated for it, if any.
 *           Runs in the timer task for a task that deleted itself (see
 *           OS_FreeRTOS_DeleteSelf), or in the idle task if the timer queue
 *           was full, so it must not block. It leaves the task table slot alone: OSAL freed
 *           that when the task was deleted and it may belong to a newer task
 *           by now. See prototype in os-impl-tasks.h.
 *
 *-----------------------------------------------------------------*/
void OS_FreeRTOS_TaskCleanup(const void *tcb)
{
    uintptr_t offset;

    /*
     * The kernel only ever passes the address of a real TCB, and the ones this
     * port creates are the tcb members of the pool entries, so the entry
     * follows from the address. The unsigned subtraction wraps for an address
     * below the pool, leaving one range check for both ends. TCBs the port did
     * not create (idle, timer service) fall outside it.
     */
    offset = (uintptr_t)tcb - (uintptr_t)OS_impl_task_tcb_pool;
    if (offset >= sizeof(OS_impl_task_tcb_pool))
    {
        return;
    }

    OS_FreeRTOS_TcbRelease(&OS_impl_task_tcb_pool[offset / sizeof(OS_impl_task_tcb_pool[0])]);
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
    OS_impl_task_tcb_t             *entry;
    OS_task_internal_record_t      *task;
    configSTACK_DEPTH_TYPE          stack_depth;

    memset(&arg, 0, sizeof(arg));

    arg.id = OS_ObjectIdFromToken(token);

    task = OS_OBJECT_TABLE_GET(OS_task_table, *token);
    impl = OS_OBJECT_TABLE_GET(OS_impl_task_table, *token);

    /*
     * Wait for a TCB pool entry. A task that has exited still holds its entry
     * until the kernel finishes deleting it, which for a task that deleted
     * itself would be when the idle task next runs; OS_FreeRTOS_DeleteSelf()
     * has the timer task do it instead, as soon as the exiting task has handed
     * itself over (the idle task only if the timer queue was full). OSAL has
     * already given the table slot back, so a create right after an exit can
     * still find every entry taken; waiting here (no OSAL lock is held) lets
     * the timer task run and free one, instead of failing a create that has a
     * free slot. The wait is bounded because a task that never blocks can keep
     * the timer task (and the idle task) off the CPU; the create then fails
     * with OS_ERR_TRY_AGAIN and the caller may try again. The port cannot make
     * either run: every task must block now and then (a delay, or a wait on a
     * queue, semaphore or notification).
     */
    if (xSemaphoreTake(OS_FreeRTOS_TcbSem, OS_FreeRTOS_MsToTicks(OS_FREERTOS_TCB_WAIT_MS)) != pdTRUE)
    {
        return OS_ERR_TRY_AGAIN;
    }

    entry = NULL;

    taskENTER_CRITICAL();
    for (uint32 i = 0; i < OS_MAX_TASKS; ++i)
    {
        if (!OS_impl_task_tcb_pool[i].in_use)
        {
            entry         = &OS_impl_task_tcb_pool[i];
            entry->in_use = true;
            break;
        }
    }
    taskEXIT_CRITICAL();

    if (entry == NULL)
    {
        /* the semaphore counts free entries, so this cannot happen; do not use one we do not own */
        (void)xSemaphoreGive(OS_FreeRTOS_TcbSem);
        return OS_ERROR;
    }

    stack_depth = task->stack_size / sizeof(StackType_t);

    if (task->stack_pointer == OSAL_TASK_STACK_ALLOCATE)
    {
        entry->stack = pvPortMalloc(stack_depth * sizeof(StackType_t));
        if (entry->stack == NULL)
        {
            OS_FreeRTOS_TcbRelease(entry);
            return OS_ERROR;
        }
    }
    else
    {
        entry->stack = NULL;
    }

    impl->id = xTaskCreateStatic(OS_FreeRTOSTaskEntry,
                                 task->task_name,
                                 stack_depth,
                                 arg.opaque_arg,
                                 OS_FreeRTOS_PriorityRemap(task->priority),
                                 (entry->stack != NULL) ? entry->stack : (StackType_t *)task->stack_pointer,
                                 &entry->tcb);

    if (impl->id == NULL)
    {
        OS_FreeRTOS_TcbRelease(entry);
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
 *  Purpose: Local helper routine, not part of OSAL API.
 *           Runs in the timer task: deletes a task that handed itself over
 *           in OS_FreeRTOS_DeleteSelf(). Deleting another task makes the
 *           kernel free its TCB at once and call portCLEAN_UP_TCB, where a
 *           task deleting itself has to wait for the idle task to do that.
 *
 *-----------------------------------------------------------------*/
// The signature is the kernel's PendedFunction_t, the first parameter cannot be const
// cppcheck-suppress constParameterCallback
static void OS_FreeRTOS_DeleteHandedOver(void *pvParameter1, uint32_t ulParameter2)
{
    (void)ulParameter2;

    vTaskDelete((TaskHandle_t)pvParameter1);
}

/*----------------------------------------------------------------
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *           Ends the calling task without making the freeing of its TCB
 *           wait for the idle task, which a task that never blocks can keep
 *           off the CPU: the timer task, which runs at the highest priority,
 *           is asked to delete it, and it suspends itself until that
 *           happens. If the timer queue is full it deletes itself, and the
 *           idle task frees the TCB. Never returns.
 *
 *-----------------------------------------------------------------*/
static void OS_FreeRTOS_DeleteSelf(void)
{
    if (xTimerPendFunctionCall(OS_FreeRTOS_DeleteHandedOver, xTaskGetCurrentTaskHandle(), 0, 0) == pdTRUE)
    {
#if defined(OSAL_FREERTOS_PORT_POSIX)
        /*
         * The POSIX port runs each task on a pthread and deletes a task by cancelling
         * its thread. A thread that is only suspended is cancelled inside its wait on a
         * condition variable, and dies owning that wait's mutex, so whoever signals the
         * thread next depends on the host recovering a mutex whose owner has died. A
         * task marked as dying has its thread leave by itself as it switches away
         * instead, which is how the kernel itself ends a task that deletes itself
         * (portPRE_TASK_DELETE_HOOK), and nothing waits on that recovery. The other
         * ports have no thread to cancel and need nothing here.
         */
        vPortThreadDying(xTaskGetCurrentTaskHandle(), NULL);
#endif
        vTaskSuspend(NULL);
    }

    /* NULL tells FreeRTOS to delete the calling task */
    vTaskDelete(NULL);
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
    TaskHandle_t                    handle;

    impl   = OS_OBJECT_TABLE_GET(OS_impl_task_table, *token);
    handle = impl->id;

    impl->id = NULL;

    if (handle == xTaskGetCurrentTaskHandle())
    {
        const OS_task_internal_record_t *task;
        OS_object_token_t                local_token;
        osal_task_entry                  delete_hook;

        /*
         * A task deleting itself never gets back out of the delete, so
         * OS_TaskDelete() would never finish it or call the hook.
         * Do both here, in the order it does: release the slot (and with it
         * the lock held on it), then the hook, which must not run locked.
         * The TCB is cleaned up later through portCLEAN_UP_TCB.
         */
        task        = OS_OBJECT_TABLE_GET(OS_task_table, *token);
        delete_hook = task->delete_hook_pointer;
        local_token = *token;

        (void)OS_ObjectIdFinalizeDelete(OS_SUCCESS, &local_token);

        if (delete_hook != NULL)
        {
            delete_hook();
        }

        OS_FreeRTOS_DeleteSelf();
    }
    else
    {
        vTaskDelete(handle);
    }

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
    OS_FreeRTOS_DeleteSelf();
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_TaskDelay_Impl(uint32 millisecond)
{
    vTaskDelay(OS_FreeRTOS_MsToTicks(millisecond));

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
