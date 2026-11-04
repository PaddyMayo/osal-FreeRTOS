/*
 * File: coveragetest-common.c
 *
 * Purpose:
 *   White-box coverage test cases for freertos/src/os-impl-common.c, run
 *   against the fake FreeRTOS semaphore API in ../fake-inc and stubbed in
 *   ../stubs/freertos-common-stubs.c - no real FreeRTOS kernel involved.
 *
 *   Each _Impl function is called directly; effects are observed via return
 *   codes, OS_SharedGlobalVars, and the fake FreeRTOS stub layer's captured
 *   call arguments and counts.
 */

#include <string.h>

#include "utassert.h"
#include "uttest.h"
#include "utstubs.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include "os-shared-common.h"
#include "os-shared-idmap.h"

#include "os-freertos.h"

#define UT_TASK_1 ((TaskHandle_t)0x7001)
#define UT_TASK_2 ((TaskHandle_t)0x7002)
#define UT_TASK_3 ((TaskHandle_t)0x7003)

/* Captured stub call arguments, declared in freertos-common-stubs.c */
extern TaskHandle_t       UT_Stub_CommonCurrentTask;
extern TaskHandle_t       UT_Stub_LastAbortedTask;
extern StaticSemaphore_t *UT_Stub_LastCreateSemBuffer;
extern SemaphoreHandle_t  UT_Stub_LastTakeSem;
extern TickType_t         UT_Stub_LastTakeTicks;
extern SemaphoreHandle_t  UT_Stub_LastGiveSem;

/*
**********************************************************************************
**          TEST CASES
**********************************************************************************
*/

void Test_OS_API_Impl_Init(void)
{
    /* any type but the first, tasks, streams and file systems -> nothing to do, nothing created */
    UtAssert_INT32_EQ(OS_API_Impl_Init(OS_OBJECT_TYPE_OS_QUEUE), OS_SUCCESS);
    UtAssert_STUB_COUNT(xSemaphoreCreateBinaryStatic, 0);
    UtAssert_STUB_COUNT(OS_FreeRTOS_TaskInit, 0);
    UtAssert_UINT32_EQ(OS_SharedGlobalVars.TicksPerSecond, 0);

    /* the task type -> the task module sets up its TCB pool, and its failure is passed on */
    UtAssert_INT32_EQ(OS_API_Impl_Init(OS_OBJECT_TYPE_OS_TASK), OS_SUCCESS);
    UtAssert_STUB_COUNT(OS_FreeRTOS_TaskInit, 1);
    UtAssert_STUB_COUNT(xSemaphoreCreateBinaryStatic, 0);

    UT_SetDeferredRetcode(UT_KEY(OS_FreeRTOS_TaskInit), 1, -1);
    UtAssert_INT32_EQ(OS_API_Impl_Init(OS_OBJECT_TYPE_OS_TASK), OS_ERROR);
    UtAssert_STUB_COUNT(OS_FreeRTOS_TaskInit, 2);

    /* the stream type -> the file module clears its handle table, and its failure is passed on */
    UtAssert_INT32_EQ(OS_API_Impl_Init(OS_OBJECT_TYPE_OS_STREAM), OS_SUCCESS);
    UtAssert_STUB_COUNT(OS_FreeRTOS_FileInit, 1);

    UT_SetDeferredRetcode(UT_KEY(OS_FreeRTOS_FileInit), 1, -1);
    UtAssert_INT32_EQ(OS_API_Impl_Init(OS_OBJECT_TYPE_OS_STREAM), OS_ERROR);
    UtAssert_STUB_COUNT(OS_FreeRTOS_FileInit, 2);

    /* the file system type -> the filesys module creates its lock, and its failure is passed on */
    UtAssert_INT32_EQ(OS_API_Impl_Init(OS_OBJECT_TYPE_OS_FILESYS), OS_SUCCESS);
    UtAssert_STUB_COUNT(OS_FreeRTOS_FileSysInit, 1);

    UT_SetDeferredRetcode(UT_KEY(OS_FreeRTOS_FileSysInit), 1, -1);
    UtAssert_INT32_EQ(OS_API_Impl_Init(OS_OBJECT_TYPE_OS_FILESYS), OS_ERROR);
    UtAssert_STUB_COUNT(OS_FreeRTOS_FileSysInit, 2);

    /* the first type -> tick rate published, idle semaphore created */
    UtAssert_INT32_EQ(OS_API_Impl_Init(OS_OBJECT_TYPE_UNDEFINED), OS_SUCCESS);
    UtAssert_STUB_COUNT(xSemaphoreCreateBinaryStatic, 1);
    UtAssert_True(UT_Stub_LastCreateSemBuffer != NULL, "idle semaphore created in a static buffer");
    UtAssert_UINT32_EQ(OS_SharedGlobalVars.TicksPerSecond, configTICK_RATE_HZ);
    UtAssert_UINT32_EQ(OS_SharedGlobalVars.MicroSecPerTick, 1000000 / configTICK_RATE_HZ);

    /* xSemaphoreCreateBinaryStatic() failure -> OS_ERROR */
    UT_SetDeferredRetcode(UT_KEY(xSemaphoreCreateBinaryStatic), 1, -1);
    UtAssert_INT32_EQ(OS_API_Impl_Init(OS_OBJECT_TYPE_UNDEFINED), OS_ERROR);
    UtAssert_STUB_COUNT(xSemaphoreCreateBinaryStatic, 2);
}

