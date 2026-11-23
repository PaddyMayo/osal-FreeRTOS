/*
 * File: os-freertos.h
 *
 * Purpose:
 *   Declarations shared by the modules of the FreeRTOS OSAL port that are not
 *   part of any OSAL contract (os-shared-*.h), the way os-posix.h, os-vxworks.h
 *   and os-rtems.h do for their ports. The gate is defined in os-impl-common.c;
 *   every other declaration names the module that defines it.
 *
 *   The gate
 *   --------
 *   OS_QueueGet(), OS_MutSemTake() and their kind look the object up with
 *   OS_LOCK_MODE_NONE, which does not raise the refcount that the object's
 *   delete waits on. Without help, the delete frees a kernel object that a
 *   task is still waiting on, or has just looked up. An object that keeps a
 *   gate next to its kernel handle fixes that:
 *
 *     - a user calls OS_FreeRTOS_GateEnter() before touching the kernel object,
 *       and OS_FreeRTOS_GateLeave() afterwards. Enter refuses once the gate is
 *       closed, so nobody new gets in;
 *     - the delete calls OS_FreeRTOS_GateClose(), then loops on
 *       OS_FreeRTOS_GateWake() (which also wakes users blocked in the kernel)
 *       until no user is left, and only then frees the kernel object;
 *     - a task deleted while it is in such a call never reaches Leave, so the
 *       task module drops its nodes with OS_FreeRTOS_GateReleaseTask() first.
 *
 *   The gate holds no kernel handle: each object keeps its own, typed, beside
 *   it. Reading that handle after a successful Enter is safe, because the
 *   delete cannot get past its wait while the caller is counted.
 *
 *   Task death
 *   ----------
 *   A primitive that keeps per-task state (a gate node on the task's stack, a
 *   mutex the task holds) must drop it before the task goes, and a task that
 *   deleted itself must be ended somewhere the kernel can be entered, i.e. at
 *   its next wait. The task module does not know the primitives, so the
 *   primitives reach it through os-impl-common.c:
 *
 *     - a primitive registers a hook with OS_FreeRTOS_OnTaskDied() from its
 *       init. The task module calls OS_FreeRTOS_TaskDied() once for a task
 *       about to be deleted, which drops the task's gate nodes and then runs
 *       the hooks in the order they were registered, which is the order
 *       OS_API_Impl_Init() is called in (osapi-idmap.h: mutexes before
 *       condition variables);
 *     - a task that deleted itself is ended at its next wait. OS_TaskDelete_Impl()
 *       only marks it, because that call has to return so OSAL can finish the
 *       delete. Every wait into the port calls OS_FreeRTOS_Enter() first, which
 *       the task module defines: OS_FreeRTOS_GateEnter() for a wait on an
 *       object, and OS_TaskDelay_Impl() for one on time. Enter ends the calling
 *       task if it is marked, and does nothing for any other.
 */

#ifndef OS_FREERTOS_H
#define OS_FREERTOS_H

#include "common_types.h"

#include "FreeRTOS.h"
#include "task.h"

#include "os-shared-idmap.h"

/*
 * closed is set once a delete has started. A zeroed gate is open.
 */
typedef struct
{
    // cppcheck-suppress unusedStructMember ; used in os-impl-common.c
    bool closed;
} OS_impl_gate_t;

/*
 * One per call in progress on a gated object, living on the stack of the task
 * that makes it. All of them are linked into one list, kept in os-impl-common.c,
 * and the users of a gate are the nodes that point at it. The list lets a
 * delete wake the tasks blocked in the kernel, and lets the nodes of a task
 * that is deleted mid-call, which would outlive its stack, be dropped.
 */
typedef struct OS_impl_gate_user
{
    TaskHandle_t              task;
    // cppcheck-suppress unusedStructMember ; the list is walked in os-impl-common.c
    const OS_impl_gate_t     *gate;
    // cppcheck-suppress unusedStructMember ; the list is walked in os-impl-common.c
    struct OS_impl_gate_user *next;
} OS_impl_gate_user_t;

/*
 * Lists the caller, through user, under the calling task and the gate, and
 * returns true; or returns false (listing nothing) if the gate is closed.
 */
bool OS_FreeRTOS_GateEnter(OS_impl_gate_t *gate, OS_impl_gate_user_t *user);

