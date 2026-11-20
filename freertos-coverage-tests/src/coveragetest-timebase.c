/*
 * File: coveragetest-timebase.c
 *
 * Purpose:
 *   White-box coverage test cases for freertos/src/os-impl-timebase.c, run
 *   against the fake FreeRTOS task API in ../fake-inc and stubbed in
 *   ../stubs/freertos-timebase-stubs.c - no real FreeRTOS kernel involved.
 *   (The black box tests for this port are OSAL's own /src/tests and
 *   /src/unit-tests, which link the real OSAL library instead.)
 *
 *   Each _Impl function is called directly; preconditions that its public
 *   API cannot reach are set up through ../adaptors/inc/ut-adaptor-timebase.h,
 *   and effects are observed via return codes plus the fake FreeRTOS stub
 *   layer's captured call arguments and counts. The servicing task cannot be
 *   run here, so its entry point, and the sync function the time base gets,
 *   are reached through the pointers that OS_TimeBaseCreate_Impl() hands out.
 */

#include <string.h>

#include "utassert.h"
#include "uttest.h"
#include "utstubs.h"

#include "FreeRTOS.h"
#include "task.h"

#include "osapi-task.h"

#include "os-shared-timebase.h"
#include "os-shared-common.h"
#include "os-shared-idmap.h"

#include "ut-adaptor-timebase.h"
#include "os-impl-timebase.h"

/* Captured stub call arguments and scripted values, declared in freertos-timebase-stubs.c */
extern TaskFunction_t         UT_Stub_LastCreateTaskCode;
extern const char            *UT_Stub_LastCreateName;
extern configSTACK_DEPTH_TYPE UT_Stub_LastCreateStackDepth;
extern void                  *UT_Stub_LastCreateParameters;
extern UBaseType_t            UT_Stub_LastCreatePriority;
extern StackType_t           *UT_Stub_LastCreateStackBuffer;
extern StaticTask_t          *UT_Stub_LastCreateTaskBuffer;
extern TaskHandle_t           UT_Stub_LastDeletedHandle;
extern TickType_t             UT_Stub_TickCount;
extern UBaseType_t            UT_Stub_LastNotifyTakeIndex;
extern BaseType_t             UT_Stub_LastNotifyTakeClear;
extern TickType_t             UT_Stub_LastNotifyTakeTicks;
extern osal_priority_t        UT_Stub_LastRemapInput;
extern UBaseType_t            UT_Stub_RemapResult;
extern TaskHandle_t           UT_Stub_LastNotifiedTask;
extern UBaseType_t            UT_Stub_LastNotifyGiveIndex;

#define UT_TIMEBASE_TOKEN(idx)                                                                                   \
    (OS_object_token_t)                                                                                          \
    {                                                                                                            \
        .obj_type = OS_OBJECT_TYPE_OS_TIMEBASE,                                                                  \
        .obj_id = (osal_id_t) { (OS_OBJECT_TYPE_OS_TIMEBASE << OS_OBJECT_TYPE_SHIFT) + (idx) }, .obj_idx = (idx) \
    }

#define UT_INDEX_1 OSAL_INDEX_C(1)

#define UT_TICK_START  500
#define UT_US_PER_TICK 10000

/* A sync function of the BSP's */
static uint32 UT_BspSync(osal_id_t timebase_id)
{
    (void)timebase_id;

    return 7;
}

/* Inside OS_ObjectIdGetById(): looks the time base up as the one in slot 1 */
static int32 UT_GetByIdFindsSlot1(void *UserObj, int32 StubRetcode, uint32 CallCount, const UT_StubContext_t *Context)
{
    (void)UserObj;
    (void)CallCount;

    if (StubRetcode == OS_SUCCESS)
    {
        OS_object_token_t *token = UT_Hook_GetArgValueByName(Context, "token", OS_object_token_t *);

        token->obj_idx = UT_INDEX_1;
    }

    return StubRetcode;
}

/* Inside the wait on the notification: the time moves on to this, as it does while the task sleeps */
static TickType_t UT_TickAfterSleep;

