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
 *   OS_FreeRTOSTaskEntry (the trampoline FreeRTOS would invoke when it
 *   actually runs a created task) is intentionally not exercised here: it
 *   only ever executes under a running scheduler, which this stub-based
 *   environment does not simulate. OS_PriorityRemap (the other static
 *   helper in that file) is covered indirectly through OS_TaskCreate_Impl
 *   and OS_TaskSetPriority_Impl, since both call it inline.
 */

#include <string.h>

#include "utassert.h"
#include "uttest.h"
#include "utstubs.h"

#include "FreeRTOS.h"
#include "task.h"

#include "os-shared-task.h"
#include "os-shared-idmap.h"

#include "ut-adaptor-tasks.h"

/* Captured stub call arguments, declared in freertos-task-stubs.c */
extern TaskHandle_t           UT_Stub_CurrentTaskHandle;
extern TaskHandle_t           UT_Stub_LastDeletedHandle;
extern TaskHandle_t           UT_Stub_LastPriorityHandle;
extern UBaseType_t            UT_Stub_LastPriorityValue;
extern TickType_t             UT_Stub_LastDelayTicks;
extern const char            *UT_Stub_LastCreateName;
extern UBaseType_t            UT_Stub_LastCreatePriority;
extern configSTACK_DEPTH_TYPE UT_Stub_LastCreateStackDepth;

extern void UT_Stub_ResetTLS(void);

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

    /* OSAL_TASK_STACK_ALLOCATE -> pvPortMalloc() path, success */
    OS_task_table[0].stack_pointer = OSAL_TASK_STACK_ALLOCATE;
    UtAssert_INT32_EQ(OS_TaskCreate_Impl(&token, 0), OS_SUCCESS);
    UtAssert_StrCmp(UT_Stub_LastCreateName, "UnitTest", "task name passed through");
    UtAssert_UINT32_EQ(UT_Stub_LastCreateStackDepth, 128);
    UtAssert_STUB_COUNT(pvPortMalloc, 1);
    UtAssert_STUB_COUNT(xTaskCreateStatic, 1);

    /* caller-supplied stack pointer -> no allocation, success */
    OS_task_table[0].stack_pointer = (osal_stackptr_t)0x2000;
    UtAssert_INT32_EQ(OS_TaskCreate_Impl(&token, 0), OS_SUCCESS);
    UtAssert_STUB_COUNT(pvPortMalloc, 1); /* unchanged - no allocation this time */
    UtAssert_STUB_COUNT(xTaskCreateStatic, 2);

    /* pvPortMalloc() failure -> OS_ERROR, no xTaskCreateStatic() call */
    OS_task_table[0].stack_pointer = OSAL_TASK_STACK_ALLOCATE;
    UT_SetDeferredRetcode(UT_KEY(pvPortMalloc), 1, -1);
    UtAssert_INT32_EQ(OS_TaskCreate_Impl(&token, 0), OS_ERROR);
    UtAssert_STUB_COUNT(pvPortMalloc, 2);
    UtAssert_STUB_COUNT(xTaskCreateStatic, 2); /* unchanged - short-circuited */

    /* xTaskCreateStatic() failure with allocated stack -> OS_ERROR, freed */
    OS_task_table[0].stack_pointer = OSAL_TASK_STACK_ALLOCATE;
    UT_SetDeferredRetcode(UT_KEY(xTaskCreateStatic), 1, -1);
    UtAssert_INT32_EQ(OS_TaskCreate_Impl(&token, 0), OS_ERROR);
    UtAssert_STUB_COUNT(vPortFree, 1);

    /* xTaskCreateStatic() failure with caller-supplied stack -> OS_ERROR, no
     * additional free (count stays at 1 from the case above - it is
     * cumulative across this whole test function, not reset per scenario) */
    OS_task_table[0].stack_pointer = (osal_stackptr_t)0x2000;
    UT_SetDeferredRetcode(UT_KEY(xTaskCreateStatic), 1, -1);
    UtAssert_INT32_EQ(OS_TaskCreate_Impl(&token, 0), OS_ERROR);
    UtAssert_STUB_COUNT(vPortFree, 1);
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

    UtAssert_INT32_EQ(OS_TaskDelete_Impl(&token), OS_SUCCESS);
    UtAssert_True(UT_Stub_LastDeletedHandle == (TaskHandle_t)0x4242, "vTaskDelete() called with task handle");
}

void Test_OS_TaskExit_Impl(void)
{
    UT_Stub_LastDeletedHandle = (TaskHandle_t)0x1;

    OS_TaskExit_Impl();

    UtAssert_True(UT_Stub_LastDeletedHandle == NULL, "vTaskDelete(NULL) called for calling task");
}

void Test_OS_TaskDelay_Impl(void)
{
    UtAssert_INT32_EQ(OS_TaskDelay_Impl(250), OS_SUCCESS);
    UtAssert_UINT32_EQ(UT_Stub_LastDelayTicks, 250);
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
    UT_Stub_ResetTLS();
}

void Osapi_Test_Teardown(void)
{
}

#define ADD_TEST(test) UtTest_Add((Test_##test), Osapi_Test_Setup, Osapi_Test_Teardown, #test)

void UtTest_Setup(void)
{
    ADD_TEST(OS_TaskMatch_Impl);
    ADD_TEST(OS_TaskCreate_Impl);
    ADD_TEST(OS_TaskDetach_Impl);
    ADD_TEST(OS_TaskDelete_Impl);
    ADD_TEST(OS_TaskExit_Impl);
    ADD_TEST(OS_TaskDelay_Impl);
    ADD_TEST(OS_TaskSetPriority_Impl);
    ADD_TEST(OS_TaskGetId_Impl);
    ADD_TEST(OS_TaskGetInfo_Impl);
    ADD_TEST(OS_TaskRegister_Impl);
    ADD_TEST(OS_TaskIdMatchSystemData_Impl);
    ADD_TEST(OS_TaskValidateSystemData_Impl);
}
