/*
 * File: coveragetest-tasks.c
 *
 * Purpose:
 *   White-box coverage test cases for freertos/src/os-impl-tasks.c, run
 *   against the fake FreeRTOS task API in ../fake-inc and stubbed in
 *   ../stubs/freertos-task-stubs.c - no real FreeRTOS kernel involved.
 *   (The black box tests for this port are OSAL's own /src/tests and
 *   /src/unit-tests, which link the real OSAL library instead.)
 *
 *   Each _Impl function is called directly; preconditions that its public
 *   API cannot reach are set up through ../adaptors/inc/ut-adaptor-tasks.h,
 *   and effects are observed via return codes plus the fake FreeRTOS stub
 *   layer's captured call arguments and counts.
 *
 *   OS_FreeRTOSTaskEntry (the static trampoline FreeRTOS invokes when it
 *   actually runs a created task) is reached by calling the function
 *   pointer OS_TaskCreate_Impl hands to the xTaskCreateStatic() stub, since
 *   there is no running scheduler here to do it. OS_FreeRTOS_PriorityRemap
 *   (the other helper in that file) is covered indirectly through
 *   OS_TaskCreate_Impl and OS_TaskSetPriority_Impl, since both call it
 *   inline.
 */

#include <stdlib.h>
#include <string.h>

#include "utassert.h"
#include "uttest.h"
#include "utstubs.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "os-shared-task.h"
#include "os-shared-idmap.h"

#include "ut-adaptor-tasks.h"

/* Captured stub call arguments, declared in freertos-task-stubs.c */
extern TaskHandle_t           UT_Stub_CurrentTaskHandle;
extern TaskHandle_t           UT_Stub_LastDeletedHandle;
extern TaskHandle_t           UT_Stub_LastPriorityHandle;
extern UBaseType_t            UT_Stub_LastPriorityValue;
extern TickType_t             UT_Stub_LastDelayTicks;
extern TaskFunction_t         UT_Stub_LastCreateTaskCode;
extern void                  *UT_Stub_LastCreateParameters;
extern const char            *UT_Stub_LastCreateName;
extern UBaseType_t            UT_Stub_LastCreatePriority;
extern configSTACK_DEPTH_TYPE UT_Stub_LastCreateStackDepth;

extern void UT_Stub_ResetTLS(void);

/* The TCB pool's semaphore, declared in freertos-task-stubs.c */
extern StaticSemaphore_t *UT_Stub_LastCountingSemBuffer;
extern UBaseType_t        UT_Stub_LastCountingSemMax;
extern UBaseType_t        UT_Stub_LastCountingSemInitial;
extern TickType_t         UT_Stub_LastTakeTicks;

/* Captured OS_TaskEntryPoint() argument, declared in osal-shared-stubs.c */
extern osal_id_t UT_Stub_LastEntryPointId;

/* OS_ObjectIdFinalizeDelete() call record, declared in osal-shared-stubs.c */
extern uint32 UT_Stub_FinalizeDeleteCount;
extern int32  UT_Stub_LastFinalizeDeleteStatus;
extern uint32 UT_Stub_LastFinalizeDeleteIndex;

extern void  OS_FreeRTOS_TaskCleanup(const void *tcb);
extern int32 OS_FreeRTOS_TaskInit(void);

/* Delete hook for the self-delete test: records how often it ran and whether the slot was released first */
static uint32 UT_DeleteHookCalls;
static uint32 UT_DeleteHookSawFinalizeCount;

static void UT_DeleteHook(void)
{
    ++UT_DeleteHookCalls;
    UT_DeleteHookSawFinalizeCount = UT_Stub_FinalizeDeleteCount;
}

#define UT_TASK_TOKEN(idx)                                                                              \
    (OS_object_token_t)                                                                                 \
    {                                                                                                   \
        .obj_type = OS_OBJECT_TYPE_OS_TASK, .obj_id = (osal_id_t) { 0x10000 + (idx) }, .obj_idx = (idx) \
    }

#define UT_INDEX_0 OSAL_INDEX_C(0)

#define UT_OBJID_1 ((osal_id_t) { 1 })
#define UT_OBJID_2 ((osal_id_t) { 2 })

/*
**********************************************************************************
**          TEST CASES
**********************************************************************************
*/

