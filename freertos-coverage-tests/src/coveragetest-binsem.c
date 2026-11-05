/*
 * File: coveragetest-binsem.c
 *
 * Purpose:
 *   White-box coverage test cases for freertos/src/os-impl-binsem.c, run
 *   against the fake FreeRTOS binary semaphore API in ../fake-inc/semphr.h and
 *   stubbed in ../stubs/freertos-binsem-stubs.c - no real FreeRTOS kernel
 *   involved. (The black box tests for this port are OSAL's own /src/tests and
 *   /src/unit-tests, which link the real OSAL library instead.)
 *
 *   Each _Impl function is called directly; preconditions that its public
 *   API cannot reach are set up through ../adaptors/inc/ut-adaptor-binsem.h,
 *   and effects are observed via return codes plus the fake FreeRTOS stub
 *   layer's captured call arguments and counts.
 *
 *   The gate (os-freertos.h) is covered with os-impl-common.c; here it is a
 *   stand-in, so these tests check how the semaphore uses it: Give, Flush,
 *   Take and TimedWait pass through it, Delete closes it and waits on it. A
 *   task blocked in a Take cannot be run here, so a flush or a delete that
 *   wakes one is stood in for by the take failing, with the flush count or the
 *   gate state a real flush or delete would have left.
 */

#include <string.h>

#include "utassert.h"
#include "uttest.h"
#include "utstubs.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "os-shared-binsem.h"
#include "os-shared-idmap.h"

#include "ut-adaptor-binsem.h"

/* Captured stub call arguments, declared in freertos-binsem-stubs.c */
extern SemaphoreHandle_t          UT_Stub_CreatedBinSemHandle;
extern SemaphoreHandle_t          UT_Stub_LastDeletedBinSem;
extern SemaphoreHandle_t          UT_Stub_LastTakeBinSem;
extern TickType_t                 UT_Stub_LastTakeBinSemTicks;
extern SemaphoreHandle_t          UT_Stub_LastGiveBinSem;
extern SemaphoreHandle_t          UT_Stub_LastCountBinSem;
extern uint32                     UT_Stub_LastMsToTicksInput;
extern TickType_t                 UT_Stub_MsToTicksResult;
extern TickType_t                 UT_Stub_BinSemLastDelayTicks;
extern const OS_impl_gate_t      *UT_Stub_LastGate;
extern OS_impl_gate_user_t       *UT_Stub_LastEnterUser;
extern const OS_impl_gate_user_t *UT_Stub_LastLeaveUser;

#define UT_BINSEM_TOKEN(idx)                                                                                   \
    (OS_object_token_t)                                                                                        \
    {                                                                                                          \
        .obj_type = OS_OBJECT_TYPE_OS_BINSEM,                                                                  \
        .obj_id = (osal_id_t) { (OS_OBJECT_TYPE_OS_BINSEM << OS_OBJECT_TYPE_SHIFT) + (idx) }, .obj_idx = (idx) \
    }

#define UT_INDEX_0 OSAL_INDEX_C(0)

#define UT_BINSEM_HANDLE ((SemaphoreHandle_t)0x4343)

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

/* Inside OS_FreeRTOS_GateWake(): the flush count a flush has left for the woken Takes to find */
static uint32 UT_FlushCountAtWake;

static int32 UT_WakeSeesFlushCount(void *UserObj, int32 StubRetcode, uint32 CallCount, const UT_StubContext_t *Context)
{
    (void)UserObj;
    (void)CallCount;
    (void)Context;

    UT_FlushCountAtWake = UT_BinSemTest_GetFlushCount(UT_INDEX_0);

    return StubRetcode;
}

/* Inside xSemaphoreTake(): a flush by another task while this Take waits, which wakes it without the semaphore */
static int32 UT_TakeIsFlushed(void *UserObj, int32 StubRetcode, uint32 CallCount, const UT_StubContext_t *Context)
{
    (void)UserObj;
    (void)CallCount;
    (void)Context;

    UT_BinSemTest_SetFlushCount(UT_INDEX_0, UT_BinSemTest_GetFlushCount(UT_INDEX_0) + 1);

    return StubRetcode;
}

/*
 * Inside OS_FreeRTOS_GateLeave(): the delete finishes the moment the Take has left, and a create makes the slot over,
 * with a flush count of zero, so what the slot says is no longer this semaphore's
 */