void Test_OS_IdleLoop_Impl(void)
{
    UtAssert_INT32_EQ(OS_API_Impl_Init(OS_OBJECT_TYPE_UNDEFINED), OS_SUCCESS);

    /* blocks forever on the semaphore created at init */
    OS_IdleLoop_Impl();
    UtAssert_STUB_COUNT(xSemaphoreTake, 1);
    UtAssert_True(UT_Stub_LastTakeSem == (SemaphoreHandle_t)UT_Stub_LastCreateSemBuffer,
                  "xSemaphoreTake() called with the idle semaphore");
    UtAssert_True(UT_Stub_LastTakeTicks == portMAX_DELAY, "idle loop waits with portMAX_DELAY");
}

void Test_OS_ApplicationShutdown_Impl(void)
{
    UtAssert_INT32_EQ(OS_API_Impl_Init(OS_OBJECT_TYPE_UNDEFINED), OS_SUCCESS);

    /* wakes the idle loop by giving the same semaphore */
    OS_ApplicationShutdown_Impl();
    UtAssert_STUB_COUNT(xSemaphoreGive, 1);
    UtAssert_True(UT_Stub_LastGiveSem == (SemaphoreHandle_t)UT_Stub_LastCreateSemBuffer,
                  "xSemaphoreGive() called with the idle semaphore");
}

void Test_Gate_Enter(void)
{
    OS_impl_gate_t      gate;
    OS_impl_gate_user_t first;
    OS_impl_gate_user_t second;
    OS_impl_gate_user_t refused;

    memset(&gate, 0, sizeof(gate));
    memset(&refused, 0, sizeof(refused));

    /* an open gate admits a caller and lists it under the calling task and this gate */
    UT_Stub_CommonCurrentTask = UT_TASK_1;
    UtAssert_True(OS_FreeRTOS_GateEnter(&gate, &first), "open gate admits a caller");
    UT_Stub_CommonCurrentTask = UT_TASK_2;
    UtAssert_True(OS_FreeRTOS_GateEnter(&gate, &second), "open gate admits a second caller");
    UtAssert_True(first.task == UT_TASK_1, "first user listed under its task");
    UtAssert_True(second.task == UT_TASK_2, "second user listed under its task");
    UtAssert_True(first.gate == &gate, "first user points at the gate");
    UtAssert_True(second.gate == &gate, "second user points at the gate");
    UtAssert_UINT32_EQ(OS_FreeRTOS_GateWake(&gate), 2);

    /* a closed gate refuses, and lists nothing */
    OS_FreeRTOS_GateClose(&gate);
    UtAssert_True(!OS_FreeRTOS_GateEnter(&gate, &refused), "closed gate refuses a caller");
    UtAssert_True(refused.task == NULL, "refused user node untouched");
    UtAssert_UINT32_EQ(OS_FreeRTOS_GateWake(&gate), 2);

    /* the nodes live on this stack, so they must not outlive the test */
    (void)OS_FreeRTOS_GateLeave(&gate, &first);
    (void)OS_FreeRTOS_GateLeave(&gate, &second);
    UtAssert_UINT32_EQ(OS_FreeRTOS_GateWake(&gate), 0);
}