void Test_OS_TaskMatch_Impl(void)
{
    OS_object_token_t token = UT_TASK_TOKEN(0);

    /* current task handle matches the table entry -> OS_SUCCESS */
    UT_TaskTest_SetImplTaskId(UT_INDEX_0, UT_Stub_CurrentTaskHandle);
    UtAssert_INT32_EQ(OS_TaskMatch_Impl(&token), OS_SUCCESS);

    /* current task handle does not match -> OS_ERROR */
    UT_TaskTest_SetImplTaskId(UT_INDEX_0, (TaskHandle_t)0x9999);
    UtAssert_INT32_EQ(OS_TaskMatch_Impl(&token), OS_ERROR);
}

void Test_OS_TaskCreate_Impl(void)
{
    OS_object_token_t token = UT_TASK_TOKEN(0);

    strncpy(OS_task_table[0].task_name, "UnitTest", sizeof(OS_task_table[0].task_name) - 1);
    OS_task_table[0].stack_size = sizeof(StackType_t) * 128;
    OS_task_table[0].priority   = 50;

    /* OSAL_TASK_STACK_ALLOCATE -> waits for a pool entry, then pvPortMalloc(); stack owned by the port */
    OS_task_table[0].stack_pointer = OSAL_TASK_STACK_ALLOCATE;
    UtAssert_INT32_EQ(OS_TaskCreate_Impl(&token, 0), OS_SUCCESS);
    UtAssert_STUB_COUNT(xSemaphoreTake, 1);
    UtAssert_True(UT_Stub_LastTakeTicks == UT_TaskTest_GetTcbWaitTicks(),
                  "waits for a free pool entry, but only for a bounded time");
    UtAssert_True(UT_TaskTest_GetPoolInUse(0), "first pool entry claimed");
    UtAssert_StrCmp(UT_Stub_LastCreateName, "UnitTest", "task name passed through");
    UtAssert_UINT32_EQ(UT_Stub_LastCreateStackDepth, 128);
    UtAssert_STUB_COUNT(pvPortMalloc, 1);
    UtAssert_STUB_COUNT(xTaskCreateStatic, 1);
    UtAssert_NOT_NULL(UT_TaskTest_GetPoolStack(0));
    UtAssert_True(UT_TaskTest_GetImplTaskId(UT_INDEX_0) == UT_TaskTest_GetPoolTcb(0),
                  "the task is built on the pool entry's TCB");

    /* the table slot is handed out again before the kernel has cleaned up its
     * previous task (the entry is still held) -> succeeds, on the next entry */
    UtAssert_INT32_EQ(OS_TaskCreate_Impl(&token, 0), OS_SUCCESS);
    UtAssert_STUB_COUNT(xSemaphoreTake, 2);
    UtAssert_True(UT_TaskTest_GetPoolInUse(1), "second pool entry claimed");
    UtAssert_STUB_COUNT(pvPortMalloc, 2);
    UtAssert_STUB_COUNT(xTaskCreateStatic, 2);
    UtAssert_True(UT_TaskTest_GetImplTaskId(UT_INDEX_0) == UT_TaskTest_GetPoolTcb(1),
                  "the new task is built on its own TCB");

    /* kernel cleanup of each frees its stack and returns its entry */
    OS_FreeRTOS_TaskCleanup(UT_TaskTest_GetPoolTcb(0));
    UtAssert_STUB_COUNT(vPortFree, 1);
    UtAssert_STUB_COUNT(xSemaphoreGive, 1);
    OS_FreeRTOS_TaskCleanup(UT_TaskTest_GetPoolTcb(1));
    UtAssert_STUB_COUNT(vPortFree, 2);
    UtAssert_STUB_COUNT(xSemaphoreGive, 2);

    /* caller-supplied stack pointer -> no allocation, success, stack not owned by the port */
    OS_task_table[0].stack_pointer = (osal_stackptr_t)0x2000;
    UtAssert_INT32_EQ(OS_TaskCreate_Impl(&token, 0), OS_SUCCESS);
    UtAssert_STUB_COUNT(pvPortMalloc, 2);
    UtAssert_STUB_COUNT(xTaskCreateStatic, 3);
    UtAssert_NULL(UT_TaskTest_GetPoolStack(0));

    OS_FreeRTOS_TaskCleanup(UT_TaskTest_GetPoolTcb(0));
    UtAssert_STUB_COUNT(vPortFree, 2);
    UtAssert_STUB_COUNT(xSemaphoreGive, 3);

    /* pvPortMalloc() failure -> OS_ERROR, no xTaskCreateStatic() call, entry handed back */
    OS_task_table[0].stack_pointer = OSAL_TASK_STACK_ALLOCATE;
    UT_SetDeferredRetcode(UT_KEY(pvPortMalloc), 1, -1);
    UtAssert_INT32_EQ(OS_TaskCreate_Impl(&token, 0), OS_ERROR);
    UtAssert_STUB_COUNT(pvPortMalloc, 3);
    UtAssert_STUB_COUNT(xTaskCreateStatic, 3);
    UtAssert_True(!UT_TaskTest_GetPoolInUse(0), "entry released after an allocation failure");
    UtAssert_STUB_COUNT(xSemaphoreGive, 4);

    /* xTaskCreateStatic() failure with allocated stack -> OS_ERROR, stack freed, entry handed back */
    OS_task_table[0].stack_pointer = OSAL_TASK_STACK_ALLOCATE;
    UT_SetDeferredRetcode(UT_KEY(xTaskCreateStatic), 1, -1);
    UtAssert_INT32_EQ(OS_TaskCreate_Impl(&token, 0), OS_ERROR);
    UtAssert_STUB_COUNT(vPortFree, 3);
    UtAssert_NULL(UT_TaskTest_GetPoolStack(0));
    UtAssert_True(!UT_TaskTest_GetPoolInUse(0), "entry released after a create failure");
    UtAssert_STUB_COUNT(xSemaphoreGive, 5);

    /* xTaskCreateStatic() failure with caller-supplied stack -> OS_ERROR, no
     * additional free (vPortFree count is cumulative across this function) */
    OS_task_table[0].stack_pointer = (osal_stackptr_t)0x2000;
    UT_SetDeferredRetcode(UT_KEY(xTaskCreateStatic), 1, -1);
    UtAssert_INT32_EQ(OS_TaskCreate_Impl(&token, 0), OS_ERROR);
    UtAssert_STUB_COUNT(vPortFree, 3);
    UtAssert_STUB_COUNT(xSemaphoreGive, 6);

    /* the semaphore says an entry is free but none is (cannot happen) -> OS_ERROR, nothing created,
     * and the count is given back */
    UT_TaskTest_SetAllPoolInUse();
    UtAssert_INT32_EQ(OS_TaskCreate_Impl(&token, 0), OS_ERROR);
    UtAssert_STUB_COUNT(xTaskCreateStatic, 5);
    UtAssert_STUB_COUNT(xSemaphoreGive, 7);

    /* no entry comes free in time (the idle task is kept off the CPU) -> OS_ERR_TRY_AGAIN, nothing
     * claimed, created or given back */
    UT_SetDeferredRetcode(UT_KEY(xSemaphoreTake), 1, -1);
    UtAssert_INT32_EQ(OS_TaskCreate_Impl(&token, 0), OS_ERR_TRY_AGAIN);
    UtAssert_STUB_COUNT(xTaskCreateStatic, 5);
    UtAssert_STUB_COUNT(xSemaphoreGive, 7);
}