static int32
UT_LeaveRecyclesTheSlot(void *UserObj, int32 StubRetcode, uint32 CallCount, const UT_StubContext_t *Context)
{
    (void)UserObj;
    (void)CallCount;
    (void)Context;

    UT_BinSemTest_SetFlushCount(UT_INDEX_0, 0);

    return StubRetcode;
}

/*
**********************************************************************************
**          TEST CASES
**********************************************************************************
*/

void Test_OS_BinSemCreate_Impl(void)
{
    OS_object_token_t token = UT_BINSEM_TOKEN(0);

    /* empty -> handle stored in the impl table, nothing given, flush count cleared, gate opened */
    UT_BinSemTest_SetFlushCount(UT_INDEX_0, 7);
    UtAssert_INT32_EQ(OS_BinSemCreate_Impl(&token, OS_SEM_EMPTY, 0), OS_SUCCESS);
    UtAssert_STUB_COUNT(xSemaphoreCreateBinary, 1);
    UtAssert_True(UT_BinSemTest_GetImplId(UT_INDEX_0) == UT_Stub_CreatedBinSemHandle,
                  "xSemaphoreCreateBinary() handle stored in impl table");
    UtAssert_STUB_COUNT(xSemaphoreGive, 0);
    UtAssert_UINT32_EQ(UT_BinSemTest_GetFlushCount(UT_INDEX_0), 0);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateOpen, 1);
    UtAssert_True(UT_Stub_LastGate == UT_BinSemTest_GetImplGate(UT_INDEX_0), "this semaphore's gate opened");

    /* full -> given once, so it starts full */
    UtAssert_INT32_EQ(OS_BinSemCreate_Impl(&token, OS_SEM_FULL, 0), OS_SUCCESS);
    UtAssert_STUB_COUNT(xSemaphoreGive, 1);
    UtAssert_True(UT_Stub_LastGiveBinSem == UT_Stub_CreatedBinSemHandle, "the new semaphore is the one given");

    /* a value above 1 is full too, as for the POSIX port, and is given once, not that many times */
    UtAssert_INT32_EQ(OS_BinSemCreate_Impl(&token, 5, 0), OS_SUCCESS);
    UtAssert_STUB_COUNT(xSemaphoreGive, 2);

    /* xSemaphoreCreateBinary() failure (e.g. out of heap) -> OS_SEM_FAILURE, no handle kept, nothing opened */
    UT_SetDeferredRetcode(UT_KEY(xSemaphoreCreateBinary), 1, -1);
    UtAssert_INT32_EQ(OS_BinSemCreate_Impl(&token, OS_SEM_FULL, 0), OS_SEM_FAILURE);
    UtAssert_STUB_COUNT(xSemaphoreCreateBinary, 4);
    UtAssert_True(UT_BinSemTest_GetImplId(UT_INDEX_0) == NULL, "no handle stored when xSemaphoreCreateBinary() fails");
    UtAssert_STUB_COUNT(xSemaphoreGive, 2);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateOpen, 3);
}

void Test_OS_BinSemDelete_Impl(void)
{
    OS_object_token_t token = UT_BINSEM_TOKEN(0);

    UT_BinSemTest_SetImplId(UT_INDEX_0, UT_BINSEM_HANDLE);

    /* nobody in the gate -> gate closed, no waiting, semaphore freed and the slot cleared */
    UtAssert_INT32_EQ(OS_BinSemDelete_Impl(&token), OS_SUCCESS);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateClose, 1);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateWake, 1);
    UtAssert_STUB_COUNT(vTaskDelay, 0);
    UtAssert_STUB_COUNT(vSemaphoreDelete, 1);
    UtAssert_True(UT_Stub_LastDeletedBinSem == UT_BINSEM_HANDLE, "vSemaphoreDelete() called with semaphore handle");
    UtAssert_True(UT_BinSemTest_GetImplId(UT_INDEX_0) == NULL, "semaphore handle cleared from impl table");
    UtAssert_True(UT_Stub_LastGate == UT_BinSemTest_GetImplGate(UT_INDEX_0), "this semaphore's gate used");
}

