/*
 * File: coveragetest-countsem.c
 *
 * Purpose:
 *   White-box coverage test cases for freertos/src/os-impl-countsem.c, run
 *   against the fake FreeRTOS counting semaphore API in ../fake-inc/semphr.h
 *   and stubbed in ../stubs/freertos-countsem-stubs.c - no real FreeRTOS kernel
 *   involved. (The black box tests for this port are OSAL's own /src/tests and
 *   /src/unit-tests, which link the real OSAL library instead.)
 *
 *   Each _Impl function is called directly; preconditions that its public
 *   API cannot reach are set up through ../adaptors/inc/ut-adaptor-countsem.h,
 *   and effects are observed via return codes plus the fake FreeRTOS stub
 *   layer's captured call arguments and counts.
 *
 *   The gate (os-freertos.h) is covered with os-impl-common.c; here it is a
 *   stand-in, so these tests check how the semaphore uses it: Give, Take and
 *   TimedWait pass through it, Delete closes it and waits on it. A task blocked
 *   in a Take cannot be run here, so a delete that wakes one is stood in for by
 *   the take failing, with the gate reporting closed when the Take leaves it.
 */

#include <string.h>

#include "utassert.h"
#include "uttest.h"
#include "utstubs.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "os-shared-countsem.h"
#include "os-shared-idmap.h"

#include "ut-adaptor-countsem.h"

#include "os-impl-countsem.h"

/* Captured stub call arguments, declared in freertos-countsem-stubs.c */
extern SemaphoreHandle_t          UT_Stub_CreatedCountSemHandle;
extern UBaseType_t                UT_Stub_LastCreateMaxCount;
extern UBaseType_t                UT_Stub_LastCreateInitialCount;
extern SemaphoreHandle_t          UT_Stub_LastDeletedCountSem;
extern SemaphoreHandle_t          UT_Stub_LastTakeCountSem;
extern TickType_t                 UT_Stub_LastTakeCountSemTicks;
extern SemaphoreHandle_t          UT_Stub_LastGiveCountSem;
extern SemaphoreHandle_t          UT_Stub_LastCountCountSem;
extern uint32                     UT_Stub_LastMsToTicksInput;
extern TickType_t                 UT_Stub_MsToTicksResult;
extern TickType_t                 UT_Stub_CountSemLastDelayTicks;
extern const OS_impl_gate_t      *UT_Stub_LastGate;
extern OS_impl_gate_user_t       *UT_Stub_LastEnterUser;
extern const OS_impl_gate_user_t *UT_Stub_LastLeaveUser;

#define UT_COUNTSEM_TOKEN(idx)                                                                                   \
    (OS_object_token_t)                                                                                          \
    {                                                                                                            \
        .obj_type = OS_OBJECT_TYPE_OS_COUNTSEM,                                                                  \
        .obj_id = (osal_id_t) { (OS_OBJECT_TYPE_OS_COUNTSEM << OS_OBJECT_TYPE_SHIFT) + (idx) }, .obj_idx = (idx) \
    }

#define UT_INDEX_0 OSAL_INDEX_C(0)

#define UT_COUNTSEM_HANDLE ((SemaphoreHandle_t)0x4444)

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
**********************************************************************************
**          TEST CASES
**********************************************************************************
*/

