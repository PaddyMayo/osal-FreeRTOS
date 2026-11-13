/*
 * File: coveragetest-condvar.c
 *
 * Purpose:
 *   White-box coverage test cases for freertos/src/os-impl-condvar.c, run
 *   against the fake FreeRTOS mutex and task notification API in ../fake-inc
 *   and stubbed in ../stubs/freertos-condvar-stubs.c - no real FreeRTOS kernel
 *   involved. (The black box tests for this port are OSAL's own /src/tests and
 *   /src/unit-tests, which link the real OSAL library instead.)
 *
 *   Each _Impl function is called directly; preconditions that its public
 *   API cannot reach are set up through ../adaptors/inc/ut-adaptor-condvar.h,
 *   and effects are observed via return codes plus the fake FreeRTOS stub
 *   layer's captured call arguments and counts.
 *
 *   A task blocked in a wait cannot be run here, so what other tasks do while
 *   it waits is done from inside the stubbed blocking call, through a hook: a
 *   signal made by calling OS_CondVarSignal_Impl() there goes through the real
 *   wait list, with the waiter's real node on it, and ends the wait the way it
 *   would on the target.
 *
 *   The gate (os-freertos.h) is covered with os-impl-common.c; here it is a
 *   stand-in, so these tests check how the condition variable uses it: every
 *   call passes through it, Delete closes it and waits on it.
 */

#include <string.h>

#include "utassert.h"
#include "uttest.h"
#include "utstubs.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "os-shared-condvar.h"
#include "os-shared-idmap.h"

#include "ut-adaptor-condvar.h"

/* Captured stub call arguments and scripted values, declared in freertos-condvar-stubs.c */
extern SemaphoreHandle_t          UT_Stub_CreatedCondVarMutex;
extern SemaphoreHandle_t          UT_Stub_LastDeletedMutex;
extern SemaphoreHandle_t          UT_Stub_LastTakeMutex;
extern TickType_t                 UT_Stub_LastTakeMutexTicks;
extern SemaphoreHandle_t          UT_Stub_LastGiveMutex;
extern SemaphoreHandle_t          UT_Stub_LastHolderQueried;
extern void                       UT_Stub_SetMutexHolder(SemaphoreHandle_t handle, TaskHandle_t holder);
extern void                       UT_Stub_ResetMutexHolders(void);
extern void                       UT_Stub_SetTaskPriority(TaskHandle_t task, UBaseType_t priority);
extern void                       UT_Stub_ResetTaskPriorities(void);
extern UBaseType_t                UT_Stub_LastNotifyTakeIndex;
extern BaseType_t                 UT_Stub_LastNotifyTakeClear;
extern TickType_t                 UT_Stub_LastNotifyTakeTicks;
extern TaskHandle_t               UT_Stub_NotifiedTasks[];
extern uint32                     UT_Stub_NotifiedCount;
extern UBaseType_t                UT_Stub_LastNotifyIndex;
extern void                       UT_Stub_ResetNotifyLog(void);
extern uint32                     UT_Stub_LastMsToTicksInput;
extern TickType_t                 UT_Stub_MsToTicksResult;
extern OS_time_t                  UT_Stub_LocalTime;
extern TickType_t                 UT_Stub_CondVarLastDelayTicks;
extern const OS_impl_gate_t      *UT_Stub_LastGate;
extern OS_FreeRTOS_TaskDiedHook_t UT_Stub_TaskDiedHook;

#define UT_CONDVAR_TOKEN(idx)                                                                                   \
    (OS_object_token_t)                                                                                         \
    {                                                                                                           \
        .obj_type = OS_OBJECT_TYPE_OS_CONDVAR,                                                                  \
        .obj_id = (osal_id_t) { (OS_OBJECT_TYPE_OS_CONDVAR << OS_OBJECT_TYPE_SHIFT) + (idx) }, .obj_idx = (idx) \
    }

#define UT_INDEX_0 OSAL_INDEX_C(0)
#define UT_INDEX_1 OSAL_INDEX_C(1)

#define UT_MUTEX       ((SemaphoreHandle_t)0x4545)
#define UT_OTHER_MUTEX ((SemaphoreHandle_t)0x4546)

/* The task that is running (the stub's default), and others */
#define UT_SELF   ((TaskHandle_t)0x7001)
#define UT_TASK_A ((TaskHandle_t)0x7101)
#define UT_TASK_B ((TaskHandle_t)0x7102)
#define UT_TASK_C ((TaskHandle_t)0x7103)
#define UT_TASK_D ((TaskHandle_t)0x7104)

/* Wait nodes of tasks that are not run here, owned by the test */
static OS_impl_condvar_waiter_t UT_Waiters[4];

static void UT_Queue(osal_index_t local_id, uint32 n, TaskHandle_t task)
{
    UT_Waiters[n].task     = task;
    UT_Waiters[n].next     = NULL;
    UT_Waiters[n].signaled = false;

    UT_CondVarTest_AddWaiter(local_id, &UT_Waiters[n]);
}

/* Inside OS_FreeRTOS_GateWake(): the users left, as a delete sees them while the calls already in finish */
static int32 UT_WakeCalls;

static int32
UT_WakeReportsUsersLeaving(void *UserObj, int32 StubRetcode, uint32 CallCount, const UT_StubContext_t *Context)
{
    (void)UserObj;
    (void)StubRetcode;
    (void)CallCount;
    (void)Context;

    return 2 - UT_WakeCalls++;
}

/* Inside vSemaphoreDelete(): how many times the delete had checked for users by then */
static uint32 UT_WakeCallsAtFree;

static int32 UT_FreeSeesWakes(void *UserObj, int32 StubRetcode, uint32 CallCount, const UT_StubContext_t *Context)
{
    (void)UserObj;
    (void)CallCount;
    (void)Context;

    UT_WakeCallsAtFree = UT_GetStubCount(UT_KEY(OS_FreeRTOS_GateWake));

    return StubRetcode;
}

/*
 * Inside the blocking call of a wait, once each time it is armed: the state the
 * wait is in by then, and what the other tasks do meanwhile - a signal, if
 * UT_HookSignals, and then the wait ends with UT_BlockResult as its notification
 * count (0 is a wait that gave up). The call that drains a notification after
 * the wait is not the blocking call, so it passes through.
 */
static OS_object_token_t UT_HookToken;
static bool              UT_HookArmed;
static bool              UT_HookSignals;
static int32             UT_BlockResult;
static uint32            UT_GivesAtBlock;
static uint32            UT_TakesAtBlock;
static bool              UT_ListedAtBlock;