void Test_OS_BinSemDelete_Impl_Drain(void)
{
    OS_object_token_t token = UT_BINSEM_TOKEN(0);

    UT_BinSemTest_SetImplId(UT_INDEX_0, UT_BINSEM_HANDLE);

    /* two calls already in the gate, which finish one after the other while the delete waits a tick for each */
    UT_WakeCalls = 0;
    UT_SetHookFunction(UT_KEY(OS_FreeRTOS_GateWake), UT_WakeReportsUsersLeaving, NULL);
    UT_SetHookFunction(UT_KEY(vSemaphoreDelete), UT_FreeSeesWakes, NULL);

    UtAssert_INT32_EQ(OS_BinSemDelete_Impl(&token), OS_SUCCESS);

    UtAssert_STUB_COUNT(OS_FreeRTOS_GateWake, 3);
    UtAssert_STUB_COUNT(vTaskDelay, 2);
    UtAssert_UINT32_EQ(UT_Stub_BinSemLastDelayTicks, 1);

    /* the semaphore is freed only once the last user has left */
    UtAssert_STUB_COUNT(vSemaphoreDelete, 1);
    UtAssert_UINT32_EQ(UT_WakeCallsAtFree, 3);
}

void Test_OS_BinSemFlush_Impl(void)
{
    OS_object_token_t token = UT_BINSEM_TOKEN(0);

    UT_BinSemTest_SetImplId(UT_INDEX_0, UT_BINSEM_HANDLE);
    UT_SetHookFunction(UT_KEY(OS_FreeRTOS_GateWake), UT_WakeSeesFlushCount, NULL);

    /* success -> passes through the gate with a user node, adds one flush and wakes the gate's blocked users */
    UtAssert_INT32_EQ(OS_BinSemFlush_Impl(&token), OS_SUCCESS);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateEnter, 1);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 1);
    UtAssert_True(UT_Stub_LastEnterUser != NULL, "a Flush passes a user node");
    UtAssert_True(UT_Stub_LastLeaveUser == UT_Stub_LastEnterUser, "and leaves with the same one");
    UtAssert_UINT32_EQ(UT_BinSemTest_GetFlushCount(UT_INDEX_0), 1);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateWake, 1);
    UtAssert_True(UT_Stub_LastGate == UT_BinSemTest_GetImplGate(UT_INDEX_0), "this semaphore's gate used");

    /* the count is added in a critical section, and before the wake, so that a Take that is woken sees it */
    UtAssert_STUB_COUNT(vTaskEnterCritical, 1);
    UtAssert_STUB_COUNT(vTaskExitCritical, 1);
    UtAssert_UINT32_EQ(UT_FlushCountAtWake, 1);

    /* the semaphore itself is left alone: nothing taken, given or freed */
    UtAssert_STUB_COUNT(xSemaphoreTake, 0);
    UtAssert_STUB_COUNT(xSemaphoreGive, 0);
    UtAssert_STUB_COUNT(vSemaphoreDelete, 0);

    /* each flush adds one more */
    UtAssert_INT32_EQ(OS_BinSemFlush_Impl(&token), OS_SUCCESS);
    UtAssert_UINT32_EQ(UT_BinSemTest_GetFlushCount(UT_INDEX_0), 2);
    UtAssert_UINT32_EQ(UT_FlushCountAtWake, 2);

    /* the gate is closed (the semaphore is being deleted) -> refused, with nothing flushed or woken */
    UT_SetDeferredRetcode(UT_KEY(OS_FreeRTOS_GateEnter), 1, -1);
    UtAssert_INT32_EQ(OS_BinSemFlush_Impl(&token), OS_ERR_INVALID_ID);
    UtAssert_UINT32_EQ(UT_BinSemTest_GetFlushCount(UT_INDEX_0), 2);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateWake, 2);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 2);
}