void Test_OS_CountSemCreate_Impl(void)
{
    OS_object_token_t token = UT_COUNTSEM_TOKEN(0);

    /* zero -> handle stored in the impl table, made with the port's limit and the initial value, gate opened */
    UtAssert_INT32_EQ(OS_CountSemCreate_Impl(&token, 0, 0), OS_SUCCESS);
    UtAssert_STUB_COUNT(xSemaphoreCreateCounting, 1);
    UtAssert_True(UT_CountSemTest_GetImplId(UT_INDEX_0) == UT_Stub_CreatedCountSemHandle,
                  "xSemaphoreCreateCounting() handle stored in impl table");
    UtAssert_UINT32_EQ(UT_Stub_LastCreateMaxCount, OS_FREERTOS_SEM_VALUE_MAX);
    UtAssert_UINT32_EQ(UT_Stub_LastCreateInitialCount, 0);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateOpen, 1);
    UtAssert_True(UT_Stub_LastGate == UT_CountSemTest_GetImplGate(UT_INDEX_0), "this semaphore's gate opened");

    /* another value -> that is the initial count */
    UtAssert_INT32_EQ(OS_CountSemCreate_Impl(&token, 2, 0), OS_SUCCESS);
    UtAssert_UINT32_EQ(UT_Stub_LastCreateInitialCount, 2);

    /* the highest value there is -> accepted */
    UtAssert_INT32_EQ(OS_CountSemCreate_Impl(&token, OS_FREERTOS_SEM_VALUE_MAX, 0), OS_SUCCESS);
    UtAssert_UINT32_EQ(UT_Stub_LastCreateInitialCount, OS_FREERTOS_SEM_VALUE_MAX);
    UtAssert_STUB_COUNT(xSemaphoreCreateCounting, 3);

    /* one above it -> OS_INVALID_SEM_VALUE, before FreeRTOS is asked for anything, nothing opened */
    UT_CountSemTest_SetImplId(UT_INDEX_0, NULL);
    UtAssert_INT32_EQ(OS_CountSemCreate_Impl(&token, OS_FREERTOS_SEM_VALUE_MAX + 1, 0), OS_INVALID_SEM_VALUE);
    UtAssert_INT32_EQ(OS_CountSemCreate_Impl(&token, 0xFFFFFFFF, 0), OS_INVALID_SEM_VALUE);
    UtAssert_STUB_COUNT(xSemaphoreCreateCounting, 3);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateOpen, 3);
    UtAssert_True(UT_CountSemTest_GetImplId(UT_INDEX_0) == NULL, "no handle stored for a value that is too high");

    /* xSemaphoreCreateCounting() failure (e.g. out of heap) -> OS_SEM_FAILURE, no handle kept, nothing opened */
    UT_SetDeferredRetcode(UT_KEY(xSemaphoreCreateCounting), 1, -1);
    UtAssert_INT32_EQ(OS_CountSemCreate_Impl(&token, 1, 0), OS_SEM_FAILURE);
    UtAssert_STUB_COUNT(xSemaphoreCreateCounting, 4);
    UtAssert_True(UT_CountSemTest_GetImplId(UT_INDEX_0) == NULL,
                  "no handle stored when xSemaphoreCreateCounting() fails");
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateOpen, 3);
}

void Test_OS_CountSemDelete_Impl(void)
{
    OS_object_token_t token = UT_COUNTSEM_TOKEN(0);

    UT_CountSemTest_SetImplId(UT_INDEX_0, UT_COUNTSEM_HANDLE);

    /* nobody in the gate -> gate closed, no waiting, semaphore freed and the slot cleared */
    UtAssert_INT32_EQ(OS_CountSemDelete_Impl(&token), OS_SUCCESS);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateClose, 1);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateWake, 1);
    UtAssert_STUB_COUNT(vTaskDelay, 0);
    UtAssert_STUB_COUNT(vSemaphoreDelete, 1);
    UtAssert_True(UT_Stub_LastDeletedCountSem == UT_COUNTSEM_HANDLE, "vSemaphoreDelete() called with semaphore handle");
    UtAssert_True(UT_CountSemTest_GetImplId(UT_INDEX_0) == NULL, "semaphore handle cleared from impl table");
    UtAssert_True(UT_Stub_LastGate == UT_CountSemTest_GetImplGate(UT_INDEX_0), "this semaphore's gate used");
}

