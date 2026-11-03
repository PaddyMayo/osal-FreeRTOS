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
 */

#include <string.h>

#include "utassert.h"
#include "uttest.h"
#include "utstubs.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include "os-shared-mutex.h"
#include "os-shared-idmap.h"

#include "ut-adaptor-mutex.h"

/* Captured stub call arguments, declared in freertos-mutex-stubs.c */
extern SemaphoreHandle_t UT_Stub_CreatedMutexHandle;
extern SemaphoreHandle_t UT_Stub_LastDeletedMutex;
extern SemaphoreHandle_t UT_Stub_LastTakeMutex;
extern TickType_t        UT_Stub_LastTakeMutexTicks;
extern SemaphoreHandle_t UT_Stub_LastGiveMutex;

#define UT_MUTEX_TOKEN(idx)                                                                              \
    (OS_object_token_t)                                                                                  \
    {                                                                                                    \
        .obj_type = OS_OBJECT_TYPE_OS_MUTEX, .obj_id = (osal_id_t) { 0x30000 + (idx) }, .obj_idx = (idx) \
    }

#define UT_INDEX_0 OSAL_INDEX_C(0)

#define UT_MUTEX_HANDLE ((SemaphoreHandle_t)0x4242)

/*
**********************************************************************************
**          TEST CASES
**********************************************************************************
*/

void Test_OS_MutSemCreate_Impl(void)
{
    OS_object_token_t token = UT_MUTEX_TOKEN(0);

    /* success -> handle stored in the impl table */
    UtAssert_INT32_EQ(OS_MutSemCreate_Impl(&token, 0), OS_SUCCESS);
    UtAssert_STUB_COUNT(xSemaphoreCreateRecursiveMutex, 1);
    UtAssert_True(UT_MutexTest_GetImplMutexId(UT_INDEX_0) == UT_Stub_CreatedMutexHandle,
                  "xSemaphoreCreateRecursiveMutex() handle stored in impl table");

    /* xSemaphoreCreateRecursiveMutex() failure (e.g. out of heap) -> OS_SEM_FAILURE, no handle kept */
    UT_SetDeferredRetcode(UT_KEY(xSemaphoreCreateRecursiveMutex), 1, -1);
    UtAssert_INT32_EQ(OS_MutSemCreate_Impl(&token, 0), OS_SEM_FAILURE);
    UtAssert_STUB_COUNT(xSemaphoreCreateRecursiveMutex, 2);
    UtAssert_True(UT_MutexTest_GetImplMutexId(UT_INDEX_0) == NULL,
                  "no handle stored when xSemaphoreCreateRecursiveMutex() fails");
}

void Test_OS_MutSemDelete_Impl(void)
{
    OS_object_token_t token = UT_MUTEX_TOKEN(0);

    UT_MutexTest_SetImplMutexId(UT_INDEX_0, UT_MUTEX_HANDLE);

    UtAssert_INT32_EQ(OS_MutSemDelete_Impl(&token), OS_SUCCESS);
    UtAssert_STUB_COUNT(vSemaphoreDelete, 1);
    UtAssert_True(UT_Stub_LastDeletedMutex == UT_MUTEX_HANDLE, "vSemaphoreDelete() called with mutex handle");
}

void Test_OS_MutSemGive_Impl(void)
{
    OS_object_token_t token = UT_MUTEX_TOKEN(0);

    UT_MutexTest_SetImplMutexId(UT_INDEX_0, UT_MUTEX_HANDLE);

    UtAssert_INT32_EQ(OS_MutSemGive_Impl(&token), OS_SUCCESS);
    UtAssert_STUB_COUNT(xSemaphoreGiveRecursive, 1);
    UtAssert_True(UT_Stub_LastGiveMutex == UT_MUTEX_HANDLE, "xSemaphoreGiveRecursive() called with mutex handle");

    /* the caller is not the holder -> OS_SEM_FAILURE */
    UT_SetDeferredRetcode(UT_KEY(xSemaphoreGiveRecursive), 1, -1);
    UtAssert_INT32_EQ(OS_MutSemGive_Impl(&token), OS_SEM_FAILURE);
    UtAssert_STUB_COUNT(xSemaphoreGiveRecursive, 2);
}

void Test_OS_MutSemTake_Impl(void)
{
    OS_object_token_t token = UT_MUTEX_TOKEN(0);

    UT_MutexTest_SetImplMutexId(UT_INDEX_0, UT_MUTEX_HANDLE);

    /* success -> waits forever for the mutex */
    UtAssert_INT32_EQ(OS_MutSemTake_Impl(&token), OS_SUCCESS);
    UtAssert_STUB_COUNT(xSemaphoreTakeRecursive, 1);
    UtAssert_True(UT_Stub_LastTakeMutex == UT_MUTEX_HANDLE, "xSemaphoreTakeRecursive() called with mutex handle");
    UtAssert_True(UT_Stub_LastTakeMutexTicks == portMAX_DELAY, "xSemaphoreTakeRecursive() waits forever");

    /* xSemaphoreTakeRecursive() failure -> OS_SEM_FAILURE */
    UT_SetDeferredRetcode(UT_KEY(xSemaphoreTakeRecursive), 1, -1);
    UtAssert_INT32_EQ(OS_MutSemTake_Impl(&token), OS_SEM_FAILURE);
    UtAssert_STUB_COUNT(xSemaphoreTakeRecursive, 2);
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
}

void Osapi_Test_Teardown(void)
{
}

#define ADD_TEST(test) UtTest_Add((Test_##test), Osapi_Test_Setup, Osapi_Test_Teardown, #test)

void UtTest_Setup(void)
{
    ADD_TEST(OS_MutSemCreate_Impl);
    ADD_TEST(OS_MutSemDelete_Impl);
    ADD_TEST(OS_MutSemGive_Impl);
    ADD_TEST(OS_MutSemTake_Impl);
    ADD_TEST(OS_MutSemGetInfo_Impl);
}
