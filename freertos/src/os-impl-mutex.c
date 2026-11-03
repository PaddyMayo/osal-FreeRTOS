/**
 * \file
 *
 * Purpose: OSAL mutex implementation on top of FreeRTOS recursive mutexes.
 *
 *          OSAL mutexes are recursive (the POSIX port sets
 *          PTHREAD_MUTEX_RECURSIVE, and OSAL's mutex-test takes one twice), so
 *          each maps to a FreeRTOS recursive mutex and must be taken and given
 *          with the *Recursive calls, never xSemaphoreTake()/xSemaphoreGive().
 *          FreeRTOS recursive mutexes also inherit priority, as the POSIX port
 *          asks of its mutexes with PTHREAD_PRIO_INHERIT.
 *
 *          Ownership is not checked here: OS_MutSemGive() in the shared layer
 *          warns when the wrong task gives, and FreeRTOS itself refuses a give
 *          from a task that is not the holder.
 */

#include "FreeRTOS.h"
#include "semphr.h"

#include "os-shared-mutex.h"
#include "os-shared-idmap.h"

#include "os-impl-mutex.h"

OS_impl_mutex_internal_record_t OS_impl_mutex_table[OS_MAX_MUTEXES];

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_MutSemCreate_Impl(const OS_object_token_t *token, uint32 options)
{
    OS_impl_mutex_internal_record_t *impl;

    impl = OS_OBJECT_TABLE_GET(OS_impl_mutex_table, *token);

    impl->id = xSemaphoreCreateRecursiveMutex();
    if (impl->id == NULL)
    {
        return OS_SEM_FAILURE;
    }

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_MutSemDelete_Impl(const OS_object_token_t *token)
{
    OS_impl_mutex_internal_record_t *impl;

    impl = OS_OBJECT_TABLE_GET(OS_impl_mutex_table, *token);

    vSemaphoreDelete(impl->id);

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_MutSemGive_Impl(const OS_object_token_t *token)
{
    OS_impl_mutex_internal_record_t *impl;

    impl = OS_OBJECT_TABLE_GET(OS_impl_mutex_table, *token);

    if (xSemaphoreGiveRecursive(impl->id) != pdTRUE)
    {
        return OS_SEM_FAILURE;
    }

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_MutSemTake_Impl(const OS_object_token_t *token)
{
    OS_impl_mutex_internal_record_t *impl;

    impl = OS_OBJECT_TABLE_GET(OS_impl_mutex_table, *token);

    if (xSemaphoreTakeRecursive(impl->id, portMAX_DELAY) != pdTRUE)
    {
        return OS_SEM_FAILURE;
    }

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
// mut_prop must stay non-const to match the os-shared-mutex.h prototype
// cppcheck-suppress constParameterPointer
int32 OS_MutSemGetInfo_Impl(const OS_object_token_t *token, OS_mut_sem_prop_t *mut_prop)
{
    /* FreeRTOS has nothing to add to the name and creator the shared layer fills in */
    return OS_SUCCESS;
}