void Test_OS_CountSemDelete_Impl_Drain(void)
{
    OS_object_token_t token = UT_COUNTSEM_TOKEN(0);

    UT_CountSemTest_SetImplId(UT_INDEX_0, UT_COUNTSEM_HANDLE);

    /* two calls already in the gate, which finish one after the other while the delete waits a tick for each */
    UT_WakeCalls = 0;
    UT_SetHookFunction(UT_KEY(OS_FreeRTOS_GateWake), UT_WakeReportsUsersLeaving, NULL);
    UT_SetHookFunction(UT_KEY(vSemaphoreDelete), UT_FreeSeesWakes, NULL);

    UtAssert_INT32_EQ(OS_CountSemDelete_Impl(&token), OS_SUCCESS);

    UtAssert_STUB_COUNT(OS_FreeRTOS_GateWake, 3);
    UtAssert_STUB_COUNT(vTaskDelay, 2);
    UtAssert_UINT32_EQ(UT_Stub_CountSemLastDelayTicks, 1);

    /* the semaphore is freed only once the last user has left */
    UtAssert_STUB_COUNT(vSemaphoreDelete, 1);
    UtAssert_UINT32_EQ(UT_WakeCallsAtFree, 3);
}

void Test_OS_CountSemGive_Impl(void)
{
    OS_object_token_t token = UT_COUNTSEM_TOKEN(0);

    UT_CountSemTest_SetImplId(UT_INDEX_0, UT_COUNTSEM_HANDLE);

    /* success -> passes through the gate with a user node */
    UtAssert_INT32_EQ(OS_CountSemGive_Impl(&token), OS_SUCCESS);
    UtAssert_STUB_COUNT(xSemaphoreGive, 1);
    UtAssert_True(UT_Stub_LastGiveCountSem == UT_COUNTSEM_HANDLE, "xSemaphoreGive() called with semaphore handle");
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateEnter, 1);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 1);
    UtAssert_True(UT_Stub_LastGate == UT_CountSemTest_GetImplGate(UT_INDEX_0), "this semaphore's gate used");
    UtAssert_True(UT_Stub_LastEnterUser != NULL, "a Give passes a user node");
    UtAssert_True(UT_Stub_LastLeaveUser == UT_Stub_LastEnterUser, "and leaves with the same one");

    /* the count is at its limit -> OS_SEM_FAILURE, and the gate is still left */
    UT_SetDeferredRetcode(UT_KEY(xSemaphoreGive), 1, -1);
    UtAssert_INT32_EQ(OS_CountSemGive_Impl(&token), OS_SEM_FAILURE);
    UtAssert_STUB_COUNT(xSemaphoreGive, 2);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 2);

    /* the gate is closed (the semaphore is being deleted) -> refused before FreeRTOS is touched */
    UT_SetDeferredRetcode(UT_KEY(OS_FreeRTOS_GateEnter), 1, -1);
    UtAssert_INT32_EQ(OS_CountSemGive_Impl(&token), OS_ERR_INVALID_ID);
    UtAssert_STUB_COUNT(xSemaphoreGive, 2);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 2);
}

void Test_OS_CountSemTake_Impl(void)
{
    OS_object_token_t token = UT_COUNTSEM_TOKEN(0);

    UT_CountSemTest_SetImplId(UT_INDEX_0, UT_COUNTSEM_HANDLE);

    /* success -> passes through the gate with a user node, and waits forever for the semaphore */
    UtAssert_INT32_EQ(OS_CountSemTake_Impl(&token), OS_SUCCESS);
    UtAssert_STUB_COUNT(xSemaphoreTake, 1);
    UtAssert_True(UT_Stub_LastTakeCountSem == UT_COUNTSEM_HANDLE, "xSemaphoreTake() called with semaphore handle");
    UtAssert_True(UT_Stub_LastTakeCountSemTicks == portMAX_DELAY, "xSemaphoreTake() waits forever");
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateEnter, 1);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 1);
    UtAssert_True(UT_Stub_LastGate == UT_CountSemTest_GetImplGate(UT_INDEX_0), "this semaphore's gate used");
    UtAssert_True(UT_Stub_LastEnterUser != NULL, "a Take passes a user node");
    UtAssert_True(UT_Stub_LastLeaveUser == UT_Stub_LastEnterUser, "and leaves with the same one");
    UtAssert_STUB_COUNT(OS_FreeRTOS_MsToTicks, 0);

    /* the wait ended without the semaphore, and nothing explains it -> OS_SEM_FAILURE, and the gate is still left */
    UT_SetDeferredRetcode(UT_KEY(xSemaphoreTake), 1, -1);
    UtAssert_INT32_EQ(OS_CountSemTake_Impl(&token), OS_SEM_FAILURE);
    UtAssert_STUB_COUNT(xSemaphoreTake, 2);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 2);

    /* the gate is closed (the semaphore is being deleted) -> refused before FreeRTOS is touched */
    UT_SetDeferredRetcode(UT_KEY(OS_FreeRTOS_GateEnter), 1, -1);
    UtAssert_INT32_EQ(OS_CountSemTake_Impl(&token), OS_ERR_INVALID_ID);
    UtAssert_STUB_COUNT(xSemaphoreTake, 2);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 2);
}