static int32 UT_SleepMovesTimeOn(void *UserObj, int32 StubRetcode, uint32 CallCount, const UT_StubContext_t *Context)
{
    (void)UserObj;
    (void)CallCount;
    (void)Context;

    UT_Stub_TickCount = UT_TickAfterSleep;

    return StubRetcode;
}

/* Inside OS_TimeBase_CallbackThread(): the id it was run for */
static osal_id_t UT_CallbackThreadId;

static int32
UT_CallbackThreadSeesId(void *UserObj, int32 StubRetcode, uint32 CallCount, const UT_StubContext_t *Context)
{
    (void)UserObj;
    (void)CallCount;

    UT_CallbackThreadId = UT_Hook_GetArgValueByName(Context, "timebase_id", osal_id_t);

    return StubRetcode;
}

/*
**********************************************************************************
**          TEST CASES
**********************************************************************************
*/

void Test_OS_TimeBaseCreate_Impl(void)
{
    OS_object_token_t token = UT_TIMEBASE_TOKEN(1);

    strncpy(OS_timebase_table[1].timebase_name, "UnitTestBase", sizeof(OS_timebase_table[1].timebase_name) - 1);

    /* no BSP sync function: the tick is simulated, so the time base is given the one in the port */
    OS_timebase_table[1].external_sync = NULL;
    UT_Stub_TickCount                  = UT_TICK_START;
    UtAssert_INT32_EQ(OS_TimeBaseCreate_Impl(&token), OS_SUCCESS);
    UtAssert_NOT_NULL(OS_timebase_table[1].external_sync);
    UtAssert_True(OS_timebase_table[1].external_sync != UT_BspSync, "the port's own sync function is set");

    /* the servicing task is built on this slot's static TCB and stack, named for the time base, at the
     * priority OSAL's highest priority remaps to, and is handed its id */
    UtAssert_STUB_COUNT(xTaskCreateStatic, 1);
    UtAssert_NOT_NULL(UT_Stub_LastCreateTaskCode);
    UtAssert_StrCmp(UT_Stub_LastCreateName, "UnitTestBase", "task named for the time base");
    UtAssert_UINT32_EQ(UT_Stub_LastCreateStackDepth, OS_FREERTOS_TIMEBASE_STACK_WORDS);
    UtAssert_True(UT_Stub_LastCreateStackBuffer == UT_TimeBaseTest_GetImplStack(UT_INDEX_1), "this slot's stack");
    UtAssert_True((void *)UT_Stub_LastCreateTaskBuffer == UT_TimeBaseTest_GetImplTask(UT_INDEX_1), "this slot's TCB");
    UtAssert_UINT32_EQ(UT_Stub_LastRemapInput, 0);
    UtAssert_UINT32_EQ(UT_Stub_LastCreatePriority, UT_Stub_RemapResult);
    UtAssert_True(UT_TimeBaseTest_GetImplHandle(UT_INDEX_1) == (TaskHandle_t)UT_Stub_LastCreateTaskBuffer,
                  "the task handle is kept");
    UtAssert_UINT32_EQ(UT_TimeBaseTest_GetImplLastTick(UT_INDEX_1), UT_TICK_START);

    /* a BSP sync function is kept, and still gets its task */
    OS_timebase_table[1].external_sync = UT_BspSync;
    UtAssert_INT32_EQ(OS_TimeBaseCreate_Impl(&token), OS_SUCCESS);
    UtAssert_True(OS_timebase_table[1].external_sync == UT_BspSync, "the BSP's sync function is kept");
    UtAssert_STUB_COUNT(xTaskCreateStatic, 2);

    /* the task cannot be created -> OS_TIMER_ERR_UNAVAILABLE, and the port's sync function is not left behind */
    OS_timebase_table[1].external_sync = NULL;
    UT_SetDeferredRetcode(UT_KEY(xTaskCreateStatic), 1, -1);
    UtAssert_INT32_EQ(OS_TimeBaseCreate_Impl(&token), OS_TIMER_ERR_UNAVAILABLE);
    UtAssert_NULL(OS_timebase_table[1].external_sync);
    UtAssert_STUB_COUNT(xTaskCreateStatic, 3);

    /* the same with a BSP sync function: it is the BSP's, so it stays */
    OS_timebase_table[1].external_sync = UT_BspSync;
    UT_SetDeferredRetcode(UT_KEY(xTaskCreateStatic), 1, -1);
    UtAssert_INT32_EQ(OS_TimeBaseCreate_Impl(&token), OS_TIMER_ERR_UNAVAILABLE);
    UtAssert_True(OS_timebase_table[1].external_sync == UT_BspSync, "the BSP's sync function is kept");
}