static int32 UT_BlockSeesState(void *UserObj, int32 StubRetcode, uint32 CallCount, const UT_StubContext_t *Context)
{
    (void)UserObj;
    (void)CallCount;
    (void)Context;

    if (!UT_HookArmed)
    {
        return StubRetcode;
    }

    UT_HookArmed = false;

    UT_GivesAtBlock  = UT_GetStubCount(UT_KEY(xSemaphoreGive));
    UT_TakesAtBlock  = UT_GetStubCount(UT_KEY(xSemaphoreTake));
    UT_ListedAtBlock = (UT_CondVarTest_GetWaiters(UT_INDEX_0) != NULL);

    if (UT_HookSignals)
    {
        (void)OS_CondVarSignal_Impl(&UT_HookToken);
    }

    return UT_BlockResult;
}

/*
 * Inside xSemaphoreGive(), the first time: whether the task that is giving the mutex up is on the wait list by then.
 * A signal made the moment the mutex is free must find it there, or it is lost.
 */
static bool UT_ListedAtGive;

static int32 UT_GiveSeesList(void *UserObj, int32 StubRetcode, uint32 CallCount, const UT_StubContext_t *Context)
{
    (void)UserObj;
    (void)Context;

    if (CallCount == 0)
    {
        UT_ListedAtGive = (UT_CondVarTest_GetWaiters(UT_INDEX_0) != NULL);
    }

    return StubRetcode;
}

/* Arms the hook for the next wait: what the other tasks do while it blocks, and how it ends */
static void UT_ArmBlock(bool signals, int32 block_result)
{
    UT_HookToken   = UT_CONDVAR_TOKEN(0);
    UT_HookArmed   = true;
    UT_HookSignals = signals;
    UT_BlockResult = block_result;
}

/* Sets up a wait: the caller holds the mutex, and the blocking call is hooked */
static void UT_SetUpWait(bool signals, int32 block_result)
{
    UT_ArmBlock(signals, block_result);

    UT_CondVarTest_SetImplMutex(UT_INDEX_0, UT_MUTEX);
    UT_Stub_SetMutexHolder(UT_MUTEX, UT_SELF);
    UT_SetHookFunction(UT_KEY(ulTaskNotifyTakeIndexed), UT_BlockSeesState, NULL);
    UT_SetHookFunction(UT_KEY(xSemaphoreGive), UT_GiveSeesList, NULL);
    UT_ListedAtGive = false;
}

/*
**********************************************************************************
**          TEST CASES
**********************************************************************************
*/

void Test_OS_CondVarCreate_Impl(void)
{
    OS_object_token_t token = UT_CONDVAR_TOKEN(0);

    /* success -> mutex stored in the impl table, an empty wait list (whatever the slot held), gate opened */
    UT_Queue(UT_INDEX_0, 0, UT_TASK_A);
    UtAssert_INT32_EQ(OS_CondVarCreate_Impl(&token, 0), OS_SUCCESS);
    UtAssert_STUB_COUNT(xSemaphoreCreateMutex, 1);
    UtAssert_True(UT_CondVarTest_GetImplMutex(UT_INDEX_0) == UT_Stub_CreatedCondVarMutex,
                  "xSemaphoreCreateMutex() handle stored in impl table");
    UtAssert_NULL(UT_CondVarTest_GetWaiters(UT_INDEX_0));
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateOpen, 1);
    UtAssert_True(UT_Stub_LastGate == UT_CondVarTest_GetImplGate(UT_INDEX_0), "this condition variable's gate opened");

    /* xSemaphoreCreateMutex() failure (e.g. out of heap) -> OS_ERROR, no handle kept, nothing opened */
    UT_SetDeferredRetcode(UT_KEY(xSemaphoreCreateMutex), 1, -1);
    UtAssert_INT32_EQ(OS_CondVarCreate_Impl(&token, 0), OS_ERROR);
    UtAssert_STUB_COUNT(xSemaphoreCreateMutex, 2);
    UtAssert_NULL(UT_CondVarTest_GetImplMutex(UT_INDEX_0));
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateOpen, 1);
}

void Test_OS_CondVarDelete_Impl(void)
{
    OS_object_token_t token = UT_CONDVAR_TOKEN(0);

    UT_CondVarTest_SetImplMutex(UT_INDEX_0, UT_MUTEX);

    /* not locked, nobody in the gate -> gate closed, no waiting, mutex freed and the slot cleared */
    UtAssert_INT32_EQ(OS_CondVarDelete_Impl(&token), OS_SUCCESS);
    UtAssert_STUB_COUNT(xSemaphoreGetMutexHolder, 1);
    UtAssert_True(UT_Stub_LastHolderQueried == UT_MUTEX, "holder asked of the mutex being deleted");
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateClose, 1);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateWake, 1);
    UtAssert_STUB_COUNT(vTaskDelay, 0);
    UtAssert_STUB_COUNT(vSemaphoreDelete, 1);
    UtAssert_True(UT_Stub_LastDeletedMutex == UT_MUTEX, "vSemaphoreDelete() called with mutex handle");
    UtAssert_NULL(UT_CondVarTest_GetImplMutex(UT_INDEX_0));
    UtAssert_True(UT_Stub_LastGate == UT_CondVarTest_GetImplGate(UT_INDEX_0), "this condition variable's gate used");
}

void Test_OS_CondVarDelete_Impl_Drain(void)
{
    OS_object_token_t token = UT_CONDVAR_TOKEN(0);

    UT_CondVarTest_SetImplMutex(UT_INDEX_0, UT_MUTEX);

    /* two calls already in the gate, which finish one after the other while the delete waits a tick for each */
    UT_WakeCalls = 0;
    UT_SetHookFunction(UT_KEY(OS_FreeRTOS_GateWake), UT_WakeReportsUsersLeaving, NULL);
    UT_SetHookFunction(UT_KEY(vSemaphoreDelete), UT_FreeSeesWakes, NULL);

    UtAssert_INT32_EQ(OS_CondVarDelete_Impl(&token), OS_SUCCESS);

    UtAssert_STUB_COUNT(OS_FreeRTOS_GateWake, 3);
    UtAssert_STUB_COUNT(vTaskDelay, 2);
    UtAssert_UINT32_EQ(UT_Stub_CondVarLastDelayTicks, 1);

    /* the mutex is freed only once the last user has left */
    UtAssert_STUB_COUNT(vSemaphoreDelete, 1);
    UtAssert_UINT32_EQ(UT_WakeCallsAtFree, 3);
}

