/*
 * File: coveragetest-mutex.c
 *
 * Purpose:
 *   White-box coverage test cases for freertos/src/os-impl-mutex.c, run
 *   against the fake FreeRTOS recursive mutex API in ../fake-inc/semphr.h and
 *   stubbed in ../stubs/freertos-mutex-stubs.c - no real FreeRTOS kernel
 *   involved. (The black box tests for this port are OSAL's own /src/tests and
 *   /src/unit-tests, which link the real OSAL library instead.)
 *
 *   Each _Impl function is called directly; preconditions that its public
 *   API cannot reach are set up through ../adaptors/inc/ut-adaptor-mutex.h,
 *   and effects are observed via return codes plus the fake FreeRTOS stub
 *   layer's captured call arguments and counts.
 *
 *   OSAL mutexes are recursive, so the tests also assert that the recursive
 *   calls are the ones made: xSemaphoreTake()/xSemaphoreGive() on a recursive
 *   mutex would be a bug, and the stub for them is not linked into this runner.
 *
 *   The gate (os-freertos.h) is covered with os-impl-common.c; here it is a
 *   stand-in, so these tests check how the mutex uses it: Take and Give pass
 *   through it, Delete closes it and waits on it.
 */

#include <string.h>

#include "utassert.h"
#include "uttest.h"
#include "utstubs.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "os-shared-mutex.h"
#include "os-shared-idmap.h"

#include "ut-adaptor-mutex.h"

/* Captured stub call arguments, declared in freertos-mutex-stubs.c */
extern SemaphoreHandle_t          UT_Stub_CreatedMutexHandle;
extern SemaphoreHandle_t          UT_Stub_LastDeletedMutex;
extern SemaphoreHandle_t          UT_Stub_LastTakeMutex;
extern TickType_t                 UT_Stub_LastTakeMutexTicks;
extern SemaphoreHandle_t          UT_Stub_LastGiveMutex;
extern SemaphoreHandle_t          UT_Stub_LastHolderQueried;
extern void                       UT_Stub_SetMutexHolder(SemaphoreHandle_t handle, TaskHandle_t holder);
extern void                       UT_Stub_ResetMutexHolders(void);
extern TickType_t                 UT_Stub_MutexLastDelayTicks;
extern const OS_impl_gate_t      *UT_Stub_LastGate;
extern OS_impl_gate_user_t       *UT_Stub_LastEnterUser;
extern const OS_impl_gate_user_t *UT_Stub_LastLeaveUser;
extern OS_FreeRTOS_TaskDiedHook_t UT_Stub_TaskDiedHook;

#define UT_MUTEX_TOKEN(idx)                                                                              \
    (OS_object_token_t)                                                                                  \
    {                                                                                                    \
        .obj_type = OS_OBJECT_TYPE_OS_MUTEX, .obj_id = (osal_id_t) { 0x30000 + (idx) }, .obj_idx = (idx) \
    }

#define UT_INDEX_0 OSAL_INDEX_C(0)

#define UT_MUTEX_HANDLE  ((SemaphoreHandle_t)0x4242)
#define UT_HOLDER_TASK   ((TaskHandle_t)0x7777)
#define UT_OTHER_TASK    ((TaskHandle_t)0x7778)
#define UT_NO_MUTEX_TASK ((TaskHandle_t)0x7779)

/* Inside OS_FreeRTOS_GateWake(): the users left, as a delete sees them while the Takes and Gives already in finish */
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

/* Inside OS_FreeRTOS_GateLeave(): whether the mutex had been handed back by the time the Take left */
static uint32 UT_GivesAtLeave;

static int32 UT_LeaveSeesGives(void *UserObj, int32 StubRetcode, uint32 CallCount, const UT_StubContext_t *Context)
{
    (void)UserObj;
    (void)CallCount;
    (void)Context;

    UT_GivesAtLeave = UT_GetStubCount(UT_KEY(xSemaphoreGiveRecursive));

    return StubRetcode;
}

/*
**********************************************************************************
**          TEST CASES
**********************************************************************************
*/