void Test_OS_FreeRTOS_TimeBaseTaskEntry(void)
{
    OS_object_token_t token = UT_TIMEBASE_TOKEN(1);

    UtAssert_INT32_EQ(OS_TimeBaseCreate_Impl(&token), OS_SUCCESS);

    /* the task runs the shared layer's loop for its own time base; when that returns, the task ends itself */
    UT_SetHookFunction(UT_KEY(OS_TimeBase_CallbackThread), UT_CallbackThreadSeesId, NULL);
    UT_Stub_LastDeletedHandle = (TaskHandle_t)0x1;
    UT_Stub_LastCreateTaskCode(UT_Stub_LastCreateParameters);
    UtAssert_STUB_COUNT(OS_TimeBase_CallbackThread, 1);
    UtAssert_True(OS_ObjectIdEqual(UT_CallbackThreadId, token.obj_id), "the loop is run for this time base");
    UtAssert_STUB_COUNT(vTaskDelete, 1);
    UtAssert_NULL(UT_Stub_LastDeletedHandle);
}

void Test_OS_FreeRTOS_TimeBaseWait(void)
{
    OS_object_token_t token = UT_TIMEBASE_TOKEN(1);
    OS_TimerSync_t    wait;

    UT_Stub_TickCount                   = UT_TICK_START;
    OS_SharedGlobalVars.MicroSecPerTick = UT_US_PER_TICK;
    OS_timebase_table[1].external_sync  = NULL;
    UtAssert_INT32_EQ(OS_TimeBaseCreate_Impl(&token), OS_SUCCESS);
    wait = OS_timebase_table[1].external_sync;
    UT_SetHookFunction(UT_KEY(OS_ObjectIdGetById), UT_GetByIdFindsSlot1, NULL);

    /* nothing is armed: it sleeps for good, on its own notification index, and clears the count so a wake is taken
     * once. The wait ending with the count at zero is not a tick, but the three ticks that went by are counted,
     * in microseconds, from the last time it returned. */
    UT_Stub_TickCount = UT_TICK_START + 3;
    UtAssert_UINT32_EQ(wait(token.obj_id), 3 * UT_US_PER_TICK);
    UtAssert_UINT32_EQ(UT_Stub_LastNotifyTakeIndex, OS_FREERTOS_TIMEBASE_NOTIFY_INDEX);
    UtAssert_True(UT_Stub_LastNotifyTakeClear == pdTRUE, "the count is cleared, so a wake is taken once");
    UtAssert_True(UT_Stub_LastNotifyTakeTicks == portMAX_DELAY, "nothing is armed: it waits for good");
    UtAssert_UINT32_EQ(UT_TimeBaseTest_GetImplLastTick(UT_INDEX_1), UT_TICK_START + 3);
    UtAssert_True(!UT_TimeBaseTest_GetImplArmed(UT_INDEX_1), "nothing gets armed by a wake");

    /* woken by a notification (the count is not zero) -> zero, which the caller takes as "call again", and
     * the time is left to be counted by the next return */
    UT_SetDeferredRetcode(UT_KEY(ulTaskNotifyTakeIndexed), 1, 1);
    UT_Stub_TickCount = UT_TICK_START + 5;
    UtAssert_UINT32_EQ(wait(token.obj_id), 0);
    UtAssert_UINT32_EQ(UT_TimeBaseTest_GetImplLastTick(UT_INDEX_1), UT_TICK_START + 3);
    UtAssert_STUB_COUNT(ulTaskNotifyTakeIndexed, 2);

    /* the time base is gone -> zero, without waiting */
    UT_SetDeferredRetcode(UT_KEY(OS_ObjectIdGetById), 1, -1);
    UtAssert_UINT32_EQ(wait(token.obj_id), 0);
    UtAssert_STUB_COUNT(ulTaskNotifyTakeIndexed, 2);
}