void Test_OS_CondVarDelete_Impl_Locked(void)
{
    OS_object_token_t token = UT_CONDVAR_TOKEN(0);

    UT_CondVarTest_SetImplMutex(UT_INDEX_0, UT_MUTEX);

    /* a task holds it -> refused, and nothing is touched: the gate stays open, the mutex is not freed */
    UT_Stub_SetMutexHolder(UT_MUTEX, UT_TASK_A);
    UtAssert_INT32_EQ(OS_CondVarDelete_Impl(&token), OS_ERROR);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateClose, 0);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateWake, 0);
    UtAssert_STUB_COUNT(vSemaphoreDelete, 0);
    UtAssert_True(UT_CondVarTest_GetImplMutex(UT_INDEX_0) == UT_MUTEX, "mutex handle kept");
}

void Test_OS_CondVarDelete_Impl_Orphaned(void)
{
    OS_object_token_t token = UT_CONDVAR_TOKEN(0);

    UT_CondVarTest_SetImplMutex(UT_INDEX_0, UT_MUTEX);

    /* held by a task that is gone, and its gate was closed when it went -> freed like a free mutex */
    UT_Stub_SetMutexHolder(UT_MUTEX, UT_TASK_A);
    UT_SetDefaultReturnValue(UT_KEY(OS_FreeRTOS_GateIsClosed), 1);

    UtAssert_INT32_EQ(OS_CondVarDelete_Impl(&token), OS_SUCCESS);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateClose, 1);
    UtAssert_STUB_COUNT(vSemaphoreDelete, 1);
    UtAssert_True(UT_Stub_LastDeletedMutex == UT_MUTEX, "vSemaphoreDelete() called with mutex handle");
    UtAssert_NULL(UT_CondVarTest_GetImplMutex(UT_INDEX_0));
}

void Test_OS_CondVarLock_Impl(void)
{
    OS_object_token_t token = UT_CONDVAR_TOKEN(0);

    UT_CondVarTest_SetImplMutex(UT_INDEX_0, UT_MUTEX);

    /* success -> through the gate, waits forever for the mutex, nothing handed back */
    UtAssert_INT32_EQ(OS_CondVarLock_Impl(&token), OS_SUCCESS);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateEnter, 1);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 1);
    UtAssert_True(UT_Stub_LastGate == UT_CondVarTest_GetImplGate(UT_INDEX_0), "this condition variable's gate used");
    UtAssert_STUB_COUNT(xSemaphoreTake, 1);
    UtAssert_True(UT_Stub_LastTakeMutex == UT_MUTEX, "xSemaphoreTake() called with mutex handle");
    UtAssert_True(UT_Stub_LastTakeMutexTicks == portMAX_DELAY, "xSemaphoreTake() waits forever");
    UtAssert_STUB_COUNT(xSemaphoreGive, 0);

    /* the caller holds it already -> OS_ERROR at once, not a wait for itself; the gate is still left */
    UT_Stub_SetMutexHolder(UT_MUTEX, UT_SELF);
    UtAssert_INT32_EQ(OS_CondVarLock_Impl(&token), OS_ERROR);
    UtAssert_STUB_COUNT(xSemaphoreTake, 1);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 2);
    UT_Stub_ResetMutexHolders();

    /* xSemaphoreTake() failure with the gate open -> OS_ERROR */
    UT_SetDeferredRetcode(UT_KEY(xSemaphoreTake), 1, -1);
    UtAssert_INT32_EQ(OS_CondVarLock_Impl(&token), OS_ERROR);
    UtAssert_STUB_COUNT(xSemaphoreGive, 0);

    /* the gate is closed (the delete woke this Lock) -> OS_ERR_INVALID_ID, nothing to hand back */
    UT_SetDefaultReturnValue(UT_KEY(OS_FreeRTOS_GateIsClosed), 1);
    UT_SetDeferredRetcode(UT_KEY(xSemaphoreTake), 1, -1);
    UtAssert_INT32_EQ(OS_CondVarLock_Impl(&token), OS_ERR_INVALID_ID);
    UtAssert_STUB_COUNT(xSemaphoreGive, 0);

    /* the gate is closed, but the Lock got the mutex before: handed back while the delete waits, so it can free it */
    UtAssert_INT32_EQ(OS_CondVarLock_Impl(&token), OS_ERR_INVALID_ID);
    UtAssert_STUB_COUNT(xSemaphoreGive, 1);
    UtAssert_True(UT_Stub_LastGiveMutex == UT_MUTEX, "the mutex that was taken is the one handed back");
    UT_ClearDefaultReturnValue(UT_KEY(OS_FreeRTOS_GateIsClosed));

    /* the gate is refused at the door -> OS_ERR_INVALID_ID, the mutex untouched */
    UT_SetDeferredRetcode(UT_KEY(OS_FreeRTOS_GateEnter), 1, -1);
    UtAssert_INT32_EQ(OS_CondVarLock_Impl(&token), OS_ERR_INVALID_ID);
    UtAssert_STUB_COUNT(xSemaphoreTake, 4);
}

void Test_OS_CondVarUnlock_Impl(void)
{
    OS_object_token_t token = UT_CONDVAR_TOKEN(0);

    UT_CondVarTest_SetImplMutex(UT_INDEX_0, UT_MUTEX);

    /* the caller holds it -> given back, through the gate */
    UT_Stub_SetMutexHolder(UT_MUTEX, UT_SELF);
    UtAssert_INT32_EQ(OS_CondVarUnlock_Impl(&token), OS_SUCCESS);
    UtAssert_STUB_COUNT(xSemaphoreGive, 1);
    UtAssert_True(UT_Stub_LastGiveMutex == UT_MUTEX, "xSemaphoreGive() called with mutex handle");
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateEnter, 1);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 1);
    UtAssert_True(UT_Stub_LastGate == UT_CondVarTest_GetImplGate(UT_INDEX_0), "this condition variable's gate used");

    /* another task holds it -> OS_ERROR, and FreeRTOS is not asked to give what the caller does not hold */
    UT_Stub_SetMutexHolder(UT_MUTEX, UT_TASK_A);
    UtAssert_INT32_EQ(OS_CondVarUnlock_Impl(&token), OS_ERROR);
    UtAssert_STUB_COUNT(xSemaphoreGive, 1);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 2);

    /* nobody holds it -> OS_ERROR as well */
    UT_Stub_ResetMutexHolders();
    UtAssert_INT32_EQ(OS_CondVarUnlock_Impl(&token), OS_ERROR);
    UtAssert_STUB_COUNT(xSemaphoreGive, 1);

    /* the gate is closed (the condition variable is being deleted) -> refused before FreeRTOS is touched */
    UT_SetDeferredRetcode(UT_KEY(OS_FreeRTOS_GateEnter), 1, -1);
    UtAssert_INT32_EQ(OS_CondVarUnlock_Impl(&token), OS_ERR_INVALID_ID);
    UtAssert_STUB_COUNT(xSemaphoreGetMutexHolder, 3);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 3);
}