/*
 * Undoes Enter (pass the same user). Returns true if the gate was closed while
 * the caller was in, i.e. the object is being deleted.
 */
bool OS_FreeRTOS_GateLeave(const OS_impl_gate_t *gate, const OS_impl_gate_user_t *user);

/* Lets users in again; called when the object is created */
void OS_FreeRTOS_GateOpen(OS_impl_gate_t *gate);

/* Refuses new users from now on; the first step of a delete */
void OS_FreeRTOS_GateClose(OS_impl_gate_t *gate);

bool OS_FreeRTOS_GateIsClosed(const OS_impl_gate_t *gate);

/*
 * Wakes every task in a call on the gate that is blocked in the kernel, and
 * returns how many users the gate has. A delete calls this until it returns 0,
 * waiting a tick between calls: a user that has entered but not yet blocked is
 * woken by a later call. A binary semaphore's flush calls it once, since a
 * task that was not yet blocked when it ran began its wait after the flush.
 */
uint32 OS_FreeRTOS_GateWake(const OS_impl_gate_t *gate);

/*
 * Task death; see the file comment. A hook may block (it waits for the tasks
 * it wakes to leave), so none of this is for interrupt context.
 */
typedef void (*OS_FreeRTOS_TaskDiedHook_t)(TaskHandle_t task);

/*
 * How many hooks OS_FreeRTOS_OnTaskDied() takes: one per primitive that keeps
 * per-task state, and the idmap's one for the wait users and table locks a dead
 * task held (see "Wait users" below)
 */
#define OS_FREERTOS_TASK_DIED_HOOKS 3

/*
 * Registers a hook that OS_FreeRTOS_TaskDied() runs, after those registered
 * before it. Returns OS_SUCCESS, or OS_ERROR once OS_FREERTOS_TASK_DIED_HOOKS
 * are registered, which fails the init: raise the count for a new primitive.
 */
int32 OS_FreeRTOS_OnTaskDied(OS_FreeRTOS_TaskDiedHook_t hook);

/*
 * Called by the task module for a task about to be deleted, before it is.
 * The task must be suspended first (or be the caller), so that it cannot
 * enter a gate again. Drops every node of the task from every gate (a task
 * deleted mid-call never reaches Leave, and its nodes would otherwise stay on
 * the list, counted as users and pointing into a stack that is gone), then
 * runs the registered hooks in order.
 */
void OS_FreeRTOS_TaskDied(TaskHandle_t task);

/*
 * Defined in os-impl-tasks.c. Called as a task enters a wait into the port
 * (OS_FreeRTOS_GateEnter(), OS_TaskDelay_Impl()), before it blocks: ends the
 * calling task if it deleted itself with OS_TaskDelete(), which only marked it,
 * in which case it does not return. Returns at once for any other task.
 */
void OS_FreeRTOS_Enter(void);

/*
 * Wait users: the refcount based replacement for the gate, used by the queue
 * first (the other primitives still use the gate until this is proven).
 *
 * OS_QueueGet() and its kind look the object up with OS_LOCK_MODE_NONE, so a
 * delete does not know a task is blocked in the kernel on it. Instead of a gate
 * kept next to every object, the blocking call takes OSAL's own refcount on the
 * object for as long as it is inside, which OS_LOCK_MODE_EXCLUSIVE (the delete)
 * waits to reach zero:
 *
 *     - OS_FreeRTOS_WaitEnter() calls OS_FreeRTOS_Enter(), then takes the
 *       refcount (OS_ObjectIdGetById() with OS_LOCK_MODE_REFCOUNT) and lists
 *       the caller as a wait user, a node kept on the caller's stack;
 *     - OS_FreeRTOS_WaitLeave() unlists the node and gives the refcount back;
 *     - a delete finds the refcount above zero, and OSAL retries it a few times,
 *       calling OS_WaitForStateChange_Impl() between the tries. That function
 *       (os-impl-idmap.c) is where the users blocked on an object that is being
 *       deleted (its record reads OS_OBJECT_ID_RESERVED) are woken, so they
 *       leave, the refcount drops to zero and the delete goes ahead;
 *     - a task deleted while it is in such a call never reaches Leave, so the
 *       idmap's task death hook unlists its nodes and gives their refcounts back.
 *
 * The table locks OSAL takes around its own bookkeeping (OS_Lock_Global_Impl()
 * and its two companions) are defined in os-impl-idmap.c too: one lock per object
 * type, which a task death releases if the dead task held it.
 */