void Test_OS_FreeRTOS_TimeBaseWait_Armed(void)
{
    OS_object_token_t token = UT_TIMEBASE_TOKEN(1);
    OS_TimerSync_t    wait;

    UT_Stub_TickCount                   = UT_TICK_START;
    OS_SharedGlobalVars.MicroSecPerTick = UT_US_PER_TICK;
    OS_timebase_table[1].external_sync  = NULL;
    UtAssert_INT32_EQ(OS_TimeBaseCreate_Impl(&token), OS_SUCCESS);
    wait = OS_timebase_table[1].external_sync;
    UT_SetHookFunction(UT_KEY(OS_ObjectIdGetById), UT_GetByIdFindsSlot1, NULL);
    UT_SetHookFunction(UT_KEY(ulTaskNotifyTakeIndexed), UT_SleepMovesTimeOn, NULL);

    /* a periodic tick due in three ticks: it sleeps for those, no more, and when it wakes with no count
     * the tick is due, and counted from the time it last returned. The next is an interval after the one
     * that was due. */
    UT_TimeBaseTest_SetImplArm(UT_INDEX_1, true, UT_TICK_START + 3, 2);
    UT_TickAfterSleep = UT_TICK_START + 3;
    UtAssert_UINT32_EQ(wait(token.obj_id), 3 * UT_US_PER_TICK);
    UtAssert_UINT32_EQ(UT_Stub_LastNotifyTakeTicks, 3);
    UtAssert_UINT32_EQ(UT_TimeBaseTest_GetImplLastTick(UT_INDEX_1), UT_TICK_START + 3);
    UtAssert_True(UT_TimeBaseTest_GetImplArmed(UT_INDEX_1), "a periodic tick stays armed");
    UtAssert_UINT32_EQ(UT_TimeBaseTest_GetImplDeadline(UT_INDEX_1), UT_TICK_START + 5);

    /* the tick is due on this very tick -> no sleep, and the deadline moves on all the same */
    UT_TimeBaseTest_SetImplArm(UT_INDEX_1, true, UT_TICK_START + 3, 2);
    UT_TickAfterSleep = UT_TICK_START + 3;
    UtAssert_UINT32_EQ(wait(token.obj_id), 0);
    UtAssert_UINT32_EQ(UT_Stub_LastNotifyTakeTicks, 0);
    UtAssert_UINT32_EQ(UT_TimeBaseTest_GetImplDeadline(UT_INDEX_1), UT_TICK_START + 5);

    /* it is late: the deadline is behind, so it does not sleep, and the missed ticks are not made up: the
     * next one is the first interval boundary after now, so the ticks keep their rhythm. The time that went by
     * is all in the count. */
    UT_TimeBaseTest_SetImplArm(UT_INDEX_1, true, UT_TICK_START, 2);
    UT_TickAfterSleep = UT_TICK_START + 8;
    UT_Stub_TickCount = UT_TICK_START + 8;
    UtAssert_UINT32_EQ(wait(token.obj_id), 5 * UT_US_PER_TICK);
    UtAssert_UINT32_EQ(UT_Stub_LastNotifyTakeTicks, 0);
    UtAssert_UINT32_EQ(UT_TimeBaseTest_GetImplDeadline(UT_INDEX_1), UT_TICK_START + 8 + 2);
    UtAssert_UINT32_EQ(UT_TimeBaseTest_GetImplLastTick(UT_INDEX_1), UT_TICK_START + 8);

    /* a one-shot is disarmed once it has come */
    UT_TimeBaseTest_SetImplArm(UT_INDEX_1, true, UT_TICK_START + 12, 0);
    UT_TickAfterSleep = UT_TICK_START + 12;
    UtAssert_UINT32_EQ(wait(token.obj_id), 4 * UT_US_PER_TICK);
    UtAssert_UINT32_EQ(UT_Stub_LastNotifyTakeTicks, 4);
    UtAssert_True(!UT_TimeBaseTest_GetImplArmed(UT_INDEX_1), "a one-shot is spent");

    /* the time ran out, but a Set has armed a different tick in the meantime, which is not due: it is left
     * alone, and the time is still counted */
    UT_TimeBaseTest_SetImplArm(UT_INDEX_1, true, UT_TICK_START + 14, 3);
    UT_TickAfterSleep = UT_TICK_START + 13;
    UtAssert_UINT32_EQ(wait(token.obj_id), 1 * UT_US_PER_TICK);
    UtAssert_UINT32_EQ(UT_TimeBaseTest_GetImplDeadline(UT_INDEX_1), UT_TICK_START + 14);
    UtAssert_True(UT_TimeBaseTest_GetImplArmed(UT_INDEX_1), "the new tick is still armed");
}