void Test_OS_BinSemGive_Impl(void)
{
    OS_object_token_t token = UT_BINSEM_TOKEN(0);

    UT_BinSemTest_SetImplId(UT_INDEX_0, UT_BINSEM_HANDLE);

    /* success -> passes through the gate with a user node */
    UtAssert_INT32_EQ(OS_BinSemGive_Impl(&token), OS_SUCCESS);
    UtAssert_STUB_COUNT(xSemaphoreGive, 1);
    UtAssert_True(UT_Stub_LastGiveBinSem == UT_BINSEM_HANDLE, "xSemaphoreGive() called with semaphore handle");
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateEnter, 1);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 1);
    UtAssert_True(UT_Stub_LastGate == UT_BinSemTest_GetImplGate(UT_INDEX_0), "this semaphore's gate used");
    UtAssert_True(UT_Stub_LastEnterUser != NULL, "a Give passes a user node");
    UtAssert_True(UT_Stub_LastLeaveUser == UT_Stub_LastEnterUser, "and leaves with the same one");

    /* already full -> not an error for a binary semaphore, and the gate is still left */
    UT_SetDeferredRetcode(UT_KEY(xSemaphoreGive), 1, -1);
    UtAssert_INT32_EQ(OS_BinSemGive_Impl(&token), OS_SUCCESS);
    UtAssert_STUB_COUNT(xSemaphoreGive, 2);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 2);

    /* the gate is closed (the semaphore is being deleted) -> refused before FreeRTOS is touched */
    UT_SetDeferredRetcode(UT_KEY(OS_FreeRTOS_GateEnter), 1, -1);
    UtAssert_INT32_EQ(OS_BinSemGive_Impl(&token), OS_ERR_INVALID_ID);
    UtAssert_STUB_COUNT(xSemaphoreGive, 2);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 2);
}

void Test_OS_BinSemTake_Impl(void)
{
    OS_object_token_t token = UT_BINSEM_TOKEN(0);

    UT_BinSemTest_SetImplId(UT_INDEX_0, UT_BINSEM_HANDLE);

    /* success -> passes through the gate with a user node, and waits forever for the semaphore */
    UtAssert_INT32_EQ(OS_BinSemTake_Impl(&token), OS_SUCCESS);
    UtAssert_STUB_COUNT(xSemaphoreTake, 1);
    UtAssert_True(UT_Stub_LastTakeBinSem == UT_BINSEM_HANDLE, "xSemaphoreTake() called with semaphore handle");
    UtAssert_True(UT_Stub_LastTakeBinSemTicks == portMAX_DELAY, "xSemaphoreTake() waits forever");
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateEnter, 1);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 1);
    UtAssert_True(UT_Stub_LastGate == UT_BinSemTest_GetImplGate(UT_INDEX_0), "this semaphore's gate used");
    UtAssert_True(UT_Stub_LastEnterUser != NULL, "a Take passes a user node");
    UtAssert_True(UT_Stub_LastLeaveUser == UT_Stub_LastEnterUser, "and leaves with the same one");
    UtAssert_STUB_COUNT(OS_FreeRTOS_MsToTicks, 0);

    /* the wait ended without the semaphore, and nothing explains it -> OS_SEM_FAILURE, and the gate is still left */
    UT_SetDeferredRetcode(UT_KEY(xSemaphoreTake), 1, -1);
    UtAssert_INT32_EQ(OS_BinSemTake_Impl(&token), OS_SEM_FAILURE);
    UtAssert_STUB_COUNT(xSemaphoreTake, 2);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 2);

    /* the gate is closed (the semaphore is being deleted) -> refused before FreeRTOS is touched */
    UT_SetDeferredRetcode(UT_KEY(OS_FreeRTOS_GateEnter), 1, -1);
    UtAssert_INT32_EQ(OS_BinSemTake_Impl(&token), OS_ERR_INVALID_ID);
    UtAssert_STUB_COUNT(xSemaphoreTake, 2);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 2);
}

void Test_OS_BinSemTake_Impl_Woken(void)
{
    OS_object_token_t token = UT_BINSEM_TOKEN(0);

    UT_BinSemTest_SetImplId(UT_INDEX_0, UT_BINSEM_HANDLE);

    /* the delete closed the gate while this Take waited, and woke it without the semaphore -> an error */
    UT_SetDefaultReturnValue(UT_KEY(OS_FreeRTOS_GateLeave), 1);
    UT_SetDeferredRetcode(UT_KEY(xSemaphoreTake), 1, -1);
    UtAssert_INT32_EQ(OS_BinSemTake_Impl(&token), OS_ERR_INVALID_ID);
    UtAssert_STUB_COUNT(xSemaphoreTake, 1);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 1);

    /* the Take got the semaphore just as the delete began -> it took it, so it succeeds */
    UtAssert_INT32_EQ(OS_BinSemTake_Impl(&token), OS_SUCCESS);
    UtAssert_STUB_COUNT(xSemaphoreTake, 2);
}

