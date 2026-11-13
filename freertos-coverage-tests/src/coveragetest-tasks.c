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
#include "timers.h"

#include "os-shared-task.h"
#include "os-shared-idmap.h"

#include "ut-adaptor-tasks.h"
#include "os-freertos.h"

/* Captured stub call arguments, declared in freertos-task-stubs.c */
extern TaskHandle_t           UT_Stub_CurrentTaskHandle;
extern TaskHandle_t           UT_Stub_LastDeletedHandle;
extern TaskHandle_t           UT_Stub_LastSuspendedHandle;
extern PendedFunction_t       UT_Stub_LastPendedFunction;
extern void                  *UT_Stub_LastPendedParameter;
extern TaskHandle_t           UT_Stub_LastPriorityHandle;
extern UBaseType_t            UT_Stub_LastPriorityValue;
extern TickType_t             UT_Stub_LastDelayTicks;
extern TaskFunction_t         UT_Stub_LastCreateTaskCode;
extern void                  *UT_Stub_LastCreateParameters;
extern const char            *UT_Stub_LastCreateName;
extern UBaseType_t            UT_Stub_LastCreatePriority;
extern configSTACK_DEPTH_TYPE UT_Stub_LastCreateStackDepth;

extern void UT_Stub_ResetTLS(void);

/* The port's task death notice and checkpoint hook, stand-ins in freertos-task-stubs.c */
extern TaskHandle_t                 UT_Stub_LastDiedTask;
extern uint32                       UT_Stub_DiedCountAtPend;
extern OS_FreeRTOS_CheckpointHook_t UT_Stub_CheckpointHook;

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

    /* the port's checkpoint is given the hook that ends a task that deleted itself: it has the primitives
     * drop what they keep for the calling task, hands it to the timer task and suspends it until it has gone */
    UtAssert_STUB_COUNT(OS_FreeRTOS_SetCheckpointHook, 1);
    UtAssert_True(UT_Stub_CheckpointHook != NULL, "a checkpoint hook is set");
    UT_Stub_CheckpointHook();
    UtAssert_STUB_COUNT(OS_FreeRTOS_TaskDied, 1);
    UtAssert_True(UT_Stub_LastDiedTask == UT_Stub_CurrentTaskHandle, "the primitives are told the calling task died");
    UtAssert_UINT32_EQ(UT_Stub_DiedCountAtPend, 1);
    UtAssert_STUB_COUNT(xTimerPendFunctionCall, 1);
    UtAssert_True(UT_Stub_LastPendedParameter == UT_Stub_CurrentTaskHandle, "the calling task is handed over");
    UtAssert_STUB_COUNT(vTaskSuspend, 1);

    /* the timer task runs the handed function: the task is deleted from outside */
    UT_Stub_LastDeletedHandle = NULL;
    UT_Stub_LastPendedFunction(UT_Stub_LastPendedParameter, 0);
    UtAssert_True(UT_Stub_LastDeletedHandle == UT_Stub_CurrentTaskHandle, "the timer task deletes the calling task");

    /* xSemaphoreCreateCountingStatic() failure -> OS_ERROR, and the hook is not set again */
    UT_SetDeferredRetcode(UT_KEY(xSemaphoreCreateCountingStatic), 1, -1);
    UtAssert_INT32_EQ(OS_FreeRTOS_TaskInit(), OS_ERROR);
    UtAssert_STUB_COUNT(xSemaphoreCreateCountingStatic, 2);
    UtAssert_STUB_COUNT(OS_FreeRTOS_SetCheckpointHook, 1);
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

/* Inside vTaskDelete(): how far the delete of another task had got, to check the order */
static uint32 UT_SuspendsAtDelete;
static uint32 UT_DiedAtDelete;

static int32 UT_DeleteSeesOrder(void *UserObj, int32 StubRetcode, uint32 CallCount, const UT_StubContext_t *Context)
{
    (void)UserObj;
    (void)CallCount;
    (void)Context;

    UT_SuspendsAtDelete = UT_GetStubCount(UT_KEY(vTaskSuspend));
    UT_DiedAtDelete     = UT_GetStubCount(UT_KEY(OS_FreeRTOS_TaskDied));

    return StubRetcode;
}