void Test_OS_TimeBaseSet_Impl(void)
{
    OS_object_token_t token = UT_TIMEBASE_TOKEN(1);

    UT_Stub_TickCount                   = UT_TICK_START;
    OS_SharedGlobalVars.MicroSecPerTick = UT_US_PER_TICK;
    OS_timebase_table[1].external_sync  = NULL;
    UtAssert_INT32_EQ(OS_TimeBaseCreate_Impl(&token), OS_SUCCESS);

    /* a periodic tick: the first is three ticks away, then one tick apart, counted from now. The accuracy is
     * the period, and the task is woken to look at it. */
    UT_Stub_TickCount = UT_TICK_START + 2;
    UtAssert_INT32_EQ(OS_TimeBaseSet_Impl(&token, 3 * UT_US_PER_TICK, UT_US_PER_TICK), OS_SUCCESS);
    UtAssert_True(UT_TimeBaseTest_GetImplArmed(UT_INDEX_1), "armed");
    UtAssert_UINT32_EQ(UT_TimeBaseTest_GetImplDeadline(UT_INDEX_1), UT_TICK_START + 2 + 3);
    UtAssert_UINT32_EQ(UT_TimeBaseTest_GetImplInterval(UT_INDEX_1), 1);
    UtAssert_UINT32_EQ(UT_TimeBaseTest_GetImplLastTick(UT_INDEX_1), UT_TICK_START + 2);
    UtAssert_UINT32_EQ(OS_timebase_table[1].accuracy_usec, UT_US_PER_TICK);
    UtAssert_STUB_COUNT(xTaskNotifyGiveIndexed, 1);
    UtAssert_True(UT_Stub_LastNotifiedTask == UT_TimeBaseTest_GetImplHandle(UT_INDEX_1), "the servicing task is woken");
    UtAssert_UINT32_EQ(UT_Stub_LastNotifyGiveIndex, OS_FREERTOS_TIMEBASE_NOTIFY_INDEX);
    UtAssert_STUB_COUNT(vTaskEnterCritical, 1);
    UtAssert_STUB_COUNT(vTaskExitCritical, 1);

    /* a one-shot, of a time that is not a whole number of ticks: rounded up, never short, and the accuracy
     * is that time, as the one tick that comes */
    UtAssert_INT32_EQ(OS_TimeBaseSet_Impl(&token, 25000, 0), OS_SUCCESS);
    UtAssert_True(UT_TimeBaseTest_GetImplArmed(UT_INDEX_1), "armed");
    UtAssert_UINT32_EQ(UT_TimeBaseTest_GetImplDeadline(UT_INDEX_1), UT_TICK_START + 2 + 3);
    UtAssert_UINT32_EQ(UT_TimeBaseTest_GetImplInterval(UT_INDEX_1), 0);
    UtAssert_UINT32_EQ(OS_timebase_table[1].accuracy_usec, 3 * UT_US_PER_TICK);

    /* less than a tick is a tick, not none */
    UtAssert_INT32_EQ(OS_TimeBaseSet_Impl(&token, 1, 1), OS_SUCCESS);
    UtAssert_UINT32_EQ(UT_TimeBaseTest_GetImplDeadline(UT_INDEX_1), UT_TICK_START + 2 + 1);
    UtAssert_UINT32_EQ(UT_TimeBaseTest_GetImplInterval(UT_INDEX_1), 1);

    /* a start time of zero disarms it, whatever the interval, and leaves the accuracy as it was */
    UtAssert_INT32_EQ(OS_TimeBaseSet_Impl(&token, 0, 0), OS_SUCCESS);
    UtAssert_True(!UT_TimeBaseTest_GetImplArmed(UT_INDEX_1), "disarmed");
    UtAssert_UINT32_EQ(OS_timebase_table[1].accuracy_usec, UT_US_PER_TICK);
    UtAssert_INT32_EQ(OS_TimeBaseSet_Impl(&token, 3 * UT_US_PER_TICK, 0), OS_SUCCESS);
    UtAssert_INT32_EQ(OS_TimeBaseSet_Impl(&token, 0, 5 * UT_US_PER_TICK), OS_SUCCESS);
    UtAssert_True(!UT_TimeBaseTest_GetImplArmed(UT_INDEX_1), "disarmed with an interval too");
    UtAssert_UINT32_EQ(OS_timebase_table[1].accuracy_usec, 3 * UT_US_PER_TICK);

    /* every one of them woke the task, so it looks again */
    UtAssert_STUB_COUNT(xTaskNotifyGiveIndexed, 6);

    /* a BSP sync function brings its own tick: nothing is armed, and nothing is woken */
    UT_TimeBaseTest_SetImplArm(UT_INDEX_1, false, 0, 0);
    OS_timebase_table[1].external_sync = UT_BspSync;
    UtAssert_INT32_EQ(OS_TimeBaseSet_Impl(&token, 3 * UT_US_PER_TICK, UT_US_PER_TICK), OS_SUCCESS);
    UtAssert_True(!UT_TimeBaseTest_GetImplArmed(UT_INDEX_1), "nothing is armed for the BSP's tick");
    UtAssert_STUB_COUNT(xTaskNotifyGiveIndexed, 6);
}

