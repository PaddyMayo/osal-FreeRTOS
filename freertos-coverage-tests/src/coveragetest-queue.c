/*
 * File: coveragetest-queue.c
 *
 * Purpose:
 *   White-box coverage test cases for freertos/src/os-impl-queue.c, run
 *   against the fake FreeRTOS queue API in ../fake-inc and stubbed in
 *   ../stubs/freertos-queue-stubs.c - no real FreeRTOS kernel involved.
 *   (The black box tests for this port are OSAL's own /src/tests and
 *   /src/unit-tests, which link the real OSAL library instead.)
 *
 *   Each _Impl function is called directly; preconditions that its public
 *   API cannot reach are set up through ../adaptors/inc/ut-adaptor-queue.h,
 *   and effects are observed via return codes plus the fake FreeRTOS stub
 *   layer's captured call arguments and counts.
 *
 *   The stubbed xQueueReceive()/xQueueSend() copy nothing real - what they
 *   are asked to do (timeout ticks, handle, item pointer) is captured and
 *   asserted on, and received data is scripted with UT_Stub_SetReceiveData().
 */

#include <string.h>

#include "utassert.h"
#include "uttest.h"
#include "utstubs.h"

#include "FreeRTOS.h"
#include "queue.h"

#include "os-shared-queue.h"
#include "os-shared-idmap.h"

#include "ut-adaptor-queue.h"

/* Captured stub call arguments, declared in freertos-queue-stubs.c */
extern QueueHandle_t UT_Stub_CreatedQueueHandle;
extern UBaseType_t   UT_Stub_LastCreateLength;
extern UBaseType_t   UT_Stub_LastCreateItemSize;
extern QueueHandle_t UT_Stub_LastDeletedQueue;
extern QueueHandle_t UT_Stub_LastSendQueue;
extern const void   *UT_Stub_LastSendItem;
extern TickType_t    UT_Stub_LastSendTicks;
extern QueueHandle_t UT_Stub_LastReceiveQueue;
extern TickType_t    UT_Stub_LastReceiveTicks;

extern void UT_Stub_SetReceiveData(const void *data, size_t size);
extern void UT_Stub_ResetReceiveData(void);

#define UT_QUEUE_TOKEN(idx)                                                                              \
    (OS_object_token_t)                                                                                  \
    {                                                                                                    \
        .obj_type = OS_OBJECT_TYPE_OS_QUEUE, .obj_id = (osal_id_t) { 0x20000 + (idx) }, .obj_idx = (idx) \
    }

#define UT_INDEX_0 OSAL_INDEX_C(0)

#define UT_QUEUE_DEPTH    4
#define UT_QUEUE_MSG_SIZE 8

/*
**********************************************************************************
**          TEST CASES
**********************************************************************************
*/

void Test_OS_QueueCreate_Impl(void)
{
    OS_object_token_t token = UT_QUEUE_TOKEN(0);

    OS_queue_table[0].max_depth = UT_QUEUE_DEPTH;
    OS_queue_table[0].max_size  = UT_QUEUE_MSG_SIZE;

    /* success -> handle stored in the impl table, sized from the shared record */
    UtAssert_INT32_EQ(OS_QueueCreate_Impl(&token, 0), OS_SUCCESS);
    UtAssert_STUB_COUNT(xQueueCreate, 1);
    UtAssert_UINT32_EQ(UT_Stub_LastCreateLength, UT_QUEUE_DEPTH);
    UtAssert_UINT32_EQ(UT_Stub_LastCreateItemSize, UT_QUEUE_MSG_SIZE);
    UtAssert_True(UT_QueueTest_GetImplQueueId(UT_INDEX_0) == UT_Stub_CreatedQueueHandle,
                  "xQueueCreate() handle stored in impl table");

    /* xQueueCreate() failure (e.g. out of heap) -> OS_ERROR, no handle kept */
    UT_SetDeferredRetcode(UT_KEY(xQueueCreate), 1, -1);
    UtAssert_INT32_EQ(OS_QueueCreate_Impl(&token, 0), OS_ERROR);
    UtAssert_STUB_COUNT(xQueueCreate, 2);
    UtAssert_True(UT_QueueTest_GetImplQueueId(UT_INDEX_0) == NULL, "no handle stored when xQueueCreate() fails");
}