void Test_Gate_Leave(void)
{
    OS_impl_gate_t      gate;
    OS_impl_gate_user_t first;
    OS_impl_gate_user_t second;
    OS_impl_gate_user_t third;

    memset(&gate, 0, sizeof(gate));

    /* the list is third -> second -> first; leave the newest, at the head */
    UtAssert_True(OS_FreeRTOS_GateEnter(&gate, &first), "enter first");
    UtAssert_True(OS_FreeRTOS_GateEnter(&gate, &second), "enter second");
    UtAssert_True(OS_FreeRTOS_GateEnter(&gate, &third), "enter third");
    UtAssert_True(!OS_FreeRTOS_GateLeave(&gate, &third), "leave while open reports not closed");
    UtAssert_UINT32_EQ(OS_FreeRTOS_GateWake(&gate), 2);

    /* leave the oldest, behind another user */
    UtAssert_True(!OS_FreeRTOS_GateLeave(&gate, &first), "leave behind another user");
    UtAssert_UINT32_EQ(OS_FreeRTOS_GateWake(&gate), 1);

    /* leave the last one */
    UtAssert_True(!OS_FreeRTOS_GateLeave(&gate, &second), "leave the last user");
    UtAssert_UINT32_EQ(OS_FreeRTOS_GateWake(&gate), 0);

    /* the gate closed while the caller was in -> reported, so the object is being deleted */
    UtAssert_True(OS_FreeRTOS_GateEnter(&gate, &first), "enter before the close");
    OS_FreeRTOS_GateClose(&gate);
    UtAssert_True(OS_FreeRTOS_GateLeave(&gate, &first), "leave after the close reports it");
    UtAssert_UINT32_EQ(OS_FreeRTOS_GateWake(&gate), 0);
}

void Test_Gate_OpenClose(void)
{
    OS_impl_gate_t gate;

    memset(&gate, 0, sizeof(gate));

    UtAssert_True(!OS_FreeRTOS_GateIsClosed(&gate), "a zeroed gate is open");

    OS_FreeRTOS_GateClose(&gate);
    UtAssert_True(OS_FreeRTOS_GateIsClosed(&gate), "gate closed");

    OS_FreeRTOS_GateOpen(&gate);
    UtAssert_True(!OS_FreeRTOS_GateIsClosed(&gate), "gate open again");
}

void Test_Gate_Wake(void)
{
    OS_impl_gate_t      gate_a;
    OS_impl_gate_t      gate_b;
    OS_impl_gate_user_t a1;
    OS_impl_gate_user_t a2;
    OS_impl_gate_user_t b1;

    memset(&gate_a, 0, sizeof(gate_a));
    memset(&gate_b, 0, sizeof(gate_b));

    /* nobody in -> nothing to wake */
    UtAssert_UINT32_EQ(OS_FreeRTOS_GateWake(&gate_a), 0);
    UtAssert_STUB_COUNT(xTaskAbortDelay, 0);

    UT_Stub_CommonCurrentTask = UT_TASK_1;
    UtAssert_True(OS_FreeRTOS_GateEnter(&gate_a, &a1), "enter gate A");
    UT_Stub_CommonCurrentTask = UT_TASK_2;
    UtAssert_True(OS_FreeRTOS_GateEnter(&gate_a, &a2), "enter gate A again");
    UT_Stub_CommonCurrentTask = UT_TASK_3;
    UtAssert_True(OS_FreeRTOS_GateEnter(&gate_b, &b1), "enter gate B");

    /* only the tasks in a call on THIS gate are woken, and its users counted */
    UtAssert_UINT32_EQ(OS_FreeRTOS_GateWake(&gate_a), 2);
    UtAssert_STUB_COUNT(xTaskAbortDelay, 2);
    UtAssert_True(UT_Stub_LastAbortedTask == UT_TASK_1, "the oldest user of the gate is woken too");

    UtAssert_UINT32_EQ(OS_FreeRTOS_GateWake(&gate_b), 1);
    UtAssert_STUB_COUNT(xTaskAbortDelay, 3);
    UtAssert_True(UT_Stub_LastAbortedTask == UT_TASK_3, "gate B's user is woken");

    (void)OS_FreeRTOS_GateLeave(&gate_a, &a1);
    (void)OS_FreeRTOS_GateLeave(&gate_a, &a2);
    (void)OS_FreeRTOS_GateLeave(&gate_b, &b1);
}