void Test_OS_FreeRTOS_TaskInit(void)
{
    /* one pool entry per task the table can hold, all free to begin with */
    UtAssert_INT32_EQ(OS_FreeRTOS_TaskInit(), OS_SUCCESS);
    UtAssert_STUB_COUNT(xSemaphoreCreateCountingStatic, 1);
    UtAssert_NOT_NULL(UT_Stub_LastCountingSemBuffer);
    UtAssert_UINT32_EQ(UT_Stub_LastCountingSemMax, OS_MAX_TASKS);
    UtAssert_UINT32_EQ(UT_Stub_LastCountingSemInitial, OS_MAX_TASKS);

    /* xSemaphoreCreateCountingStatic() failure -> OS_ERROR */
    UT_SetDeferredRetcode(UT_KEY(xSemaphoreCreateCountingStatic), 1, -1);
    UtAssert_INT32_EQ(OS_FreeRTOS_TaskInit(), OS_ERROR);
    UtAssert_STUB_COUNT(xSemaphoreCreateCountingStatic, 2);
}

void Test_OS_FreeRTOSTaskEntry(void)
{
    OS_object_token_t token = UT_TASK_TOKEN(0);

    OS_task_table[0].stack_size    = sizeof(StackType_t) * 128;
    OS_task_table[0].stack_pointer = (osal_stackptr_t)0x2000;
    UtAssert_INT32_EQ(OS_TaskCreate_Impl(&token, 0), OS_SUCCESS);
    UtAssert_NOT_NULL(UT_Stub_LastCreateTaskCode);

    /* run the trampoline as the scheduler would -> OSAL task id unwrapped */
    UT_Stub_LastEntryPointId = OS_OBJECT_ID_UNDEFINED;
    UT_Stub_LastCreateTaskCode(UT_Stub_LastCreateParameters);
    UtAssert_True(OS_ObjectIdEqual(UT_Stub_LastEntryPointId, token.obj_id), "OS_TaskEntryPoint() called with task id");
}