void Test_OS_CondVarSignal_Impl(void)
{
    OS_object_token_t token = UT_CONDVAR_TOKEN(0);

    /* nobody waits -> success, and nothing is notified or kept for a wait that comes later */
    UtAssert_INT32_EQ(OS_CondVarSignal_Impl(&token), OS_SUCCESS);
    UtAssert_UINT32_EQ(UT_Stub_NotifiedCount, 0);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateEnter, 1);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 1);
    UtAssert_True(UT_Stub_LastGate == UT_CondVarTest_GetImplGate(UT_INDEX_0), "this condition variable's gate used");

    /* one task waits -> it is taken off the list, marked signaled, and notified on the port's index, in one section */
    UT_Queue(UT_INDEX_0, 0, UT_TASK_A);
    UtAssert_INT32_EQ(OS_CondVarSignal_Impl(&token), OS_SUCCESS);
    UtAssert_NULL(UT_CondVarTest_GetWaiters(UT_INDEX_0));
    UtAssert_True(UT_Waiters[0].signaled, "the waiter is marked signaled");
    UtAssert_UINT32_EQ(UT_Stub_NotifiedCount, 1);
    UtAssert_True(UT_Stub_NotifiedTasks[0] == UT_TASK_A, "the waiting task is the one notified");
    UtAssert_UINT32_EQ(UT_Stub_LastNotifyIndex, OS_FREERTOS_CONDVAR_NOTIFY_INDEX);
    UtAssert_STUB_COUNT(vTaskEnterCritical, 2);
    UtAssert_STUB_COUNT(vTaskExitCritical, 2);

    /* the gate is closed (the condition variable is being deleted) -> refused, nobody notified */
    UT_Queue(UT_INDEX_0, 1, UT_TASK_B);
    UT_SetDeferredRetcode(UT_KEY(OS_FreeRTOS_GateEnter), 1, -1);
    UtAssert_INT32_EQ(OS_CondVarSignal_Impl(&token), OS_ERR_INVALID_ID);
    UtAssert_UINT32_EQ(UT_Stub_NotifiedCount, 1);
    UtAssert_True(UT_CondVarTest_GetWaiters(UT_INDEX_0) == &UT_Waiters[1], "the waiter is left on the list");
}

void Test_OS_CondVarSignal_Impl_Priority(void)
{
    OS_object_token_t token = UT_CONDVAR_TOKEN(0);

    /* A is the longest waiting, B and C wait at the highest priority, D is in between */
    UT_Stub_SetTaskPriority(UT_TASK_A, 1);
    UT_Stub_SetTaskPriority(UT_TASK_B, 3);
    UT_Stub_SetTaskPriority(UT_TASK_C, 3);
    UT_Stub_SetTaskPriority(UT_TASK_D, 2);
    UT_Queue(UT_INDEX_0, 0, UT_TASK_A);
    UT_Queue(UT_INDEX_0, 1, UT_TASK_B);
    UT_Queue(UT_INDEX_0, 2, UT_TASK_C);
    UT_Queue(UT_INDEX_0, 3, UT_TASK_D);

    /* each signal wakes one: the highest priority first, the longest waiting of equals, then down the priorities */
    UtAssert_INT32_EQ(OS_CondVarSignal_Impl(&token), OS_SUCCESS);
    UtAssert_INT32_EQ(OS_CondVarSignal_Impl(&token), OS_SUCCESS);
    UtAssert_INT32_EQ(OS_CondVarSignal_Impl(&token), OS_SUCCESS);
    UtAssert_INT32_EQ(OS_CondVarSignal_Impl(&token), OS_SUCCESS);
    UtAssert_UINT32_EQ(UT_Stub_NotifiedCount, 4);
    UtAssert_True(UT_Stub_NotifiedTasks[0] == UT_TASK_B, "highest priority first");
    UtAssert_True(UT_Stub_NotifiedTasks[1] == UT_TASK_C, "then the one of equal priority that waited after it");
    UtAssert_True(UT_Stub_NotifiedTasks[2] == UT_TASK_D, "then the next priority down");
    UtAssert_True(UT_Stub_NotifiedTasks[3] == UT_TASK_A, "lowest last");
    UtAssert_NULL(UT_CondVarTest_GetWaiters(UT_INDEX_0));
}

void Test_OS_CondVarBroadcast_Impl(void)
{
    OS_object_token_t token = UT_CONDVAR_TOKEN(0);
    uint32            i;

    /* nobody waits -> success, nothing notified */
    UtAssert_INT32_EQ(OS_CondVarBroadcast_Impl(&token), OS_SUCCESS);
    UtAssert_UINT32_EQ(UT_Stub_NotifiedCount, 0);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateEnter, 1);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 1);
    UtAssert_True(UT_Stub_LastGate == UT_CondVarTest_GetImplGate(UT_INDEX_0), "this condition variable's gate used");

    /* three wait -> every one is taken off the list, signaled and notified, in one critical section */
    UT_Queue(UT_INDEX_0, 0, UT_TASK_A);
    UT_Queue(UT_INDEX_0, 1, UT_TASK_B);
    UT_Queue(UT_INDEX_0, 2, UT_TASK_C);
    UtAssert_INT32_EQ(OS_CondVarBroadcast_Impl(&token), OS_SUCCESS);
    UtAssert_NULL(UT_CondVarTest_GetWaiters(UT_INDEX_0));
    UtAssert_UINT32_EQ(UT_Stub_NotifiedCount, 3);
    UtAssert_True(UT_Stub_NotifiedTasks[0] == UT_TASK_A, "the first to wait is notified first");
    UtAssert_True(UT_Stub_NotifiedTasks[1] == UT_TASK_B, "then the next");
    UtAssert_True(UT_Stub_NotifiedTasks[2] == UT_TASK_C, "and the last");
    for (i = 0; i < 3; ++i)
    {
        UtAssert_True(UT_Waiters[i].signaled, "every waiter is marked signaled");
    }
    UtAssert_UINT32_EQ(UT_Stub_LastNotifyIndex, OS_FREERTOS_CONDVAR_NOTIFY_INDEX);
    UtAssert_STUB_COUNT(vTaskEnterCritical, 2);
    UtAssert_STUB_COUNT(vTaskExitCritical, 2);

    /* the gate is closed (the condition variable is being deleted) -> refused, nobody notified */
    UT_Queue(UT_INDEX_0, 3, UT_TASK_D);
    UT_SetDeferredRetcode(UT_KEY(OS_FreeRTOS_GateEnter), 1, -1);
    UtAssert_INT32_EQ(OS_CondVarBroadcast_Impl(&token), OS_ERR_INVALID_ID);
    UtAssert_UINT32_EQ(UT_Stub_NotifiedCount, 3);
}