void Test_Gate_ReleaseTask(void)
{
    OS_impl_gate_t      gate_a;
    OS_impl_gate_t      gate_b;
    OS_impl_gate_user_t t1_a;
    OS_impl_gate_user_t t2_a;
    OS_impl_gate_user_t t1_b;

    memset(&gate_a, 0, sizeof(gate_a));
    memset(&gate_b, 0, sizeof(gate_b));

    /* the list is t1_b -> t2_a -> t1_a: task 1 has a node at the head and at the end, task 2 one between */
    UT_Stub_CommonCurrentTask = UT_TASK_1;
    UtAssert_True(OS_FreeRTOS_GateEnter(&gate_a, &t1_a), "task 1 enters gate A");
    UT_Stub_CommonCurrentTask = UT_TASK_2;
    UtAssert_True(OS_FreeRTOS_GateEnter(&gate_a, &t2_a), "task 2 enters gate A");
    UT_Stub_CommonCurrentTask = UT_TASK_1;
    UtAssert_True(OS_FreeRTOS_GateEnter(&gate_b, &t1_b), "task 1 enters gate B");

    OS_FreeRTOS_GateReleaseTask(UT_TASK_1);

    /* task 1's nodes are gone from both gates, task 2's stays */
    UtAssert_UINT32_EQ(OS_FreeRTOS_GateWake(&gate_a), 1);
    UtAssert_UINT32_EQ(OS_FreeRTOS_GateWake(&gate_b), 0);

    /* a task with no nodes -> nothing changes */
    OS_FreeRTOS_GateReleaseTask(UT_TASK_3);
    UtAssert_UINT32_EQ(OS_FreeRTOS_GateWake(&gate_a), 1);

    (void)OS_FreeRTOS_GateLeave(&gate_a, &t2_a);
    UtAssert_UINT32_EQ(OS_FreeRTOS_GateWake(&gate_a), 0);
}

/*
 * Setup function prior to every test
 */
void Osapi_Test_Setup(void)
{
    UT_ResetState(0);
    memset(&OS_SharedGlobalVars, 0, sizeof(OS_SharedGlobalVars));
    UT_Stub_LastCreateSemBuffer = NULL;
    UT_Stub_LastTakeSem         = NULL;
    UT_Stub_LastTakeTicks       = 0;
    UT_Stub_LastGiveSem         = NULL;
    UT_Stub_CommonCurrentTask   = (TaskHandle_t)0x7000;
    UT_Stub_LastAbortedTask     = NULL;
}

void Osapi_Test_Teardown(void)
{
}

#define ADD_TEST(test) UtTest_Add((Test_##test), Osapi_Test_Setup, Osapi_Test_Teardown, #test)

void UtTest_Setup(void)
{
    ADD_TEST(OS_API_Impl_Init);
    ADD_TEST(OS_IdleLoop_Impl);
    ADD_TEST(OS_ApplicationShutdown_Impl);
    ADD_TEST(Gate_Enter);
    ADD_TEST(Gate_Leave);
    ADD_TEST(Gate_OpenClose);
    ADD_TEST(Gate_Wake);
    ADD_TEST(Gate_ReleaseTask);
}