void Test_OS_MutSemCreate_Impl(void)
{
    OS_object_token_t token = UT_MUTEX_TOKEN(0);

    /* success -> handle stored in the impl table, gate opened */
    UtAssert_INT32_EQ(OS_MutSemCreate_Impl(&token, 0), OS_SUCCESS);
    UtAssert_STUB_COUNT(xSemaphoreCreateRecursiveMutex, 1);
    UtAssert_True(UT_MutexTest_GetImplMutexId(UT_INDEX_0) == UT_Stub_CreatedMutexHandle,
                  "xSemaphoreCreateRecursiveMutex() handle stored in impl table");
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateOpen, 1);
    UtAssert_True(UT_Stub_LastGate == UT_MutexTest_GetImplGate(UT_INDEX_0), "this mutex's gate opened");

    /* xSemaphoreCreateRecursiveMutex() failure (e.g. out of heap) -> OS_SEM_FAILURE, no handle kept */
    UT_SetDeferredRetcode(UT_KEY(xSemaphoreCreateRecursiveMutex), 1, -1);
    UtAssert_INT32_EQ(OS_MutSemCreate_Impl(&token, 0), OS_SEM_FAILURE);
    UtAssert_STUB_COUNT(xSemaphoreCreateRecursiveMutex, 2);
    UtAssert_True(UT_MutexTest_GetImplMutexId(UT_INDEX_0) == NULL,
                  "no handle stored when xSemaphoreCreateRecursiveMutex() fails");
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateOpen, 1);
}

void Test_OS_MutSemDelete_Impl(void)
{
    OS_object_token_t token = UT_MUTEX_TOKEN(0);

    UT_MutexTest_SetImplMutexId(UT_INDEX_0, UT_MUTEX_HANDLE);

    /* not locked, nobody in the gate -> gate closed, no waiting, mutex freed and the slot cleared */
    UtAssert_INT32_EQ(OS_MutSemDelete_Impl(&token), OS_SUCCESS);
    UtAssert_STUB_COUNT(xSemaphoreGetMutexHolder, 1);
    UtAssert_True(UT_Stub_LastHolderQueried == UT_MUTEX_HANDLE, "holder asked of the mutex being deleted");
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateClose, 1);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateWake, 1);
    UtAssert_STUB_COUNT(vTaskDelay, 0);
    UtAssert_STUB_COUNT(vSemaphoreDelete, 1);
    UtAssert_True(UT_Stub_LastDeletedMutex == UT_MUTEX_HANDLE, "vSemaphoreDelete() called with mutex handle");
    UtAssert_True(UT_MutexTest_GetImplMutexId(UT_INDEX_0) == NULL, "mutex handle cleared from impl table");
    UtAssert_True(UT_Stub_LastGate == UT_MutexTest_GetImplGate(UT_INDEX_0), "this mutex's gate used");
}

void Test_OS_MutSemDelete_Impl_Drain(void)
{
    OS_object_token_t token = UT_MUTEX_TOKEN(0);

    UT_MutexTest_SetImplMutexId(UT_INDEX_0, UT_MUTEX_HANDLE);

    /* two Takes/Gives already in the gate, which finish one after the other while the delete waits a tick for each */
    UT_WakeCalls = 0;
    UT_SetHookFunction(UT_KEY(OS_FreeRTOS_GateWake), UT_WakeReportsUsersLeaving, NULL);
    UT_SetHookFunction(UT_KEY(vSemaphoreDelete), UT_FreeSeesWakes, NULL);

    UtAssert_INT32_EQ(OS_MutSemDelete_Impl(&token), OS_SUCCESS);

    UtAssert_STUB_COUNT(OS_FreeRTOS_GateWake, 3);
    UtAssert_STUB_COUNT(vTaskDelay, 2);
    UtAssert_UINT32_EQ(UT_Stub_MutexLastDelayTicks, 1);

    /* the mutex is freed only once the last user has left */
    UtAssert_STUB_COUNT(vSemaphoreDelete, 1);
    UtAssert_UINT32_EQ(UT_WakeCallsAtFree, 3);
}