void Test_OS_CountSemTake_Impl_Woken(void)
{
    OS_object_token_t token = UT_COUNTSEM_TOKEN(0);

    UT_CountSemTest_SetImplId(UT_INDEX_0, UT_COUNTSEM_HANDLE);

    /* the delete closed the gate while this Take waited, and woke it without the semaphore -> an error */
    UT_SetDefaultReturnValue(UT_KEY(OS_FreeRTOS_GateLeave), 1);
    UT_SetDeferredRetcode(UT_KEY(xSemaphoreTake), 1, -1);
    UtAssert_INT32_EQ(OS_CountSemTake_Impl(&token), OS_ERR_INVALID_ID);
    UtAssert_STUB_COUNT(xSemaphoreTake, 1);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 1);

    /* the Take got the semaphore just as the delete began -> it took it, so it succeeds */
    UtAssert_INT32_EQ(OS_CountSemTake_Impl(&token), OS_SUCCESS);
    UtAssert_STUB_COUNT(xSemaphoreTake, 2);
}

void Test_OS_CountSemTimedWait_Impl(void)
{
    OS_object_token_t token = UT_COUNTSEM_TOKEN(0);

    UT_CountSemTest_SetImplId(UT_INDEX_0, UT_COUNTSEM_HANDLE);

    /* success -> the milliseconds are converted by OS_FreeRTOS_MsToTicks() and that is how long the wait is */
    UtAssert_INT32_EQ(OS_CountSemTimedWait_Impl(&token, 250), OS_SUCCESS);
    UtAssert_STUB_COUNT(OS_FreeRTOS_MsToTicks, 1);
    UtAssert_UINT32_EQ(UT_Stub_LastMsToTicksInput, 250);
    UtAssert_STUB_COUNT(xSemaphoreTake, 1);
    UtAssert_True(UT_Stub_LastTakeCountSem == UT_COUNTSEM_HANDLE, "xSemaphoreTake() called with semaphore handle");
    UtAssert_UINT32_EQ(UT_Stub_LastTakeCountSemTicks, UT_Stub_MsToTicksResult);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateEnter, 1);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 1);
    UtAssert_True(UT_Stub_LastGate == UT_CountSemTest_GetImplGate(UT_INDEX_0), "this semaphore's gate used");
    UtAssert_True(UT_Stub_LastEnterUser != NULL, "a TimedWait passes a user node");
    UtAssert_True(UT_Stub_LastLeaveUser == UT_Stub_LastEnterUser, "and leaves with the same one");

    /* the time ran out -> OS_SEM_TIMEOUT, and the gate is still left */
    UT_SetDeferredRetcode(UT_KEY(xSemaphoreTake), 1, -1);
    UtAssert_INT32_EQ(OS_CountSemTimedWait_Impl(&token, 0), OS_SEM_TIMEOUT);
    UtAssert_UINT32_EQ(UT_Stub_LastMsToTicksInput, 0);
    UtAssert_STUB_COUNT(xSemaphoreTake, 2);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 2);

    /* the gate is closed (the semaphore is being deleted) -> refused before FreeRTOS is touched */
    UT_SetDeferredRetcode(UT_KEY(OS_FreeRTOS_GateEnter), 1, -1);
    UtAssert_INT32_EQ(OS_CountSemTimedWait_Impl(&token, 250), OS_ERR_INVALID_ID);
    UtAssert_STUB_COUNT(xSemaphoreTake, 2);
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateLeave, 2);
}

