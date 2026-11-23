/**
 * \file
 *
 * Purpose: OSAL message queue implementation on top of FreeRTOS queues.
 *
 *          Each OSAL queue maps to one FreeRTOS queue, created with the OSAL
 *          max_depth and max_size as its length and fixed item size. Because
 *          FreeRTOS always copies whole items, Get and Put reject buffers
 *          smaller than max_size with OS_QUEUE_INVALID_SIZE, and Get always
 *          reports max_size as the size copied. Put never blocks; Get maps
 *          OS_PEND / OS_CHECK / millisecond timeouts onto FreeRTOS ticks.
 *
 *          Get and Put look the queue up without a lock, so they take
 *          OSAL's own refcount on it for as long as they are inside the
 *          kernel call (OS_FreeRTOS_WaitEnter()/WaitLeave(), see os-freertos.h).
 *          OS_QueueDelete() takes the queue exclusively, which waits for that
 *          refcount to reach zero (the blocked Gets are woken by
 *          OS_WaitForStateChange_Impl() and return OS_ERROR), so
 *          OS_QueueDelete_Impl() only runs with no user inside the queue and
 *          can free it - vQueueDelete() would otherwise free a queue that tasks
 *          are still waiting on.
 */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "os-shared-queue.h"
#include "os-shared-idmap.h"

#include "os-freertos.h"
#include "os-impl-queue.h"

OS_impl_queue_internal_record_t OS_impl_queue_table[OS_MAX_QUEUES];

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_QueueCreate_Impl(const OS_object_token_t *token, uint32 flags)
{
    OS_impl_queue_internal_record_t *impl;
    OS_queue_internal_record_t      *queue;

    impl  = OS_OBJECT_TABLE_GET(OS_impl_queue_table, *token);
    queue = OS_OBJECT_TABLE_GET(OS_queue_table, *token);

    impl->id = xQueueCreate(queue->max_depth, queue->max_size);
    if (impl->id == NULL)
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
int32 OS_QueueDelete_Impl(const OS_object_token_t *token)
{
    OS_impl_queue_internal_record_t *impl;

    impl = OS_OBJECT_TABLE_GET(OS_impl_queue_table, *token);

    /*
     * Safe to free the queue: OS_QueueDelete() holds the object exclusively,
     * which OSAL grants only once the refcount is zero, and every Get and Put
     * holds a refcount while it is inside. No task can be using impl->id.
     */
    vQueueDelete(impl->id);
    impl->id = NULL;

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_QueueGet_Impl(const OS_object_token_t *token, void *data, size_t size, size_t *size_copied, int32 timeout)
{
    OS_impl_queue_internal_record_t  *impl;
    const OS_queue_internal_record_t *queue;
    OS_impl_wait_user_t               user;
    TickType_t                        ticks;
    size_t                            max_size;
    BaseType_t                        received;
    int32                             status;
    bool                              deleted;

    impl  = OS_OBJECT_TABLE_GET(OS_impl_queue_table, *token);
    queue = OS_OBJECT_TABLE_GET(OS_queue_table, *token);

    /*
     * xQueueReceive() always copies exactly the item size the queue was
     * created with - if the caller's buffer is smaller than that, it would
     * overflow, so this has to be checked here before ever calling it.
     */
    max_size = queue->max_size;

    if (size < max_size)
    {
        *size_copied = 0;
        return OS_QUEUE_INVALID_SIZE;
    }

    if (timeout == OS_PEND)
    {
        ticks = portMAX_DELAY;
    }
    else if (timeout == OS_CHECK)
    {
        ticks = 0;
    }
    else
    {
        ticks = OS_FreeRTOS_MsToTicks(timeout);
    }

    status = OS_FreeRTOS_WaitEnter(token, &user);
    if (status != OS_SUCCESS)
    {
        *size_copied = 0;
        return status;
    }

    received = xQueueReceive(impl->id, data, ticks);
    deleted  = OS_FreeRTOS_WaitLeave(&user);

    if (received != pdPASS)
    {
        *size_copied = 0;

        /* The delete woke this Get, it did not time out */
        if (deleted)
        {
            return OS_ERROR;
        }

        if (timeout == OS_CHECK)
        {
            return OS_QUEUE_EMPTY;
        }

        return OS_QUEUE_TIMEOUT;
    }

    /*
     * Not queue->max_size: the refcount is given back by now, so a delete may
     * have run and reset the shared record. The copy taken before entering is
     * the one this Get was sized against.
     */
    *size_copied = max_size;

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_QueuePut_Impl(const OS_object_token_t *token, const void *data, size_t size, uint32 flags)
{
    OS_impl_queue_internal_record_t  *impl;
    const OS_queue_internal_record_t *queue;
    OS_impl_wait_user_t               user;
    BaseType_t                        sent;
    int32                             status;

    impl  = OS_OBJECT_TABLE_GET(OS_impl_queue_table, *token);
    queue = OS_OBJECT_TABLE_GET(OS_queue_table, *token);

    /*
     * xQueueSend() always copies exactly the item size the queue was
     * created with - if the caller's message is smaller than that, it would
     * read out of bounds, so this has to be checked here before ever calling it.
     */
    if (size < queue->max_size)
    {
        return OS_QUEUE_INVALID_SIZE;
    }

    status = OS_FreeRTOS_WaitEnter(token, &user);
    if (status != OS_SUCCESS)
    {
        return status;
    }

    /* OS_QueuePut() is documented as non-blocking, so use a zero timeout. */
    sent = xQueueSend(impl->id, data, 0);
    (void)OS_FreeRTOS_WaitLeave(&user);

    if (sent != pdPASS)
    {
        return OS_QUEUE_FULL;
    }

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
// queue_prop must stay non-const to match the os-shared-queue.h prototype
// cppcheck-suppress constParameterPointer
int32 OS_QueueGetInfo_Impl(const OS_object_token_t *token, OS_queue_prop_t *queue_prop)
{
    /* No extra info for queues in the OS implementation */
    return OS_SUCCESS;
}