void Test_OS_MutSemDelete_Impl_Locked(void)
{
    OS_object_token_t token = UT_MUTEX_TOKEN(0);

    UT_MutexTest_SetImplMutexId(UT_INDEX_0, UT_MUTEX_HANDLE);

    /* a task holds it -> refused, and nothing is touched: the gate stays open, the mutex is not freed */
    UT_Stub_SetMutexHolder(UT_MUTEX_HANDLE, UT_HOLDER_TASK);
    UtAssert_INT32_EQ(OS_MutSemDelete_Impl(&token), OS_SEM_FAILURE);
    UtAssert_STUB_COUNT(xSemaphoreGetMutexHolder, 1);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateClose, 0);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateWake, 0);
    UtAssert_STUB_COUNT(vSemaphoreDelete, 0);
    UtAssert_True(UT_MutexTest_GetImplMutexId(UT_INDEX_0) == UT_MUTEX_HANDLE, "mutex handle kept");
}

void Test_OS_MutSemDelete_Impl_Orphaned(void)
{
    OS_object_token_t token = UT_MUTEX_TOKEN(0);

    UT_MutexTest_SetImplMutexId(UT_INDEX_0, UT_MUTEX_HANDLE);

    /* held by a task that is gone, and its gate was closed when it went -> freed like a free mutex */
    UT_Stub_SetMutexHolder(UT_MUTEX_HANDLE, UT_HOLDER_TASK);
    UT_SetDefaultReturnValue(UT_KEY(OS_FreeRTOS_GateIsClosed), 1);

    UtAssert_INT32_EQ(OS_MutSemDelete_Impl(&token), OS_SUCCESS);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateClose, 1);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateWake, 1);
    UtAssert_STUB_COUNT(vSemaphoreDelete, 1);
    UtAssert_True(UT_Stub_LastDeletedMutex == UT_MUTEX_HANDLE, "vSemaphoreDelete() called with mutex handle");
    UtAssert_True(UT_MutexTest_GetImplMutexId(UT_INDEX_0) == NULL, "mutex handle cleared from impl table");
}

void Test_OS_FreeRTOS_MutexInit(void)
{
    /* registers, for task death, the hook that orphans a dead task's mutexes */
    UtAssert_INT32_EQ(OS_FreeRTOS_MutexInit(), OS_SUCCESS);
    UtAssert_STUB_COUNT(OS_FreeRTOS_OnTaskDied, 1);
    UtAssert_True(UT_Stub_TaskDiedHook != NULL, "a task death hook is registered");

    /* the registry refuses -> OS_ERROR, which fails OS_API_Init() */
    UT_SetDeferredRetcode(UT_KEY(OS_FreeRTOS_OnTaskDied), 1, -1);
    UtAssert_INT32_EQ(OS_FreeRTOS_MutexInit(), OS_ERROR);
}

void Test_OS_MutexOwnerDied(void)
{
    SemaphoreHandle_t free_mutex  = (SemaphoreHandle_t)0x5001;
    SemaphoreHandle_t dead_mutex  = (SemaphoreHandle_t)0x5002;
    SemaphoreHandle_t other_mutex = (SemaphoreHandle_t)0x5003;

    /* the hook the init registered is what runs when a task dies */
    UtAssert_INT32_EQ(OS_FreeRTOS_MutexInit(), OS_SUCCESS);

    /* three mutexes exist; the fourth slot was never created and is skipped */
    UT_MutexTest_SetImplMutexId(OSAL_INDEX_C(0), free_mutex);
    UT_MutexTest_SetImplMutexId(OSAL_INDEX_C(1), dead_mutex);
    UT_MutexTest_SetImplMutexId(OSAL_INDEX_C(2), other_mutex);
    UT_Stub_SetMutexHolder(dead_mutex, UT_HOLDER_TASK);
    UT_Stub_SetMutexHolder(other_mutex, UT_OTHER_TASK);

    /* the Takes in the dead task's mutex leave one after the other while the call waits a tick for each */
    UT_WakeCalls = 0;
    UT_SetHookFunction(UT_KEY(OS_FreeRTOS_GateWake), UT_WakeReportsUsersLeaving, NULL);

    UT_Stub_TaskDiedHook(UT_HOLDER_TASK);

    /* only the mutex that task held was closed, woken and waited on */
    UtAssert_STUB_COUNT(xSemaphoreGetMutexHolder, 3);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateClose, 1);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateWake, 3);
    UtAssert_STUB_COUNT(vTaskDelay, 2);
    UtAssert_UINT32_EQ(UT_Stub_MutexLastDelayTicks, 1);
    UtAssert_True(UT_Stub_LastGate == UT_MutexTest_GetImplGate(OSAL_INDEX_C(1)),
                  "the dead task's mutex was the one closed");

    /* a task that holds none of them -> nothing is closed or waited on */
    UT_Stub_TaskDiedHook(UT_NO_MUTEX_TASK);
    UtAssert_STUB_COUNT(xSemaphoreGetMutexHolder, 6);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateClose, 1);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateWake, 3);
}