void Test_OS_CountSemTimedWait_Impl_Woken(void)
{
    OS_object_token_t token = UT_COUNTSEM_TOKEN(0);

    UT_CountSemTest_SetImplId(UT_INDEX_0, UT_COUNTSEM_HANDLE);

    /* the delete woke the wait: not a timeout */
    UT_SetDefaultReturnValue(UT_KEY(OS_FreeRTOS_GateLeave), 1);
    UT_SetDeferredRetcode(UT_KEY(xSemaphoreTake), 1, -1);
    UtAssert_INT32_EQ(OS_CountSemTimedWait_Impl(&token, 250), OS_ERR_INVALID_ID);
}

void Test_OS_CountSemGetInfo_Impl(void)
{
    OS_object_token_t   token = UT_COUNTSEM_TOKEN(0);
    OS_count_sem_prop_t count_prop;

    UT_CountSemTest_SetImplId(UT_INDEX_0, UT_COUNTSEM_HANDLE);

    /* the count of this semaphore is its value */
    memset(&count_prop, 0xA5, sizeof(count_prop));
    UT_SetDefaultReturnValue(UT_KEY(uxSemaphoreGetCount), 3);
    UtAssert_INT32_EQ(OS_CountSemGetInfo_Impl(&token, &count_prop), OS_SUCCESS);
    UtAssert_INT32_EQ(count_prop.value, 3);
    UtAssert_True(UT_Stub_LastCountCountSem == UT_COUNTSEM_HANDLE, "the count of this semaphore was read");

    /* empty -> value 0 */
    UT_SetDefaultReturnValue(UT_KEY(uxSemaphoreGetCount), 0);
    UtAssert_INT32_EQ(OS_CountSemGetInfo_Impl(&token, &count_prop), OS_SUCCESS);
    UtAssert_INT32_EQ(count_prop.value, 0);

    /* the highest count there is still reads as a positive int32 */
    UT_SetDefaultReturnValue(UT_KEY(uxSemaphoreGetCount), (int32)OS_FREERTOS_SEM_VALUE_MAX);
    UtAssert_INT32_EQ(OS_CountSemGetInfo_Impl(&token, &count_prop), OS_SUCCESS);
    UtAssert_INT32_EQ(count_prop.value, 0x7FFFFFFF);

    /* the shared layer holds off a delete while this runs, so it needs no gate */
    UtAssert_STUB_COUNT(OS_FreeRTOS_GateEnter, 0);
}

/* ------------------- End of test cases --------------------------------------*/

void Osapi_Test_Setup(void)
{
    UT_ResetState(0);
    memset(UT_Ref_OS_impl_count_sem_table, 0, UT_Ref_OS_impl_count_sem_table_SIZE);
}

void Osapi_Test_Teardown(void)
{
}

#define ADD_TEST(test) UtTest_Add((Test_##test), Osapi_Test_Setup, Osapi_Test_Teardown, #test)

void UtTest_Setup(void)
{
    ADD_TEST(OS_CountSemCreate_Impl);
    ADD_TEST(OS_CountSemDelete_Impl);
    ADD_TEST(OS_CountSemDelete_Impl_Drain);
    ADD_TEST(OS_CountSemGive_Impl);
    ADD_TEST(OS_CountSemTake_Impl);
    ADD_TEST(OS_CountSemTake_Impl_Woken);
    ADD_TEST(OS_CountSemTimedWait_Impl);
    ADD_TEST(OS_CountSemTimedWait_Impl_Woken);
    ADD_TEST(OS_CountSemGetInfo_Impl);
}