void Test_OS_QueueDelete_Impl(void)
{
    OS_object_token_t token = UT_QUEUE_TOKEN(0);

    UT_QueueTest_SetImplQueueId(UT_INDEX_0, (QueueHandle_t)0x4242);

    UtAssert_INT32_EQ(OS_QueueDelete_Impl(&token), OS_SUCCESS);
    UtAssert_STUB_COUNT(vQueueDelete, 1);
    UtAssert_True(UT_Stub_LastDeletedQueue == (QueueHandle_t)0x4242, "vQueueDelete() called with queue handle");
}

void Test_OS_QueueGet_Impl(void)
{
    OS_object_token_t token = UT_QUEUE_TOKEN(0);
    unsigned char     message[UT_QUEUE_MSG_SIZE];
    unsigned char     oversized[UT_QUEUE_MSG_SIZE * 2];
    unsigned char     buffer[UT_QUEUE_MSG_SIZE];
    size_t            size_copied;

    memset(message, 0xA5, sizeof(message));

    OS_queue_table[0].max_depth = UT_QUEUE_DEPTH;
    OS_queue_table[0].max_size  = UT_QUEUE_MSG_SIZE;
    UT_QueueTest_SetImplQueueId(UT_INDEX_0, (QueueHandle_t)0x5151);

    /* caller buffer smaller than max_size -> rejected before touching FreeRTOS */
    size_copied = 99;
    UtAssert_INT32_EQ(OS_QueueGet_Impl(&token, buffer, sizeof(buffer) - 1, &size_copied, OS_CHECK),
                      OS_QUEUE_INVALID_SIZE);
    UtAssert_UINT32_EQ(size_copied, 0);
    UtAssert_STUB_COUNT(xQueueReceive, 0);

    /* OS_PEND -> waits forever, message copied out, max_size reported */
    UT_Stub_SetReceiveData(message, sizeof(message));
    memset(buffer, 0, sizeof(buffer));
    size_copied = 0;
    UtAssert_INT32_EQ(OS_QueueGet_Impl(&token, buffer, sizeof(buffer), &size_copied, OS_PEND), OS_SUCCESS);
    UtAssert_STUB_COUNT(xQueueReceive, 1);
    UtAssert_True(UT_Stub_LastReceiveQueue == (QueueHandle_t)0x5151, "xQueueReceive() called with queue handle");
    UtAssert_True(UT_Stub_LastReceiveTicks == portMAX_DELAY, "OS_PEND maps to portMAX_DELAY");
    UtAssert_UINT32_EQ(size_copied, UT_QUEUE_MSG_SIZE);
    UtAssert_MemCmp(buffer, message, sizeof(message), "message copied to caller buffer");

    /* OS_CHECK -> polls with zero ticks */
    UtAssert_INT32_EQ(OS_QueueGet_Impl(&token, buffer, sizeof(buffer), &size_copied, OS_CHECK), OS_SUCCESS);
    UtAssert_UINT32_EQ(UT_Stub_LastReceiveTicks, 0);

    /* millisecond timeout -> converted to ticks via pdMS_TO_TICKS() */
    UtAssert_INT32_EQ(OS_QueueGet_Impl(&token, buffer, sizeof(buffer), &size_copied, 250), OS_SUCCESS);
    UtAssert_UINT32_EQ(UT_Stub_LastReceiveTicks, 250);

    /* caller buffer larger than max_size -> fine, still reports max_size, not the buffer size */
    memset(oversized, 0, sizeof(oversized));
    size_copied = 0;
    UtAssert_INT32_EQ(OS_QueueGet_Impl(&token, oversized, sizeof(oversized), &size_copied, OS_CHECK), OS_SUCCESS);
    UtAssert_UINT32_EQ(size_copied, UT_QUEUE_MSG_SIZE);
    UtAssert_MemCmp(oversized, message, sizeof(message), "message copied to start of oversized buffer");

    /* nothing received while polling -> OS_QUEUE_EMPTY */
    size_copied = 99;
    UT_SetDeferredRetcode(UT_KEY(xQueueReceive), 1, -1);
    UtAssert_INT32_EQ(OS_QueueGet_Impl(&token, buffer, sizeof(buffer), &size_copied, OS_CHECK), OS_QUEUE_EMPTY);
    UtAssert_UINT32_EQ(size_copied, 0);

    /* nothing received before the millisecond timeout expired -> OS_QUEUE_TIMEOUT */
    size_copied = 99;
    UT_SetDeferredRetcode(UT_KEY(xQueueReceive), 1, -1);
    UtAssert_INT32_EQ(OS_QueueGet_Impl(&token, buffer, sizeof(buffer), &size_copied, 100), OS_QUEUE_TIMEOUT);
    UtAssert_UINT32_EQ(size_copied, 0);

    /* OS_PEND cannot legitimately time out, but a failing xQueueReceive() is still reported as one */
    size_copied = 99;
    UT_SetDeferredRetcode(UT_KEY(xQueueReceive), 1, -1);
    UtAssert_INT32_EQ(OS_QueueGet_Impl(&token, buffer, sizeof(buffer), &size_copied, OS_PEND), OS_QUEUE_TIMEOUT);
    UtAssert_UINT32_EQ(size_copied, 0);
}