void Test_OS_TaskDetach_Impl(void)
{
    OS_object_token_t token = UT_TASK_TOKEN(0);

    UtAssert_INT32_EQ(OS_TaskDetach_Impl(&token), OS_SUCCESS);
}

void Test_OS_TaskDelete_Impl(void)
{
    OS_object_token_t token = UT_TASK_TOKEN(0);

    UT_TaskTest_SetImplTaskId(UT_INDEX_0, (TaskHandle_t)0x4242);

    /* another task: the kernel frees it right away; the stack still goes through the portCLEAN_UP_TCB hook, not here */
    UtAssert_INT32_EQ(OS_TaskDelete_Impl(&token), OS_SUCCESS);
    UtAssert_True(UT_Stub_LastDeletedHandle == (TaskHandle_t)0x4242, "vTaskDelete() called with task handle");
    UtAssert_NULL(UT_TaskTest_GetImplTaskId(UT_INDEX_0));
    UtAssert_STUB_COUNT(vPortFree, 0);
    UtAssert_UINT32_EQ(UT_Stub_FinalizeDeleteCount, 0);

    /* the calling task: vTaskDelete() will not return, so OS_TaskDelete() never would either.
     * The slot is released first, then the delete hook runs (unlocked), then the task goes. */
    OS_task_table[0].delete_hook_pointer = UT_DeleteHook;
    UT_TaskTest_SetImplTaskId(UT_INDEX_0, UT_Stub_CurrentTaskHandle);
    UT_Stub_LastDeletedHandle = NULL;
    UtAssert_INT32_EQ(OS_TaskDelete_Impl(&token), OS_SUCCESS);
    UtAssert_UINT32_EQ(UT_Stub_FinalizeDeleteCount, 1);
    UtAssert_INT32_EQ(UT_Stub_LastFinalizeDeleteStatus, OS_SUCCESS);
    UtAssert_UINT32_EQ(UT_Stub_LastFinalizeDeleteIndex, UT_INDEX_0);
    UtAssert_UINT32_EQ(UT_DeleteHookCalls, 1);
    UtAssert_UINT32_EQ(UT_DeleteHookSawFinalizeCount, 1);
    UtAssert_True(UT_Stub_LastDeletedHandle == UT_Stub_CurrentTaskHandle, "vTaskDelete() called for the calling task");
    UtAssert_NULL(UT_TaskTest_GetImplTaskId(UT_INDEX_0));

    /* the calling task, no delete hook registered -> none called */
    OS_task_table[0].delete_hook_pointer = NULL;
    UT_TaskTest_SetImplTaskId(UT_INDEX_0, UT_Stub_CurrentTaskHandle);
    UtAssert_INT32_EQ(OS_TaskDelete_Impl(&token), OS_SUCCESS);
    UtAssert_UINT32_EQ(UT_Stub_FinalizeDeleteCount, 2);
    UtAssert_UINT32_EQ(UT_DeleteHookCalls, 1);
}