void Test_OS_TaskDelete_Impl(void)
{
    OS_object_token_t token = UT_TASK_TOKEN(0);

    UT_TaskTest_SetImplTaskId(UT_INDEX_0, (TaskHandle_t)0x4242);

    /* another task: it is frozen, what the primitives keep for it is dropped, and only then does the kernel
     * free it right away; the stack still goes through the portCLEAN_UP_TCB hook, not here */
    UT_SetHookFunction(UT_KEY(vTaskDelete), UT_DeleteSeesOrder, NULL);
    UtAssert_INT32_EQ(OS_TaskDelete_Impl(&token), OS_SUCCESS);
    UtAssert_True(UT_Stub_LastSuspendedHandle == (TaskHandle_t)0x4242, "the task is suspended first");
    UtAssert_True(UT_Stub_LastDiedTask == (TaskHandle_t)0x4242, "the primitives are told it died");
    UtAssert_UINT32_EQ(UT_SuspendsAtDelete, 1);
    UtAssert_UINT32_EQ(UT_DiedAtDelete, 1);
    UtAssert_True(UT_Stub_LastDeletedHandle == (TaskHandle_t)0x4242, "vTaskDelete() called with task handle");
    UtAssert_NULL(UT_TaskTest_GetImplTaskId(UT_INDEX_0));
    UtAssert_STUB_COUNT(vPortFree, 0);
    UtAssert_UINT32_EQ(UT_Stub_FinalizeDeleteCount, 0);
    UtAssert_NULL(pvTaskGetThreadLocalStoragePointer(NULL, OS_FREERTOS_TASK_DOOMED_TLS_INDEX));

    /* the calling task: it cannot be ended from inside its own delete, as OS_TaskDelete() must get
     * back to release the slot and call the delete hook. It is only marked, for the port's checkpoint
     * to find at its next wait, and the call returns. */
    OS_task_table[0].delete_hook_pointer = UT_DeleteHook;
    UT_Stub_CurrentTaskHandle            = UT_TaskTest_GetPoolTcb(1);
    UT_TaskTest_SetImplTaskId(UT_INDEX_0, UT_Stub_CurrentTaskHandle);
    UtAssert_INT32_EQ(OS_TaskDelete_Impl(&token), OS_SUCCESS);
    UtAssert_NULL(UT_TaskTest_GetImplTaskId(UT_INDEX_0));
    UtAssert_True(pvTaskGetThreadLocalStoragePointer(NULL, OS_FREERTOS_TASK_DOOMED_TLS_INDEX) != NULL,
                  "the task is marked");
    UtAssert_UINT32_EQ(UT_Stub_FinalizeDeleteCount, 0);
    UtAssert_UINT32_EQ(UT_DeleteHookCalls, 0);
    UtAssert_STUB_COUNT(vTaskSuspend, 1);
    UtAssert_STUB_COUNT(vTaskDelete, 1);
    UtAssert_STUB_COUNT(xTimerPendFunctionCall, 0);
    UtAssert_STUB_COUNT(OS_FreeRTOS_TaskDied, 1);
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
    /* the calling task is handed to the timer task and suspends until it has gone */
    UT_Stub_LastDeletedHandle   = (TaskHandle_t)0x1;
    UT_Stub_LastSuspendedHandle = (TaskHandle_t)0x1;

    OS_TaskExit_Impl();

    UtAssert_STUB_COUNT(OS_FreeRTOS_TaskDied, 1);
    UtAssert_True(UT_Stub_LastDiedTask == UT_Stub_CurrentTaskHandle, "the primitives are told the calling task died");
    UtAssert_UINT32_EQ(UT_Stub_DiedCountAtPend, 1);
    UtAssert_STUB_COUNT(xTimerPendFunctionCall, 1);
    UtAssert_True(UT_Stub_LastPendedParameter == UT_Stub_CurrentTaskHandle, "the calling task is handed over");
    UtAssert_STUB_COUNT(vTaskSuspend, 1);
    UtAssert_NULL(UT_Stub_LastSuspendedHandle);

    /* the timer task runs the handed function: the task is deleted from outside */
    UT_Stub_LastDeletedHandle = NULL;
    UT_Stub_LastPendedFunction(UT_Stub_LastPendedParameter, 0);
    UtAssert_True(UT_Stub_LastDeletedHandle == UT_Stub_CurrentTaskHandle, "the timer task deletes the calling task");

    /* the timer queue is full -> no suspend, the task deletes itself and the idle task frees it */
    UT_SetDeferredRetcode(UT_KEY(xTimerPendFunctionCall), 1, -1);
    UT_Stub_LastDeletedHandle = (TaskHandle_t)0x1;
    OS_TaskExit_Impl();
    UtAssert_STUB_COUNT(xTimerPendFunctionCall, 2);
    UtAssert_STUB_COUNT(OS_FreeRTOS_TaskDied, 2);
    UtAssert_STUB_COUNT(vTaskSuspend, 1);
    UtAssert_NULL(UT_Stub_LastDeletedHandle);
}

/* Inside OS_FreeRTOS_Checkpoint(): how many delays had been made by its first pass, to check the order */
static uint32 UT_DelaysAtFirstCheckpoint;

static int32 UT_CheckpointSeesOrder(void *UserObj, int32 StubRetcode, uint32 CallCount, const UT_StubContext_t *Context)
{
    (void)UserObj;
    (void)Context;

    /* ut_assert counts from zero here */
    if (CallCount == 0)
    {
        UT_DelaysAtFirstCheckpoint = UT_GetStubCount(UT_KEY(vTaskDelay));
    }

    return StubRetcode;
}

void Test_OS_TaskDelay_Impl(void)
{
    /* the fake kernel runs at 100 Hz: 10 ms per tick */
    UT_DelaysAtFirstCheckpoint = 99;
    UT_SetHookFunction(UT_KEY(OS_FreeRTOS_Checkpoint), UT_CheckpointSeesOrder, NULL);

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

    /* a delay is a wait into the port: every one passed the port's checkpoint, before it slept */
    UtAssert_STUB_COUNT(OS_FreeRTOS_Checkpoint, 4);
    UtAssert_UINT32_EQ(UT_DelaysAtFirstCheckpoint, 0);
}

void Test_OS_TaskSetPriority_Impl(void)
{
    OS_object_token_t token = UT_TASK_TOKEN(0);

    UT_TaskTest_SetImplTaskId(UT_INDEX_0, (TaskHandle_t)0x55);

    /* OSAL priority 0 (highest) -> FreeRTOS priority OSAL_FREERTOS_MAX_TASK_PRIORITY (highest) */
    UtAssert_INT32_EQ(OS_TaskSetPriority_Impl(&token, 0), OS_SUCCESS);
    UtAssert_True(UT_Stub_LastPriorityHandle == (TaskHandle_t)0x55, "vTaskPrioritySet() called with task handle");
    UtAssert_UINT32_EQ(UT_Stub_LastPriorityValue, OSAL_FREERTOS_MAX_TASK_PRIORITY);

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

    UT_Stub_CurrentTaskHandle        = (TaskHandle_t)0x1000;
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