void Test_OS_BinSemTake_Impl_Flushed(void)
{
    OS_object_token_t token = UT_BINSEM_TOKEN(0);

    UT_BinSemTest_SetImplId(UT_INDEX_0, UT_BINSEM_HANDLE);
    UT_BinSemTest_SetFlushCount(UT_INDEX_0, 3);

    /* flushes in the past are not one this Take was waiting for: the count it sees did not change */
    UT_SetDeferredRetcode(UT_KEY(xSemaphoreTake), 1, -1);
    UtAssert_INT32_EQ(OS_BinSemTake_Impl(&token), OS_SEM_FAILURE);

    /* a flush came while this Take waited, and woke it without the semaphore -> it succeeds, having taken nothing */
    UT_SetHookFunction(UT_KEY(xSemaphoreTake), UT_TakeIsFlushed, NULL);
    UT_SetDeferredRetcode(UT_KEY(xSemaphoreTake), 1, -1);
    UtAssert_INT32_EQ(OS_BinSemTake_Impl(&token), OS_SUCCESS);
    UtAssert_STUB_COUNT(xSemaphoreTake, 2);
    UtAssert_STUB_COUNT(xSemaphoreGive, 0);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 2);

    /* flushed and deleted at once -> deleted: the semaphore is gone, whatever the flush did */
    UT_SetDefaultReturnValue(UT_KEY(OS_FreeRTOS_GateLeave), 1);
    UT_SetDeferredRetcode(UT_KEY(xSemaphoreTake), 1, -1);
    UtAssert_INT32_EQ(OS_BinSemTake_Impl(&token), OS_ERR_INVALID_ID);
}

void Test_OS_BinSemTake_Impl_SlotRecycled(void)
{
    OS_object_token_t token = UT_BINSEM_TOKEN(0);

    UT_BinSemTest_SetImplId(UT_INDEX_0, UT_BINSEM_HANDLE);

    /*
     * Flushed, and a delete and a create then make the slot over as soon as the Take has left the gate, which puts
     * the count back to what it was when the Take began. The Take has to have worked out why it was woken while
     * still in, or it would find the count unchanged and call this a failure.
     */
    UT_SetHookFunction(UT_KEY(xSemaphoreTake), UT_TakeIsFlushed, NULL);
    UT_SetHookFunction(UT_KEY(OS_FreeRTOS_GateLeave), UT_LeaveRecyclesTheSlot, NULL);
    UT_SetDeferredRetcode(UT_KEY(xSemaphoreTake), 1, -1);

    UtAssert_INT32_EQ(OS_BinSemTake_Impl(&token), OS_SUCCESS);
    UtAssert_UINT32_EQ(UT_BinSemTest_GetFlushCount(UT_INDEX_0), 0);
}

void Test_OS_BinSemTimedWait_Impl(void)
{
    OS_object_token_t token = UT_BINSEM_TOKEN(0);

    UT_BinSemTest_SetImplId(UT_INDEX_0, UT_BINSEM_HANDLE);

    /* success -> the milliseconds are converted by OS_FreeRTOS_MsToTicks() and that is how long the wait is */
    UtAssert_INT32_EQ(OS_BinSemTimedWait_Impl(&token, 250), OS_SUCCESS);
    UtAssert_STUB_COUNT(OS_FreeRTOS_MsToTicks, 1);
    UtAssert_UINT32_EQ(UT_Stub_LastMsToTicksInput, 250);
    UtAssert_STUB_COUNT(xSemaphoreTake, 1);
    UtAssert_True(UT_Stub_LastTakeBinSem == UT_BINSEM_HANDLE, "xSemaphoreTake() called with semaphore handle");
    UtAssert_UINT32_EQ(UT_Stub_LastTakeBinSemTicks, UT_Stub_MsToTicksResult);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateEnter, 1);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 1);
    UtAssert_True(UT_Stub_LastGate == UT_BinSemTest_GetImplGate(UT_INDEX_0), "this semaphore's gate used");
    UtAssert_True(UT_Stub_LastEnterUser != NULL, "a TimedWait passes a user node");
    UtAssert_True(UT_Stub_LastLeaveUser == UT_Stub_LastEnterUser, "and leaves with the same one");

    /* the time ran out -> OS_SEM_TIMEOUT, and the gate is still left */
    UT_SetDeferredRetcode(UT_KEY(xSemaphoreTake), 1, -1);
    UtAssert_INT32_EQ(OS_BinSemTimedWait_Impl(&token, 0), OS_SEM_TIMEOUT);
    UtAssert_UINT32_EQ(UT_Stub_LastMsToTicksInput, 0);
    UtAssert_STUB_COUNT(xSemaphoreTake, 2);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 2);

    /* the gate is closed (the semaphore is being deleted) -> refused before FreeRTOS is touched */
    UT_SetDeferredRetcode(UT_KEY(OS_FreeRTOS_GateEnter), 1, -1);
    UtAssert_INT32_EQ(OS_BinSemTimedWait_Impl(&token, 250), OS_ERR_INVALID_ID);
    UtAssert_STUB_COUNT(xSemaphoreTake, 2);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 2);
}