void Test_OS_CondVarWait_Impl(void)
{
    OS_object_token_t token = UT_CONDVAR_TOKEN(0);

    /* signaled while blocked: the wait ends in success, the signal having gone through the real wait list */
    UT_SetUpWait(true, 1);
    UtAssert_INT32_EQ(OS_CondVarWait_Impl(&token), OS_SUCCESS);

    /* it was on the list, with the mutex given up and not yet taken back, when it blocked; blocks forever on its index
     */
    UtAssert_True(UT_ListedAtBlock, "the task is on the wait list before it blocks");
    UtAssert_True(UT_ListedAtGive, "and was on it already when the mutex was given up, so no signal can miss it");
    UtAssert_UINT32_EQ(UT_GivesAtBlock, 1);
    UtAssert_UINT32_EQ(UT_TakesAtBlock, 0);
    UtAssert_UINT32_EQ(UT_Stub_LastNotifyTakeIndex, OS_FREERTOS_CONDVAR_NOTIFY_INDEX);
    UtAssert_True(UT_Stub_LastNotifyTakeClear == pdTRUE, "the notification count is cleared on the way out");
    UtAssert_True(UT_Stub_LastNotifyTakeTicks == portMAX_DELAY, "a Wait waits for ever");
    UtAssert_True(UT_Stub_LastGiveMutex == UT_MUTEX, "the mutex given up is the condition variable's");

    /* the signal notified this task, and it took the mutex back */
    UtAssert_UINT32_EQ(UT_Stub_NotifiedCount, 1);
    UtAssert_True(UT_Stub_NotifiedTasks[0] == UT_SELF, "the waiting task was the one notified");
    UtAssert_STUB_COUNT(xSemaphoreTake, 1);
    UtAssert_True(UT_Stub_LastTakeMutex == UT_MUTEX, "the mutex taken back is the condition variable's");
    UtAssert_True(UT_Stub_LastTakeMutexTicks == portMAX_DELAY, "and waited for as long as it takes");
    UtAssert_NULL(UT_CondVarTest_GetWaiters(UT_INDEX_0));

    /* nothing left over: the one wake was taken by the wait, so no drain, and the gate was left (the signal's too) */
    UtAssert_STUB_COUNT(ulTaskNotifyTakeIndexed, 1);
    UtAssert_STUB_COUNT(xSemaphoreGive, 1);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateEnter, 2);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 2);
}

void Test_OS_CondVarWait_Impl_NotHolder(void)
{
    OS_object_token_t token = UT_CONDVAR_TOKEN(0);

    UT_CondVarTest_SetImplMutex(UT_INDEX_0, UT_MUTEX);

    /* the caller does not hold the mutex -> OS_ERROR, before anything is changed: no node, no give, no wait */
    UtAssert_INT32_EQ(OS_CondVarWait_Impl(&token), OS_ERROR);
    UtAssert_NULL(UT_CondVarTest_GetWaiters(UT_INDEX_0));
    UtAssert_STUB_COUNT(xSemaphoreGive, 0);
    UtAssert_STUB_COUNT(ulTaskNotifyTakeIndexed, 0);
    UtAssert_STUB_COUNT(xSemaphoreTake, 0);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 1);

    /* ... and the same when another task holds it */
    UT_Stub_SetMutexHolder(UT_MUTEX, UT_TASK_A);
    UtAssert_INT32_EQ(OS_CondVarWait_Impl(&token), OS_ERROR);
    UtAssert_STUB_COUNT(xSemaphoreGive, 0);

    /* the gate is refused at the door -> OS_ERR_INVALID_ID, with the mutex not even looked at */
    UT_SetDeferredRetcode(UT_KEY(OS_FreeRTOS_GateEnter), 1, -1);
    UtAssert_INT32_EQ(OS_CondVarWait_Impl(&token), OS_ERR_INVALID_ID);
    UtAssert_STUB_COUNT(xSemaphoreGetMutexHolder, 2);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 2);
}

void Test_OS_CondVarWait_Impl_NoSignal(void)
{
    OS_object_token_t token = UT_CONDVAR_TOKEN(0);

    /* woken with nobody having signaled -> a spurious wake, which a condition variable may have: a success */
    UT_SetUpWait(false, 0);
    UtAssert_INT32_EQ(OS_CondVarWait_Impl(&token), OS_SUCCESS);

    /* the node came off the list by itself, and the mutex is held again */
    UtAssert_NULL(UT_CondVarTest_GetWaiters(UT_INDEX_0));
    UtAssert_STUB_COUNT(xSemaphoreTake, 1);
    UtAssert_STUB_COUNT(ulTaskNotifyTakeIndexed, 1);

    /* so a signal made now has no one to wake, and none is kept for the next wait */
    UtAssert_INT32_EQ(OS_CondVarSignal_Impl(&token), OS_SUCCESS);
    UtAssert_UINT32_EQ(UT_Stub_NotifiedCount, 0);
}

void Test_OS_CondVarWait_Impl_SignalAfterGivingUp(void)
{
    OS_object_token_t token = UT_CONDVAR_TOKEN(0);

    /*
     * The wait gave up (count 0), and a signal came just after, before the task
     * took itself off the list: the signal was meant for this task, so it counts,
     * and its notification, still pending, is taken here so that it cannot cut
     * the next wait short.
     */
    UT_SetUpWait(true, 0);
    UtAssert_INT32_EQ(OS_CondVarWait_Impl(&token), OS_SUCCESS);

    UtAssert_STUB_COUNT(ulTaskNotifyTakeIndexed, 2);
    UtAssert_UINT32_EQ(UT_Stub_LastNotifyTakeIndex, OS_FREERTOS_CONDVAR_NOTIFY_INDEX);
    UtAssert_True(UT_Stub_LastNotifyTakeClear == pdTRUE, "the drain clears the count");
    UtAssert_True(UT_Stub_LastNotifyTakeTicks == 0, "and does not wait: the notification is there already");
    UtAssert_NULL(UT_CondVarTest_GetWaiters(UT_INDEX_0));
    UtAssert_STUB_COUNT(xSemaphoreTake, 1);
}

