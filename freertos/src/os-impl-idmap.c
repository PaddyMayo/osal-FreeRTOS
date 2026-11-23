/**
 * \file
 *
 * Purpose: OSAL id map implementation on top of FreeRTOS.
 *
 *          OSAL guards each object type's table with a lock it calls through
 *          OS_Lock_Global_Impl() and OS_Unlock_Global_Impl(), and a delete that
 *          finds the object still in use waits for it through
 *          OS_WaitForStateChange_Impl(). This file provides those three, one
 *          lock per object type.
 *
 *          A table lock is a held flag and an owner, changed only inside a
 *          critical section, not a FreeRTOS mutex or semaphore. A mutex is
 *          owned by the task that took it, so nobody else could give it back
 *          for a task that was deleted while holding it. A semaphore has no
 *          owner in the kernel, but it cannot be taken and have its owner
 *          recorded atomically, so a task deleted in between leaked the lock.
 *          With the flag, taking and recording the owner is one step that a
 *          deletion cannot split, and the task death hook releases the lock
 *          of a dead owner. The price is that a contended lock is polled, one
 *          tick at a time, instead of blocked on.
 *
 *          This file also keeps the wait users described in os-freertos.h: the
 *          blocking calls that hold a refcount on their object, which
 *          OS_WaitForStateChange_Impl() wakes once the object is being deleted.
 */

#include "FreeRTOS.h"
#include "task.h"

#include "osapi-idmap.h"
#include "os-shared-idmap.h"

#include "os-freertos.h"

/* One lock per object type below OS_OBJECT_TYPE_USER */
typedef struct
{
    bool         created; /* set by OS_FreeRTOS_TableMutex_Init(); a lock not created is not taken */
    bool         held;    /* the lock is taken; owner can be NULL before the scheduler starts, so it cannot say */
    TaskHandle_t owner;   /* the task that holds the lock, when held */
} OS_FreeRTOS_TableLock_t;

static OS_FreeRTOS_TableLock_t OS_FreeRTOS_TableLocks[OS_OBJECT_TYPE_USER];

/* Every blocking call that holds a refcount on its object, newest first; see os-freertos.h */
static OS_impl_wait_user_t *OS_FreeRTOS_WaitUsers;

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *           An object type with no lock (not below OS_OBJECT_TYPE_USER, or
 *           not created yet: OS_ObjectIdInit() runs before
 *           OS_API_Impl_Init()) is not guarded, which is safe because
 *           nothing runs concurrently that early.
 *
 *           Taking the lock and recording the owner is one critical section,
 *           so a task deleted at any point either has not taken the lock or
 *           is already its recorded owner, where the death hook finds it. A
 *           lock found held is polled once per tick.
 *
 *-----------------------------------------------------------------*/