typedef struct OS_impl_wait_user
{
    // cppcheck-suppress unusedStructMember ; used in os-impl-idmap.c
    TaskHandle_t              task;
    // cppcheck-suppress unusedStructMember ; used in os-impl-idmap.c
    OS_object_token_t         token; /* the refcount lock this user holds, from OS_ObjectIdGetById() */
    // cppcheck-suppress unusedStructMember ; the list is walked in os-impl-idmap.c
    struct OS_impl_wait_user *next;
} OS_impl_wait_user_t;

/*
 * Calls OS_FreeRTOS_Enter(), then takes a refcount on the object that token
 * names, through a lookup by the token's type and id, and lists the caller under
 * the calling task. token is the one the Impl function was handed (the lookup that
 * took no lock); user keeps the new refcount token. Returns OS_SUCCESS, or what the
 * lookup returned (OS_ERR_INVALID_ID for an object that was deleted,
 * OS_ERR_OBJECT_IN_USE if it stayed reserved for OSAL's retries), in which case
 * nothing is listed and Leave must not be called.
 */
int32 OS_FreeRTOS_WaitEnter(const OS_object_token_t *token, OS_impl_wait_user_t *user);

/*
 * Undoes a successful Enter (pass the same user): unlists the node and gives the
 * refcount back. Returns true if the object was being deleted (its record read
 * OS_OBJECT_ID_RESERVED) just before the refcount was given back, i.e. a blocked
 * call that failed was woken by the delete and did not time out.
 */
bool OS_FreeRTOS_WaitLeave(OS_impl_wait_user_t *user);

/*
 * Defined in os-impl-idmap.c, called by OS_API_Impl_Init() for the undefined type
 * once it has emptied the task death hook list. Resets the table lock of every
 * object type (a held flag and an owner, not a semaphore, see os-impl-idmap.c) and
 * registers the task death hook. Returns OS_SUCCESS, or OS_ERROR if the hook could
 * not be registered.
 */
int32 OS_FreeRTOS_TableMutex_Init(void);

/*
 * The init hooks OS_API_Impl_Init() in os-impl-common.c calls, each defined by
 * the module that owns the state it sets up (os-posix.h lists its
 * OS_Posix_*API_Impl_Init the same way).
 */

/*
 * Defined in os-impl-tasks.c. Creates the semaphore that counts free TCB pool
 * entries. Returns OS_SUCCESS, or OS_ERROR if the semaphore could not be
 * created.
 */
int32 OS_FreeRTOS_TaskAPI_Impl_Init(void);

/*
 * Defined in os-impl-mutex.c. Registers the task death hook that orphans the
 * mutexes a dead task held. Returns what OS_FreeRTOS_OnTaskDied() does.
 */
int32 OS_FreeRTOS_MutexAPI_Impl_Init(void);

/*
 * Defined in os-impl-condvar.c. Registers the task death hook that drops a
 * dead task's wait nodes and orphans the condition variable mutexes it held.
 * Returns what OS_FreeRTOS_OnTaskDied() does.
 */
int32 OS_FreeRTOS_CondVarAPI_Impl_Init(void);

/* Defined in os-impl-files.c. Clears the file handle table. Returns OS_SUCCESS. */
int32 OS_FreeRTOS_StreamAPI_Impl_Init(void);

/*
 * Defined in os-impl-filesys.c. Creates the lock that serializes every call
 * into the one littlefs instance, then mounts the BSP's storage into it,
 * formatting the device first if it holds no file system. Returns OS_SUCCESS,
 * or OS_ERROR if the lock could not be created or the storage could not be
 * mounted.
 */
int32 OS_FreeRTOS_FileSysAPI_Impl_Init(void);

/*
 * Defined in os-impl-tasks.c. Converts a wait in milliseconds to ticks,
 * rounding up so the wait is never shorter than asked (pdMS_TO_TICKS()
 * truncates, and a sub-tick wait would become zero ticks, i.e. a yield or a
 * poll). Zero stays zero. Every OSAL call that waits goes through this, not
 * pdMS_TO_TICKS() directly.
 */
TickType_t OS_FreeRTOS_MsToTicks(uint32 millisecond);

#endif /* OS_FREERTOS_H */