void Test_OS_MutSemGive_Impl(void)
{
    OS_object_token_t token = UT_MUTEX_TOKEN(0);

    UT_MutexTest_SetImplMutexId(UT_INDEX_0, UT_MUTEX_HANDLE);

    /* success -> passes through the gate with a user node */
    UtAssert_INT32_EQ(OS_MutSemGive_Impl(&token), OS_SUCCESS);
    UtAssert_STUB_COUNT(xSemaphoreGiveRecursive, 1);
    UtAssert_True(UT_Stub_LastGiveMutex == UT_MUTEX_HANDLE, "xSemaphoreGiveRecursive() called with mutex handle");
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateEnter, 1);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 1);
    UtAssert_True(UT_Stub_LastGate == UT_MutexTest_GetImplGate(UT_INDEX_0), "this mutex's gate used");
    UtAssert_True(UT_Stub_LastEnterUser != NULL, "a Give passes a user node");
    UtAssert_True(UT_Stub_LastLeaveUser == UT_Stub_LastEnterUser, "and leaves with the same one");

    /* the caller is not the holder -> OS_SEM_FAILURE, and the gate is still left */
    UT_SetDeferredRetcode(UT_KEY(xSemaphoreGiveRecursive), 1, -1);
    UtAssert_INT32_EQ(OS_MutSemGive_Impl(&token), OS_SEM_FAILURE);
    UtAssert_STUB_COUNT(xSemaphoreGiveRecursive, 2);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 2);

    /* the gate is closed (the mutex is being deleted) -> refused before FreeRTOS is touched */
    UT_SetDeferredRetcode(UT_KEY(OS_FreeRTOS_GateEnter), 1, -1);
    UtAssert_INT32_EQ(OS_MutSemGive_Impl(&token), OS_ERR_INVALID_ID);
    UtAssert_STUB_COUNT(xSemaphoreGiveRecursive, 2);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 2);
}

void Test_OS_MutSemTake_Impl(void)
{
    OS_object_token_t token = UT_MUTEX_TOKEN(0);

    UT_MutexTest_SetImplMutexId(UT_INDEX_0, UT_MUTEX_HANDLE);

    /* success -> passes through the gate with a user node, and waits forever for the mutex */
    UtAssert_INT32_EQ(OS_MutSemTake_Impl(&token), OS_SUCCESS);
    UtAssert_STUB_COUNT(xSemaphoreTakeRecursive, 1);
    UtAssert_True(UT_Stub_LastTakeMutex == UT_MUTEX_HANDLE, "xSemaphoreTakeRecursive() called with mutex handle");
    UtAssert_True(UT_Stub_LastTakeMutexTicks == portMAX_DELAY, "xSemaphoreTakeRecursive() waits forever");
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateEnter, 1);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 1);
    UtAssert_True(UT_Stub_LastGate == UT_MutexTest_GetImplGate(UT_INDEX_0), "this mutex's gate used");
    UtAssert_True(UT_Stub_LastEnterUser != NULL, "a Take passes a user node");
    UtAssert_True(UT_Stub_LastLeaveUser == UT_Stub_LastEnterUser, "and leaves with the same one");
    UtAssert_STUB_COUNT(xSemaphoreGiveRecursive, 0);

    /* xSemaphoreTakeRecursive() failure -> OS_SEM_FAILURE, and the gate is still left */
    UT_SetDeferredRetcode(UT_KEY(xSemaphoreTakeRecursive), 1, -1);
    UtAssert_INT32_EQ(OS_MutSemTake_Impl(&token), OS_SEM_FAILURE);
    UtAssert_STUB_COUNT(xSemaphoreTakeRecursive, 2);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 2);
    UtAssert_STUB_COUNT(xSemaphoreGiveRecursive, 0);

    /* the gate is closed (the mutex is being deleted) -> refused before FreeRTOS is touched */
    UT_SetDeferredRetcode(UT_KEY(OS_FreeRTOS_GateEnter), 1, -1);
    UtAssert_INT32_EQ(OS_MutSemTake_Impl(&token), OS_ERR_INVALID_ID);
    UtAssert_STUB_COUNT(xSemaphoreTakeRecursive, 2);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 2);
}