void Test_OS_TimeBaseNotImplemented(void)
{
    OS_object_token_t  token = UT_TIMEBASE_TOKEN(1);
    OS_timebase_prop_t prop;

    /* not implemented yet: these only report that, or do nothing */
    UtAssert_INT32_EQ(OS_TimeBaseDelete_Impl(&token), OS_ERR_NOT_IMPLEMENTED);
    UtAssert_VOIDCALL(OS_TimeBaseLock_Impl(&token));
    UtAssert_VOIDCALL(OS_TimeBaseUnlock_Impl(&token));

    /* there is nothing OS-specific to report */
    UtAssert_INT32_EQ(OS_TimeBaseGetInfo_Impl(&token, &prop), OS_SUCCESS);
}

/* ------------------- End of test cases --------------------------------------*/

void Osapi_Test_Setup(void)
{
    UT_ResetState(0);
    memset(OS_timebase_table, 0, sizeof(OS_timebase_table));
    memset(UT_Ref_OS_impl_timebase_table, 0, UT_Ref_OS_impl_timebase_table_SIZE);
    memset(&OS_SharedGlobalVars, 0, sizeof(OS_SharedGlobalVars));

    UT_Stub_TickCount   = 0;
    UT_Stub_RemapResult = 6;
    UT_TickAfterSleep   = 0;
}

void Osapi_Test_Teardown(void)
{
}

#define ADD_TEST(test) UtTest_Add((Test_##test), Osapi_Test_Setup, Osapi_Test_Teardown, #test)

void UtTest_Setup(void)
{
    ADD_TEST(OS_TimeBaseCreate_Impl);
    ADD_TEST(OS_FreeRTOS_TimeBaseTaskEntry);
    ADD_TEST(OS_FreeRTOS_TimeBaseWait);
    ADD_TEST(OS_FreeRTOS_TimeBaseWait_Armed);
    ADD_TEST(OS_TimeBaseSet_Impl);
    ADD_TEST(OS_TimeBaseNotImplemented);
}