void Test_OS_BinSemTimedWait_Impl_Woken(void)
{
    OS_object_token_t token = UT_BINSEM_TOKEN(0);

    UT_BinSemTest_SetImplId(UT_INDEX_0, UT_BINSEM_HANDLE);

    /* the delete woke the wait: not a timeout */
    UT_SetDefaultReturnValue(UT_KEY(OS_FreeRTOS_GateLeave), 1);
    UT_SetDeferredRetcode(UT_KEY(xSemaphoreTake), 1, -1);
    UtAssert_INT32_EQ(OS_BinSemTimedWait_Impl(&token, 250), OS_ERR_INVALID_ID);

    /* a flush woke the wait: not a timeout either, and the semaphore was not taken */
    UT_ClearDefaultReturnValue(UT_KEY(OS_FreeRTOS_GateLeave));
    UT_SetHookFunction(UT_KEY(xSemaphoreTake), UT_TakeIsFlushed, NULL);
    UT_SetDeferredRetcode(UT_KEY(xSemaphoreTake), 1, -1);
    UtAssert_INT32_EQ(OS_BinSemTimedWait_Impl(&token, 250), OS_SUCCESS);
    UtAssert_STUB_COUNT(xSemaphoreGive, 0);
}

void Test_OS_BinSemGetInfo_Impl(void)
{
    OS_object_token_t token = UT_BINSEM_TOKEN(0);
    OS_bin_sem_prop_t bin_prop;

    UT_BinSemTest_SetImplId(UT_INDEX_0, UT_BINSEM_HANDLE);

    /* full -> value 1, read from this semaphore */
    memset(&bin_prop, 0xA5, sizeof(bin_prop));
    UT_SetDefaultReturnValue(UT_KEY(uxSemaphoreGetCount), 1);
    UtAssert_INT32_EQ(OS_BinSemGetInfo_Impl(&token, &bin_prop), OS_SUCCESS);
    UtAssert_INT32_EQ(bin_prop.value, 1);
    UtAssert_True(UT_Stub_LastCountBinSem == UT_BINSEM_HANDLE, "the count of this semaphore was read");

    /* empty -> value 0 */
    UT_SetDefaultReturnValue(UT_KEY(uxSemaphoreGetCount), 0);
    UtAssert_INT32_EQ(OS_BinSemGetInfo_Impl(&token, &bin_prop), OS_SUCCESS);
    UtAssert_INT32_EQ(bin_prop.value, 0);

    /* the shared layer holds off a delete while this runs, so it needs no gate */
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateEnter, 0);
}

/* ------------------- End of test cases --------------------------------------*/

void Osapi_Test_Setup(void)
{
    UT_ResetState(0);
    memset(UT_Ref_OS_impl_bin_sem_table, 0, UT_Ref_OS_impl_bin_sem_table_SIZE);
}

void Osapi_Test_Teardown(void)
{
}

#define ADD_TEST(test) UtTest_Add((Test_##test), Osapi_Test_Setup, Osapi_Test_Teardown, #test)

void UtTest_Setup(void)
{
    ADD_TEST(OS_BinSemCreate_Impl);
    ADD_TEST(OS_BinSemDelete_Impl);
    ADD_TEST(OS_BinSemDelete_Impl_Drain);
    ADD_TEST(OS_BinSemFlush_Impl);
    ADD_TEST(OS_BinSemGive_Impl);
    ADD_TEST(OS_BinSemTake_Impl);
    ADD_TEST(OS_BinSemTake_Impl_Woken);
    ADD_TEST(OS_BinSemTake_Impl_Flushed);
    ADD_TEST(OS_BinSemTake_Impl_SlotRecycled);
    ADD_TEST(OS_BinSemTimedWait_Impl);
    ADD_TEST(OS_BinSemTimedWait_Impl_Woken);
    ADD_TEST(OS_BinSemGetInfo_Impl);
}