void Test_OS_QueuePut_Impl(void)
{
    OS_object_token_t token = UT_QUEUE_TOKEN(0);
    unsigned char     message[UT_QUEUE_MSG_SIZE * 2];

    memset(message, 0x5A, sizeof(message));

    OS_queue_table[0].max_depth = UT_QUEUE_DEPTH;
    OS_queue_table[0].max_size  = UT_QUEUE_MSG_SIZE;
    UT_QueueTest_SetImplQueueId(UT_INDEX_0, (QueueHandle_t)0x6161);

    /* message smaller than max_size -> rejected before touching FreeRTOS */
    UtAssert_INT32_EQ(OS_QueuePut_Impl(&token, message, UT_QUEUE_MSG_SIZE - 1, 0), OS_QUEUE_INVALID_SIZE);
    UtAssert_STUB_COUNT(xQueueSend, 0);

    /* exact size -> sent to the right queue without blocking */
    UtAssert_INT32_EQ(OS_QueuePut_Impl(&token, message, UT_QUEUE_MSG_SIZE, 0), OS_SUCCESS);
    UtAssert_STUB_COUNT(xQueueSend, 1);
    UtAssert_True(UT_Stub_LastSendQueue == (QueueHandle_t)0x6161, "xQueueSend() called with queue handle");
    UtAssert_True(UT_Stub_LastSendItem == message, "xQueueSend() called with caller's message");
    UtAssert_UINT32_EQ(UT_Stub_LastSendTicks, 0);

    /* message larger than max_size -> accepted (FreeRTOS copies only max_size bytes of it) */
    UtAssert_INT32_EQ(OS_QueuePut_Impl(&token, message, sizeof(message), 0), OS_SUCCESS);
    UtAssert_STUB_COUNT(xQueueSend, 2);

    /* queue full -> OS_QUEUE_FULL */
    UT_SetDeferredRetcode(UT_KEY(xQueueSend), 1, -1);
    UtAssert_INT32_EQ(OS_QueuePut_Impl(&token, message, UT_QUEUE_MSG_SIZE, 0), OS_QUEUE_FULL);
    UtAssert_STUB_COUNT(xQueueSend, 3);
}

void Test_OS_QueueGetInfo_Impl(void)
{
    OS_object_token_t token = UT_QUEUE_TOKEN(0);
    OS_queue_prop_t   queue_prop;

    memset(&queue_prop, 0, sizeof(queue_prop));

    UtAssert_INT32_EQ(OS_QueueGetInfo_Impl(&token, &queue_prop), OS_SUCCESS);
}

/* ------------------- End of test cases --------------------------------------*/

void Osapi_Test_Setup(void)
{
    UT_ResetState(0);
    memset(OS_queue_table, 0, sizeof(OS_queue_table));
    memset(UT_Ref_OS_impl_queue_table, 0, UT_Ref_OS_impl_queue_table_SIZE);
    UT_Stub_ResetReceiveData();
}

void Osapi_Test_Teardown(void)
{
}

#define ADD_TEST(test) UtTest_Add((Test_##test), Osapi_Test_Setup, Osapi_Test_Teardown, #test)

void UtTest_Setup(void)
{
    ADD_TEST(OS_QueueCreate_Impl);
    ADD_TEST(OS_QueueDelete_Impl);
    ADD_TEST(OS_QueueGet_Impl);
    ADD_TEST(OS_QueuePut_Impl);
    ADD_TEST(OS_QueueGetInfo_Impl);
}