void Test_OS_CondVarWait_Impl_Deleted(void)
{
    OS_object_token_t token = UT_CONDVAR_TOKEN(0);

    /* the delete closed the gate and woke this wait; the mutex was free, so it was taken: handed back, for the delete
     */
    UT_SetUpWait(false, 0);
    UT_SetDefaultReturnValue(UT_KEY(OS_FreeRTOS_GateIsClosed), 1);
    UtAssert_INT32_EQ(OS_CondVarWait_Impl(&token), OS_ERR_INVALID_ID);
    UtAssert_STUB_COUNT(xSemaphoreTake, 1);
    UtAssert_STUB_COUNT(xSemaphoreGive, 2);
    UtAssert_NULL(UT_CondVarTest_GetWaiters(UT_INDEX_0));
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 1);
}

void Test_OS_CondVarWait_Impl_RelockWoken(void)
{
    OS_object_token_t token = UT_CONDVAR_TOKEN(0);

    /* the wait for the mutex was cut short with the gate still open -> asked again, and then it is held */
    UT_SetUpWait(true, 1);
    UT_SetDeferredRetcode(UT_KEY(xSemaphoreTake), 1, -1);
    UtAssert_INT32_EQ(OS_CondVarWait_Impl(&token), OS_SUCCESS);
    UtAssert_STUB_COUNT(xSemaphoreTake, 2);
    UtAssert_STUB_COUNT(xSemaphoreGive, 1);
}

void Test_OS_CondVarWait_Impl_DeletedWhileRelocking(void)
{
    OS_object_token_t token = UT_CONDVAR_TOKEN(0);

    /* signaled, but the delete woke the wait for the mutex, which another task holds -> no mutex to hand back */
    UT_SetUpWait(true, 1);
    UT_SetDefaultReturnValue(UT_KEY(xSemaphoreTake), -1);
    UT_SetDefaultReturnValue(UT_KEY(OS_FreeRTOS_GateIsClosed), 1);
    UtAssert_INT32_EQ(OS_CondVarWait_Impl(&token), OS_ERR_INVALID_ID);
    UtAssert_STUB_COUNT(xSemaphoreTake, 1);
    UtAssert_STUB_COUNT(xSemaphoreGive, 1);
}

void Test_OS_CondVarWait_Impl_QueuedBehind(void)
{
    OS_object_token_t token = UT_CONDVAR_TOKEN(0);
    OS_time_t         abs_time;

    /*
     * Another task waits already, so this one joins the list behind it. One signal
     * comes while they both wait, and goes to the one that waited first, as their
     * priorities are the same: this one is not signaled, times out, and takes itself
     * off the list from behind the other, leaving that alone.
     */
    UT_Queue(UT_INDEX_0, 0, UT_TASK_A);
    UT_SetUpWait(true, 0);
    UT_Stub_LocalTime = OS_TimeFromTotalMilliseconds(5000);
    abs_time          = OS_TimeFromTotalMilliseconds(5100);

    UtAssert_INT32_EQ(OS_CondVarTimedWait_Impl(&token, &abs_time), OS_ERROR_TIMEOUT);
    UtAssert_UINT32_EQ(UT_Stub_NotifiedCount, 1);
    UtAssert_True(UT_Stub_NotifiedTasks[0] == UT_TASK_A, "the task that waited first is the one woken");
    UtAssert_True(UT_Waiters[0].signaled, "and marked signaled");
    UtAssert_NULL(UT_CondVarTest_GetWaiters(UT_INDEX_0));

    /* nothing was left pending for this task: its one blocking call was the only wait on its notification */
    UtAssert_STUB_COUNT(ulTaskNotifyTakeIndexed, 1);
}

void Test_OS_CondVarWait_Impl_TimesOutBehind(void)
{
    OS_object_token_t token = UT_CONDVAR_TOKEN(0);
    OS_time_t         abs_time;

    /*
     * Another task waits already, and nothing signals: this one times out with that
     * one still on the list ahead of it, and takes only its own node off, from
     * behind it.
     */
    UT_Queue(UT_INDEX_0, 0, UT_TASK_A);
    UT_SetUpWait(false, 0);
    UT_Stub_LocalTime = OS_TimeFromTotalMilliseconds(5000);
    abs_time          = OS_TimeFromTotalMilliseconds(5100);

    UtAssert_INT32_EQ(OS_CondVarTimedWait_Impl(&token, &abs_time), OS_ERROR_TIMEOUT);
    UtAssert_True(UT_CondVarTest_GetWaiters(UT_INDEX_0) == &UT_Waiters[0], "the other task's node is the one left");
    UtAssert_NULL(UT_Waiters[0].next);
    UtAssert_True(!UT_Waiters[0].signaled, "and it was not signaled");
    UtAssert_UINT32_EQ(UT_Stub_NotifiedCount, 0);
}

void Test_OS_CondVarWait_Impl_QueuedAhead(void)
{
    OS_object_token_t token = UT_CONDVAR_TOKEN(0);

    /*
     * The same two waiters, but this one has the higher priority: the signal goes to it
     * though it waited last, and the other is left on the list, untouched.
     */
    UT_Queue(UT_INDEX_0, 0, UT_TASK_A);
    UT_Stub_SetTaskPriority(UT_SELF, 5);
    UT_Stub_SetTaskPriority(UT_TASK_A, 1);
    UT_SetUpWait(true, 1);

    UtAssert_INT32_EQ(OS_CondVarWait_Impl(&token), OS_SUCCESS);
    UtAssert_UINT32_EQ(UT_Stub_NotifiedCount, 1);
    UtAssert_True(UT_Stub_NotifiedTasks[0] == UT_SELF, "the higher priority task is the one woken");
    UtAssert_True(!UT_Waiters[0].signaled, "the lower priority one is not");
    UtAssert_True(UT_CondVarTest_GetWaiters(UT_INDEX_0) == &UT_Waiters[0], "and stays on the list");
    UtAssert_NULL(UT_Waiters[0].next);
}