void Test_OS_FreeRTOS_TaskCleanup(void)
{
    StaticTask_t other_tcb;

    memset(&other_tcb, 0, sizeof(other_tcb));

    /* port-allocated stack -> freed, entry released and the semaphore given */
    UT_TaskTest_SetPoolInUse(0, true);
    UT_TaskTest_SetPoolStack(0, malloc(sizeof(StackType_t) * 16));
    OS_FreeRTOS_TaskCleanup(UT_TaskTest_GetPoolTcb(0));
    UtAssert_STUB_COUNT(vPortFree, 1);
    UtAssert_NULL(UT_TaskTest_GetPoolStack(0));
    UtAssert_True(!UT_TaskTest_GetPoolInUse(0), "entry released");
    UtAssert_STUB_COUNT(xSemaphoreGive, 1);

    /* caller-supplied stack -> nothing freed, entry still released */
    UT_TaskTest_SetPoolInUse(1, true);
    OS_FreeRTOS_TaskCleanup(UT_TaskTest_GetPoolTcb(1));
    UtAssert_STUB_COUNT(vPortFree, 1);
    UtAssert_True(!UT_TaskTest_GetPoolInUse(1), "entry released");
    UtAssert_STUB_COUNT(xSemaphoreGive, 2);

    /* the last entry in the pool is found too */
    UT_TaskTest_SetPoolInUse(OS_MAX_TASKS - 1, true);
    OS_FreeRTOS_TaskCleanup(UT_TaskTest_GetPoolTcb(OS_MAX_TASKS - 1));
    UtAssert_True(!UT_TaskTest_GetPoolInUse(OS_MAX_TASKS - 1), "last entry released");
    UtAssert_STUB_COUNT(xSemaphoreGive, 3);

    /* the task table slot is left alone: OSAL freed it when the task was deleted,
     * and it may belong to a newer task by now */
    UT_TaskTest_SetImplTaskId(UT_INDEX_0, (TaskHandle_t)0x4242);
    UT_TaskTest_SetPoolInUse(0, true);
    OS_FreeRTOS_TaskCleanup(UT_TaskTest_GetPoolTcb(0));
    UtAssert_ADDRESS_EQ(UT_TaskTest_GetImplTaskId(UT_INDEX_0), 0x4242);

    /* TCB not in the pool (e.g. the idle task) -> nothing touched */
    UT_TaskTest_SetPoolInUse(0, true);
    UT_TaskTest_SetPoolStack(0, (StackType_t *)0x3000);
    OS_FreeRTOS_TaskCleanup(&other_tcb);
    UtAssert_STUB_COUNT(vPortFree, 1);
    UtAssert_STUB_COUNT(xSemaphoreGive, 4);
    UtAssert_True(UT_TaskTest_GetPoolInUse(0), "entry still in use");
    UtAssert_ADDRESS_EQ(UT_TaskTest_GetPoolStack(0), 0x3000);
}

void Test_OS_TaskExit_Impl(void)
{
    UT_Stub_LastDeletedHandle = (TaskHandle_t)0x1;

    OS_TaskExit_Impl();

    UtAssert_True(UT_Stub_LastDeletedHandle == NULL, "vTaskDelete(NULL) called for calling task");
}

void Test_OS_TaskDelay_Impl(void)
{
    /* the fake kernel runs at 100 Hz: 10 ms per tick */

    /* zero stays zero: a yield, not a sleep */
    UtAssert_INT32_EQ(OS_TaskDelay_Impl(0), OS_SUCCESS);
    UtAssert_UINT32_EQ(UT_Stub_LastDelayTicks, 0);

    /* shorter than a tick must still sleep, not truncate to a yield */
    UtAssert_INT32_EQ(OS_TaskDelay_Impl(1), OS_SUCCESS);
    UtAssert_UINT32_EQ(UT_Stub_LastDelayTicks, 1);

    /* not a whole number of ticks -> rounded up, never short */
    UtAssert_INT32_EQ(OS_TaskDelay_Impl(15), OS_SUCCESS);
    UtAssert_UINT32_EQ(UT_Stub_LastDelayTicks, 2);

    UtAssert_INT32_EQ(OS_TaskDelay_Impl(250), OS_SUCCESS);
    UtAssert_UINT32_EQ(UT_Stub_LastDelayTicks, 26);
}

void Test_OS_TaskSetPriority_Impl(void)
{
    OS_object_token_t token = UT_TASK_TOKEN(0);

    UT_TaskTest_SetImplTaskId(UT_INDEX_0, (TaskHandle_t)0x55);

    /* OSAL priority 0 (highest) -> FreeRTOS priority configMAX_PRIORITIES-1 (highest) */
    UtAssert_INT32_EQ(OS_TaskSetPriority_Impl(&token, 0), OS_SUCCESS);
    UtAssert_True(UT_Stub_LastPriorityHandle == (TaskHandle_t)0x55, "vTaskPrioritySet() called with task handle");
    UtAssert_UINT32_EQ(UT_Stub_LastPriorityValue, configMAX_PRIORITIES - 1);

    /* OSAL priority OS_MAX_TASK_PRIORITY (lowest) -> FreeRTOS priority 0 (lowest) */
    UtAssert_INT32_EQ(OS_TaskSetPriority_Impl(&token, OS_MAX_TASK_PRIORITY), OS_SUCCESS);
    UtAssert_UINT32_EQ(UT_Stub_LastPriorityValue, 0);
}

