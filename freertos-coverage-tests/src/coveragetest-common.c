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

/* Captured stub call arguments, declared in freertos-common-stubs.c */
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
    /* any type but the first -> nothing to do, nothing created */
    UtAssert_INT32_EQ(OS_API_Impl_Init(OS_OBJECT_TYPE_OS_TASK), OS_SUCCESS);
    UtAssert_STUB_COUNT(xSemaphoreCreateBinaryStatic, 0);
    UtAssert_UINT32_EQ(OS_SharedGlobalVars.TicksPerSecond, 0);

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
}