void Test_OS_CondVarTimedWait_Impl(void)
{
    OS_object_token_t token = UT_CONDVAR_TOKEN(0);
    OS_time_t         abs_time;

    UT_SetUpWait(false, 0);
    UT_Stub_LocalTime = OS_TimeFromTotalMilliseconds(5000);

    /* 200 ms from now, nobody signals -> the wait is for 200 ms in ticks, and the end of it is a timeout, mutex held */
    abs_time = OS_TimeFromTotalMilliseconds(5200);
    UtAssert_INT32_EQ(OS_CondVarTimedWait_Impl(&token, &abs_time), OS_ERROR_TIMEOUT);
    UtAssert_STUB_COUNT(OS_GetLocalTime, 1);
    UtAssert_STUB_COUNT(OS_FreeRTOS_MsToTicks, 1);
    UtAssert_UINT32_EQ(UT_Stub_LastMsToTicksInput, 200);
    UtAssert_UINT32_EQ(UT_Stub_LastNotifyTakeTicks, UT_Stub_MsToTicksResult);
    UtAssert_STUB_COUNT(xSemaphoreTake, 1);
    UtAssert_NULL(UT_CondVarTest_GetWaiters(UT_INDEX_0));

    /* a part of a millisecond is waited as a whole one, never less than asked for */
    abs_time = OS_TimeAdd(UT_Stub_LocalTime, OS_TimeFromTotalNanoseconds(OS_TIME_TICK_RESOLUTION_NS));
    UtAssert_INT32_EQ(OS_CondVarTimedWait_Impl(&token, &abs_time), OS_ERROR_TIMEOUT);
    UtAssert_UINT32_EQ(UT_Stub_LastMsToTicksInput, 1);

    abs_time = OS_TimeAdd(UT_Stub_LocalTime, OS_TimeFromTotalMicroseconds(1500));
    UtAssert_INT32_EQ(OS_CondVarTimedWait_Impl(&token, &abs_time), OS_ERROR_TIMEOUT);
    UtAssert_UINT32_EQ(UT_Stub_LastMsToTicksInput, 2);

    /* a whole number of milliseconds is not rounded up past itself */
    abs_time = OS_TimeFromTotalMilliseconds(5003);
    UtAssert_INT32_EQ(OS_CondVarTimedWait_Impl(&token, &abs_time), OS_ERROR_TIMEOUT);
    UtAssert_UINT32_EQ(UT_Stub_LastMsToTicksInput, 3);
    UtAssert_STUB_COUNT(OS_FreeRTOS_MsToTicks, 4);

    /* signaled in time -> a success, and not a timeout */
    abs_time = OS_TimeFromTotalMilliseconds(5200);
    UT_ArmBlock(true, 1);
    UtAssert_INT32_EQ(OS_CondVarTimedWait_Impl(&token, &abs_time), OS_SUCCESS);
}

void Test_OS_CondVarTimedWait_Impl_Past(void)
{
    OS_object_token_t token = UT_CONDVAR_TOKEN(0);
    OS_time_t         abs_time;

    UT_SetUpWait(false, 0);
    UT_Stub_LocalTime = OS_TimeFromTotalMilliseconds(5000);

    /* the time is now -> the same wait, for no ticks, which ends at once in a timeout, and the mutex is held again */
    abs_time = UT_Stub_LocalTime;
    UtAssert_INT32_EQ(OS_CondVarTimedWait_Impl(&token, &abs_time), OS_ERROR_TIMEOUT);
    UtAssert_STUB_COUNT(OS_FreeRTOS_MsToTicks, 0);
    UtAssert_UINT32_EQ(UT_Stub_LastNotifyTakeTicks, 0);
    UtAssert_STUB_COUNT(xSemaphoreGive, 1);
    UtAssert_STUB_COUNT(xSemaphoreTake, 1);

    /* the time has gone by -> the same */
    abs_time = OS_TimeFromTotalMilliseconds(4000);
    UtAssert_INT32_EQ(OS_CondVarTimedWait_Impl(&token, &abs_time), OS_ERROR_TIMEOUT);
    UtAssert_STUB_COUNT(OS_FreeRTOS_MsToTicks, 0);
    UtAssert_UINT32_EQ(UT_Stub_LastNotifyTakeTicks, 0);

    /* ... but a signal made on the way in is not lost to it */
    UT_ArmBlock(true, 1);
    UtAssert_INT32_EQ(OS_CondVarTimedWait_Impl(&token, &abs_time), OS_SUCCESS);
}

void Test_OS_CondVarTimedWait_Impl_Far(void)
{
    OS_object_token_t token = UT_CONDVAR_TOKEN(0);
    OS_time_t         abs_time;

    UT_SetUpWait(false, 0);
    UT_Stub_LocalTime = OS_TimeFromTotalMilliseconds(5000);

    /* a wait of months is cut to the longest one there is, so that its ticks cannot wrap round to a short one */
    abs_time = OS_TimeFromTotalMilliseconds(5000 + 10000000000LL);
    UtAssert_INT32_EQ(OS_CondVarTimedWait_Impl(&token, &abs_time), OS_ERROR_TIMEOUT);
    UtAssert_UINT32_EQ(UT_Stub_LastMsToTicksInput, OS_FREERTOS_CONDVAR_MAX_WAIT_MS);

    /* the longest one itself is not cut */
    abs_time = OS_TimeFromTotalMilliseconds(5000LL + OS_FREERTOS_CONDVAR_MAX_WAIT_MS);
    UtAssert_INT32_EQ(OS_CondVarTimedWait_Impl(&token, &abs_time), OS_ERROR_TIMEOUT);
    UtAssert_UINT32_EQ(UT_Stub_LastMsToTicksInput, OS_FREERTOS_CONDVAR_MAX_WAIT_MS);
}

void Test_OS_CondVarTimedWait_Impl_NoClock(void)
{
    OS_object_token_t token = UT_CONDVAR_TOKEN(0);
    OS_time_t         abs_time;

    UT_SetUpWait(false, 0);
    abs_time = OS_TimeFromTotalMilliseconds(5200);

    /* the clock cannot be read -> OS_ERROR, before the condition variable is touched: the mutex is still held */
    UT_SetDeferredRetcode(UT_KEY(OS_GetLocalTime), 1, -1);
    UtAssert_INT32_EQ(OS_CondVarTimedWait_Impl(&token, &abs_time), OS_ERROR);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateEnter, 0);
    UtAssert_STUB_COUNT(xSemaphoreGive, 0);
    UtAssert_STUB_COUNT(OS_FreeRTOS_MsToTicks, 0);
}