void Test_OS_TaskGetId_Impl(void)
{
    /* nothing registered yet -> undefined (zero) id */
    UtAssert_True(!OS_ObjectIdDefined(OS_TaskGetId_Impl()), "OS_TaskGetId_Impl() undefined before registration");

    UtAssert_INT32_EQ(OS_TaskRegister_Impl(UT_OBJID_1), OS_SUCCESS);
    UtAssert_True(OS_ObjectIdEqual(OS_TaskGetId_Impl(), UT_OBJID_1), "OS_TaskGetId_Impl() returns registered id");
}

void Test_OS_TaskGetInfo_Impl(void)
{
    OS_object_token_t token = UT_TASK_TOKEN(0);
    OS_task_prop_t    task_prop;

    memset(&task_prop, 0, sizeof(task_prop));

    UtAssert_INT32_EQ(OS_TaskGetInfo_Impl(&token, &task_prop), OS_SUCCESS);
}

void Test_OS_TaskRegister_Impl(void)
{
    UtAssert_INT32_EQ(OS_TaskRegister_Impl(UT_OBJID_2), OS_SUCCESS);
    UtAssert_True(OS_ObjectIdEqual(OS_TaskGetId_Impl(), UT_OBJID_2), "round-trips through TLS storage");
}

void Test_OS_TaskIdMatchSystemData_Impl(void)
{
    OS_object_token_t  token = UT_TASK_TOKEN(0);
    OS_common_record_t record;
    TaskHandle_t       target = (TaskHandle_t)0x77;

    UT_TaskTest_SetImplTaskId(UT_INDEX_0, target);
    UtAssert_True(OS_TaskIdMatchSystemData_Impl(&target, &token, &record), "matching handle returns true");

    target = (TaskHandle_t)0x88;
    UtAssert_True(!OS_TaskIdMatchSystemData_Impl(&target, &token, &record), "non-matching handle returns false");
}

void Test_OS_TaskValidateSystemData_Impl(void)
{
    TaskHandle_t dummy = (TaskHandle_t)0x99;

    UtAssert_INT32_EQ(OS_TaskValidateSystemData_Impl(NULL, sizeof(dummy)), OS_INVALID_POINTER);
    UtAssert_INT32_EQ(OS_TaskValidateSystemData_Impl(&dummy, sizeof(dummy) + 1), OS_INVALID_POINTER);
    UtAssert_INT32_EQ(OS_TaskValidateSystemData_Impl(&dummy, sizeof(dummy)), OS_SUCCESS);
}

/* ------------------- End of test cases --------------------------------------*/

void Osapi_Test_Setup(void)
{
    UT_ResetState(0);
    memset(OS_task_table, 0, sizeof(OS_task_table));
    memset(UT_Ref_OS_impl_task_table, 0, UT_Ref_OS_impl_task_table_SIZE);
    memset(UT_Ref_OS_impl_task_tcb_pool, 0, UT_Ref_OS_impl_task_tcb_pool_SIZE);
    UT_Stub_ResetTLS();

    UT_Stub_FinalizeDeleteCount      = 0;
    UT_Stub_LastFinalizeDeleteStatus = 0;
    UT_Stub_LastFinalizeDeleteIndex  = 0;
    UT_DeleteHookCalls               = 0;
    UT_DeleteHookSawFinalizeCount    = 0;
}

void Osapi_Test_Teardown(void)
{
}

#define ADD_TEST(test) UtTest_Add((Test_##test), Osapi_Test_Setup, Osapi_Test_Teardown, #test)

void UtTest_Setup(void)
{
    ADD_TEST(OS_TaskMatch_Impl);
    ADD_TEST(OS_TaskCreate_Impl);
    ADD_TEST(OS_FreeRTOS_TaskInit);
    ADD_TEST(OS_FreeRTOSTaskEntry);
    ADD_TEST(OS_TaskDetach_Impl);
    ADD_TEST(OS_TaskDelete_Impl);
    ADD_TEST(OS_FreeRTOS_TaskCleanup);
    ADD_TEST(OS_TaskExit_Impl);
    ADD_TEST(OS_TaskDelay_Impl);
    ADD_TEST(OS_TaskSetPriority_Impl);
    ADD_TEST(OS_TaskGetId_Impl);
    ADD_TEST(OS_TaskGetInfo_Impl);
    ADD_TEST(OS_TaskRegister_Impl);
    ADD_TEST(OS_TaskIdMatchSystemData_Impl);
    ADD_TEST(OS_TaskValidateSystemData_Impl);
}