void Test_OS_MutSemTake_Impl_Woken(void)
{
    OS_object_token_t token = UT_MUTEX_TOKEN(0);

    UT_MutexTest_SetImplMutexId(UT_INDEX_0, UT_MUTEX_HANDLE);

    /* the gate closed (the holder was deleted) and the blocked Take was woken without the mutex -> an error, nothing to
     * hand back */
    UT_SetDefaultReturnValue(UT_KEY(OS_FreeRTOS_GateIsClosed), 1);
    UT_SetDeferredRetcode(UT_KEY(xSemaphoreTakeRecursive), 1, -1);

    UtAssert_INT32_EQ(OS_MutSemTake_Impl(&token), OS_ERR_INVALID_ID);
    UtAssert_STUB_COUNT(xSemaphoreTakeRecursive, 1);
    UtAssert_STUB_COUNT(xSemaphoreGiveRecursive, 0);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 1);
}

void Test_OS_MutSemTake_Impl_Deleted(void)
{
    OS_object_token_t token = UT_MUTEX_TOKEN(0);

    UT_MutexTest_SetImplMutexId(UT_INDEX_0, UT_MUTEX_HANDLE);

    /* the delete closed the gate while this Take waited, then the Take got the mutex: it hands it back, then leaves */
    UT_SetDefaultReturnValue(UT_KEY(OS_FreeRTOS_GateIsClosed), 1);
    UT_SetHookFunction(UT_KEY(OS_FreeRTOS_GateLeave), UT_LeaveSeesGives, NULL);

    UtAssert_INT32_EQ(OS_MutSemTake_Impl(&token), OS_ERR_INVALID_ID);

    UtAssert_STUB_COUNT(xSemaphoreTakeRecursive, 1);
    UtAssert_STUB_COUNT(xSemaphoreGiveRecursive, 1);
    UtAssert_True(UT_Stub_LastGiveMutex == UT_MUTEX_HANDLE, "the mutex that was taken is the one handed back");
    UtAssert_UINT32_EQ(UT_GivesAtLeave, 1);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 1);
}

void Test_OS_MutSemGetInfo_Impl(void)
{
    OS_object_token_t token = UT_MUTEX_TOKEN(0);
    OS_mut_sem_prop_t mut_prop;

    memset(&mut_prop, 0, sizeof(mut_prop));

    UtAssert_INT32_EQ(OS_MutSemGetInfo_Impl(&token, &mut_prop), OS_SUCCESS);
}

/* ------------------- End of test cases --------------------------------------*/

void Osapi_Test_Setup(void)
{
    UT_ResetState(0);
    memset(UT_Ref_OS_impl_mutex_table, 0, UT_Ref_OS_impl_mutex_table_SIZE);
    UT_Stub_ResetMutexHolders();
}

void Osapi_Test_Teardown(void)
{
}

#define ADD_TEST(test) UtTest_Add((Test_##test), Osapi_Test_Setup, Osapi_Test_Teardown, #test)

void UtTest_Setup(void)
{
    ADD_TEST(OS_MutSemCreate_Impl);
    ADD_TEST(OS_MutSemDelete_Impl);
    ADD_TEST(OS_MutSemDelete_Impl_Drain);
    ADD_TEST(OS_MutSemDelete_Impl_Locked);
    ADD_TEST(OS_MutSemDelete_Impl_Orphaned);
    ADD_TEST(OS_FreeRTOS_MutexInit);
    ADD_TEST(OS_MutexOwnerDied);
    ADD_TEST(OS_MutSemGive_Impl);
    ADD_TEST(OS_MutSemTake_Impl);
    ADD_TEST(OS_MutSemTake_Impl_Woken);
    ADD_TEST(OS_MutSemTake_Impl_Deleted);
    ADD_TEST(OS_MutSemGetInfo_Impl);
}
