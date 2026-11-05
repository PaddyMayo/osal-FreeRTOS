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
 */

#ifndef OS_FREERTOS_H
#define OS_FREERTOS_H

#include "common_types.h"

#include "FreeRTOS.h"
#include "task.h"

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
 * Drops every node of a task that is about to be deleted, from every gate. A
 * task deleted mid-call never reaches Leave, and its nodes would otherwise
 * stay on the list, counted as users and pointing into a stack that is gone.
 * The task must be suspended first, so that it cannot enter a gate again.
 */
void OS_FreeRTOS_GateReleaseTask(TaskHandle_t task);

/*
 * The init hooks OS_API_Impl_Init() in os-impl-common.c calls, each defined by
 * the module that owns the state it sets up (os-posix.h lists its
 * OS_Posix_*API_Impl_Init the same way).
 */

/* Defined in os-impl-tasks.c. Creates the semaphore that counts free TCB pool entries. */
int32 OS_FreeRTOS_TaskInit(void);

/* Defined in os-impl-files.c. Clears the file handle table. Returns OS_SUCCESS. */
int32 OS_FreeRTOS_FileInit(void);

/*
 * Defined in os-impl-filesys.c. Creates the lock that serializes every call
 * into the one littlefs instance, then mounts the BSP's storage into it,
 * formatting the device first if it holds no file system. Returns OS_SUCCESS,
 * or OS_ERROR if the lock could not be created or the storage could not be
 * mounted.
 */
int32 OS_FreeRTOS_FileSysInit(void);

/*
 * Defined in os-impl-tasks.c. Converts a wait in milliseconds to ticks,
 * rounding up so the wait is never shorter than asked (pdMS_TO_TICKS()
 * truncates, and a sub-tick wait would become zero ticks, i.e. a yield or a
 * poll). Zero stays zero. Every OSAL call that waits goes through this, not
 * pdMS_TO_TICKS() directly.
 */
TickType_t OS_FreeRTOS_MsToTicks(uint32 millisecond);

/*
 * Defined in os-impl-mutex.c. Called for a task about to be deleted, before it
 * is, for the mutexes it holds: it cannot give them back (only the holder can),
 * so each is orphaned. The gate is closed, the Takes blocked on it are woken
 * with an error, and the call returns once none is left in, so that nothing
 * touches the holder when it goes. OS_MutSemDelete() then frees an orphaned
 * mutex, which it otherwise refuses while a task holds it. Blocks, so it is not
 * for interrupt context.
 */
void OS_FreeRTOS_MutexOwnerDied(TaskHandle_t task);

#endif /* OS_FREERTOS_H */
