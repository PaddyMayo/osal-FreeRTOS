/*
 * File: os-freertos.h
 *
 * Purpose:
 *   Declarations shared by the modules of the FreeRTOS OSAL port that are not
 *   part of any OSAL contract (os-shared-*.h), the way os-posix.h, os-vxworks.h
 *   and os-rtems.h do for their ports. The definitions are in os-impl-common.c.
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
 *       until no user is left, and only then frees the kernel object.
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
 * One per task blocked in the kernel on a gated object, living on that task's
 * stack, so a delete can wake the task before it frees the object.
 */
typedef struct OS_impl_gate_waiter
{
    TaskHandle_t                task;
    // cppcheck-suppress unusedStructMember ; the list is walked in os-impl-common.c
    struct OS_impl_gate_waiter *next;
} OS_impl_gate_waiter_t;

/*
 * closed is set once a delete has started; users counts the callers between
 * Enter and Leave. Both, and the list, are only changed inside a critical
 * section. A zeroed gate is open, with nobody in it.
 */
typedef struct
{
    // cppcheck-suppress unusedStructMember ; used in os-impl-common.c
    bool                   closed;
    uint32                 users;
    // cppcheck-suppress unusedStructMember ; used in os-impl-common.c
    OS_impl_gate_waiter_t *waiters;
} OS_impl_gate_t;

/*
 * Counts the caller as a user and returns true, or returns false (counting
 * nothing) if the gate is closed. A caller that may block in the kernel passes
 * a waiter node, which is listed under the calling task until Leave.
 */
bool OS_FreeRTOS_GateEnter(OS_impl_gate_t *gate, OS_impl_gate_waiter_t *waiter);

/*
 * Undoes Enter (pass the same waiter, or NULL). Returns true if the gate was
 * closed while the caller was in, i.e. the object is being deleted.
 */
bool OS_FreeRTOS_GateLeave(OS_impl_gate_t *gate, const OS_impl_gate_waiter_t *waiter);

/* Lets users in again; called when the object is created */
void OS_FreeRTOS_GateOpen(OS_impl_gate_t *gate);

/* Refuses new users from now on; the first step of a delete */
void OS_FreeRTOS_GateClose(OS_impl_gate_t *gate);

bool OS_FreeRTOS_GateIsClosed(const OS_impl_gate_t *gate);

/*
 * Wakes every listed task blocked in the kernel, and returns how many users
 * are still in. A delete calls this until it returns 0, waiting a tick between
 * calls: a user that has entered but not yet blocked is woken by a later call.
 */
uint32 OS_FreeRTOS_GateWake(const OS_impl_gate_t *gate);

#endif /* OS_FREERTOS_H */