void OS_Lock_Global_Impl(osal_objtype_t idtype)
{
    OS_FreeRTOS_TableLock_t *lock;
    bool                     taken;

    if (idtype >= OS_OBJECT_TYPE_USER)
    {
        return;
    }

    lock = &OS_FreeRTOS_TableLocks[idtype];
    if (!lock->created)
    {
        return;
    }

    taken = false;
    while (!taken)
    {
        taskENTER_CRITICAL();
        if (!lock->held)
        {
            lock->held  = true;
            lock->owner = xTaskGetCurrentTaskHandle();
            taken       = true;
        }
        taskEXIT_CRITICAL();

        if (!taken)
        {
            /* Held by another task: let it run, and look again next tick */
            vTaskDelay(1);
        }
    }
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
void OS_Unlock_Global_Impl(osal_objtype_t idtype)
{
    OS_FreeRTOS_TableLock_t *lock;

    if (idtype >= OS_OBJECT_TYPE_USER)
    {
        return;
    }

    lock = &OS_FreeRTOS_TableLocks[idtype];
    if (!lock->created)
    {
        return;
    }

    taskENTER_CRITICAL();
    lock->held  = false;
    lock->owner = NULL;
    taskEXIT_CRITICAL();
}

/*----------------------------------------------------------------
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *           Tells whether the record a wait user holds a refcount on reads
 *           reserved, which is what a delete in progress leaves in it.
 *
 *-----------------------------------------------------------------*/
static bool OS_FreeRTOS_WaitRecordReserved(const OS_impl_wait_user_t *user)
{
    /* OS_OBJECT_ID_RESERVED is OSAL's own constant, written without the U suffix rule 7.2 asks for */
    // cppcheck-suppress misra-c2012-7.2
    return OS_ObjectIdEqual(OS_ObjectIdGlobalFromToken(&user->token)->active_id, OS_OBJECT_ID_RESERVED);
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *           The caller owns the type's lock. It is given up while this
 *           waits, so whoever the caller waits for can finish, and taken
 *           again before returning. A delete that retries after the wait
 *           finds the users it woke gone. xTaskAbortDelay() only unblocks a
 *           task, it never blocks, so it is safe inside the critical section
 *           that keeps the nodes (which live on the users' stacks) from
 *           vanishing. A node whose token was already released (its task is
 *           leaving, see OS_FreeRTOS_WaitLeave()) is skipped: its refcount is
 *           given back, and the task may have moved on to a block that is
 *           not this one's.
 *
 *-----------------------------------------------------------------*/
void OS_WaitForStateChange_Impl(osal_objtype_t objtype, uint32 attempts)
{
    const OS_impl_wait_user_t *user;
    uint32                     millisecond;

    OS_Unlock_Global_Impl(objtype);

    taskENTER_CRITICAL();
    for (user = OS_FreeRTOS_WaitUsers; user != NULL; user = user->next)
    {
        /* A record that reads reserved belongs to an object being deleted */
        if ((user->token.obj_type == objtype) && (user->token.lock_mode != OS_LOCK_MODE_NONE)
            && OS_FreeRTOS_WaitRecordReserved(user))
        {
            (void)xTaskAbortDelay(user->task);
        }
    }
    taskEXIT_CRITICAL();

    if (attempts <= 10U)
    {
        /* Wait an increasing amount of time, starting at 10ms */
        millisecond = attempts * attempts * 10U;
    }
    else
    {
        /* wait 1 second (max for polling) */
        millisecond = 1000U;
    }

    vTaskDelay(OS_FreeRTOS_MsToTicks(millisecond));

    OS_Lock_Global_Impl(objtype);
}

/*----------------------------------------------------------------
 *
 *  Purpose: Port-internal helper routine, not part of OSAL API.
 *           See prototype in os-freertos.h.
 *
 *-----------------------------------------------------------------*/
int32 OS_FreeRTOS_WaitEnter(const OS_object_token_t *token, OS_impl_wait_user_t *user)
{
    int32 status;

    /* Every wait on a kernel object starts here: a task that deleted itself ends now */
    OS_FreeRTOS_Enter();

    status = OS_ObjectIdGetById(OS_LOCK_MODE_REFCOUNT, token->obj_type, token->obj_id, &user->token);
    if (status != OS_SUCCESS)
    {
        return status;
    }

    user->task = xTaskGetCurrentTaskHandle();

    taskENTER_CRITICAL();
    user->next            = OS_FreeRTOS_WaitUsers;
    OS_FreeRTOS_WaitUsers = user;
    taskEXIT_CRITICAL();

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Port-internal helper routine, not part of OSAL API.
 *           See prototype in os-freertos.h.
 *
 *-----------------------------------------------------------------*/
bool OS_FreeRTOS_WaitLeave(OS_impl_wait_user_t *user)
{
    bool deleted;

    /* Sampled before the refcount goes back: once it does, the delete may finish and recycle the record */
    deleted = OS_FreeRTOS_WaitRecordReserved(user);

    /*
     * The refcount goes back first and the node is unlisted second. OS_ObjectIdRelease() marks the token released
     * (OS_LOCK_MODE_NONE) when it is done, so a task deleted between the two leaves a node the death hook gives
     * back as a no-op, and which the wake scan skips. The reverse order would leak the refcount if the task died
     * between them. The one window left, a task deleted inside OS_ObjectIdRelease() itself between its decrement
     * and marking the token, is accepted.
     */
    OS_ObjectIdRelease(&user->token);

    taskENTER_CRITICAL();
    {
        /* The list is singly linked: find the slot that points at the node, so the head needs no special case */
        OS_impl_wait_user_t **link = &OS_FreeRTOS_WaitUsers;

        while (*link != user)
        {
            link = &(*link)->next;
        }

        *link = user->next;
    }
    taskEXIT_CRITICAL();

    return deleted;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *           Unlists one node of a task and returns it, or NULL if the task
 *           has none left.
 *
 *-----------------------------------------------------------------*/
static OS_impl_wait_user_t *OS_FreeRTOS_WaitUnlinkTask(TaskHandle_t task)
{
    OS_impl_wait_user_t **link;
    OS_impl_wait_user_t  *found;

    found = NULL;

    taskENTER_CRITICAL();
    link = &OS_FreeRTOS_WaitUsers;
    while ((*link != NULL) && (found == NULL))
    {
        if ((*link)->task == task)
        {
            found = *link;
            *link = found->next;
        }
        else
        {
            link = &(*link)->next;
        }
    }
    taskEXIT_CRITICAL();

    return found;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *           The task death hook. Releases the table locks the dead task
 *           held, so it cannot freeze the system, and then gives back the
 *           refcounts its wait users held, so a delete is not left waiting
 *           for them (a node already released is a no-op to release again).
 *           The locks go first: giving a refcount back takes the table lock.
 *
 *-----------------------------------------------------------------*/
static void OS_FreeRTOS_IdmapTaskDied(TaskHandle_t task)
{
    osal_objtype_t       idtype;
    OS_impl_wait_user_t *user;

    taskENTER_CRITICAL();
    for (idtype = 0; idtype < OS_OBJECT_TYPE_USER; ++idtype)
    {
        if (OS_FreeRTOS_TableLocks[idtype].held && (OS_FreeRTOS_TableLocks[idtype].owner == task))
        {
            OS_FreeRTOS_TableLocks[idtype].held  = false;
            OS_FreeRTOS_TableLocks[idtype].owner = NULL;
        }
    }
    taskEXIT_CRITICAL();

    /* One node at a time: giving the refcount back may block, which a critical section must not */
    user = OS_FreeRTOS_WaitUnlinkTask(task);
    while (user != NULL)
    {
        OS_ObjectIdRelease(&user->token);
        user = OS_FreeRTOS_WaitUnlinkTask(task);
    }
}

/*----------------------------------------------------------------
 *
 *  Purpose: Port-internal helper routine, not part of OSAL API.
 *           See prototype in os-freertos.h.
 *
 *-----------------------------------------------------------------*/
int32 OS_FreeRTOS_TableMutex_Init(void)
{
    osal_objtype_t idtype;

    /* OS_API_Init() can run again: no lock is held, no user is waiting */
    OS_FreeRTOS_WaitUsers = NULL;

    for (idtype = 0; idtype < OS_OBJECT_TYPE_USER; ++idtype)
    {
        OS_FreeRTOS_TableLocks[idtype].created = true;
        OS_FreeRTOS_TableLocks[idtype].held    = false;
        OS_FreeRTOS_TableLocks[idtype].owner   = NULL;
    }

    return OS_FreeRTOS_OnTaskDied(OS_FreeRTOS_IdmapTaskDied);
}