void Test_OS_FreeRTOS_CondVarInit(void)
{
    /* registers, for task death, the hook that drops a dead task's wait nodes and orphans its mutexes */
    UtAssert_INT32_EQ(OS_FreeRTOS_CondVarInit(), OS_SUCCESS);
    UtAssert_STUB_COUNT(OS_FreeRTOS_OnTaskDied, 1);
    UtAssert_True(UT_Stub_TaskDiedHook != NULL, "a task death hook is registered");

    /* the registry refuses -> OS_ERROR, which fails OS_API_Init() */
    UT_SetDeferredRetcode(UT_KEY(OS_FreeRTOS_OnTaskDied), 1, -1);
    UtAssert_INT32_EQ(OS_FreeRTOS_CondVarInit(), OS_ERROR);
}

void Test_OS_FreeRTOS_CondVarTaskDied(void)
{
    /* the hook the init registered is what runs when a task dies */
    UtAssert_INT32_EQ(OS_FreeRTOS_CondVarInit(), OS_SUCCESS);

    /* two condition variables exist; the others were never created and are skipped */
    UT_CondVarTest_SetImplMutex(UT_INDEX_0, UT_MUTEX);
    UT_CondVarTest_SetImplMutex(UT_INDEX_1, UT_OTHER_MUTEX);

    /* the dead task waits on both, one of the nodes among those of a task that lives; it holds the second mutex */
    UT_Queue(UT_INDEX_0, 0, UT_TASK_A);
    UT_Queue(UT_INDEX_0, 1, UT_TASK_B);
    UT_Queue(UT_INDEX_0, 2, UT_TASK_A);
    UT_Queue(UT_INDEX_1, 3, UT_TASK_A);
    UT_Stub_SetMutexHolder(UT_OTHER_MUTEX, UT_TASK_A);

    /* the calls in the held one's gate leave one after the other while the call waits a tick for each */
    UT_WakeCalls = 0;
    UT_SetHookFunction(UT_KEY(OS_FreeRTOS_GateWake), UT_WakeReportsUsersLeaving, NULL);

    UT_Stub_TaskDiedHook(UT_TASK_A);

    /* every node of the dead task is gone, from the head, the middle and the end of lists, and the others stay */
    UtAssert_True(UT_CondVarTest_GetWaiters(UT_INDEX_0) == &UT_Waiters[1], "only the live task's node is left");
    UtAssert_NULL(UT_Waiters[1].next);
    UtAssert_NULL(UT_CondVarTest_GetWaiters(UT_INDEX_1));

    /* only the mutex the dead task held was closed, woken and waited on; both lists were cleaned in one section */
    UtAssert_STUB_COUNT(xSemaphoreGetMutexHolder, 2);
    UtAssert_STUB_COUNT(vTaskEnterCritical, 1);
    UtAssert_STUB_COUNT(vTaskExitCritical, 1);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateClose, 1);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateWake, 3);
    UtAssert_STUB_COUNT(vTaskDelay, 2);
    UtAssert_UINT32_EQ(UT_Stub_CondVarLastDelayTicks, 1);
    UtAssert_True(UT_Stub_LastGate == UT_CondVarTest_GetImplGate(UT_INDEX_1), "the held mutex's condition variable");

    /* a task that waits on none and holds none -> nothing is closed or waited on, and the lists are as they were */
    UT_Stub_TaskDiedHook(UT_TASK_C);
    UtAssert_True(UT_CondVarTest_GetWaiters(UT_INDEX_0) == &UT_Waiters[1], "the live task's node is still there");
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateClose, 1);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateWake, 3);
}

void Test_OS_CondVarGetInfo_Impl(void)
{
    OS_object_token_t token = UT_CONDVAR_TOKEN(0);
    OS_condvar_prop_t condvar_prop;

    memset(&condvar_prop, 0, sizeof(condvar_prop));

    UtAssert_INT32_EQ(OS_CondVarGetInfo_Impl(&token, &condvar_prop), OS_SUCCESS);
}

/* ------------------- End of test cases --------------------------------------*/

void Osapi_Test_Setup(void)
{
    UT_ResetState(0);
    memset(UT_Ref_OS_impl_condvar_table, 0, UT_Ref_OS_impl_condvar_table_SIZE);
    memset(UT_Waiters, 0, sizeof(UT_Waiters));
    memset(&UT_Stub_LocalTime, 0, sizeof(UT_Stub_LocalTime));
    UT_Stub_ResetMutexHolders();
    UT_Stub_ResetTaskPriorities();
    UT_Stub_ResetNotifyLog();
    UT_HookArmed   = false;
    UT_HookSignals = false;
    UT_BlockResult = 0;
}

void Osapi_Test_Teardown(void)
{
}

#define ADD_TEST(test) UtTest_Add((Test_##test), Osapi_Test_Setup, Osapi_Test_Teardown, #test)

void UtTest_Setup(void)
{
    ADD_TEST(OS_CondVarCreate_Impl);
    ADD_TEST(OS_CondVarDelete_Impl);
    ADD_TEST(OS_CondVarDelete_Impl_Drain);
    ADD_TEST(OS_CondVarDelete_Impl_Locked);
    ADD_TEST(OS_CondVarDelete_Impl_Orphaned);
    ADD_TEST(OS_CondVarLock_Impl);
    ADD_TEST(OS_CondVarUnlock_Impl);
    ADD_TEST(OS_CondVarSignal_Impl);
    ADD_TEST(OS_CondVarSignal_Impl_Priority);
    ADD_TEST(OS_CondVarBroadcast_Impl);
    ADD_TEST(OS_CondVarWait_Impl);
    ADD_TEST(OS_CondVarWait_Impl_NotHolder);
    ADD_TEST(OS_CondVarWait_Impl_NoSignal);
    ADD_TEST(OS_CondVarWait_Impl_SignalAfterGivingUp);
    ADD_TEST(OS_CondVarWait_Impl_Deleted);
    ADD_TEST(OS_CondVarWait_Impl_RelockWoken);
    ADD_TEST(OS_CondVarWait_Impl_DeletedWhileRelocking);
    ADD_TEST(OS_CondVarWait_Impl_QueuedBehind);
    ADD_TEST(OS_CondVarWait_Impl_TimesOutBehind);
    ADD_TEST(OS_CondVarWait_Impl_QueuedAhead);
    ADD_TEST(OS_CondVarTimedWait_Impl);
    ADD_TEST(OS_CondVarTimedWait_Impl_Past);
    ADD_TEST(OS_CondVarTimedWait_Impl_Far);
    ADD_TEST(OS_CondVarTimedWait_Impl_NoClock);
    ADD_TEST(OS_FreeRTOS_CondVarInit);
    ADD_TEST(OS_FreeRTOS_CondVarTaskDied);
    ADD_TEST(OS_CondVarGetInfo_Impl);
}
