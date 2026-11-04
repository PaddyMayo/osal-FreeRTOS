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
#include "task.h"
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
extern uint32        UT_Stub_LastMsToTicksInput;
extern TickType_t    UT_Stub_MsToTicksResult;

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

#define UT_QUEUE_HANDLE ((QueueHandle_t)0x4242)
#define UT_TASK_A       ((TaskHandle_t)0x7101)
#define UT_TASK_B       ((TaskHandle_t)0x7102)
#define UT_OTHER_TASK   ((TaskHandle_t)0x7103)

/* Declared in freertos-queue-stubs.c */
extern TaskHandle_t UT_Stub_QueueCurrentTask;
extern TaskHandle_t UT_Stub_LastAbortedTask;
extern TickType_t   UT_Stub_QueueLastDelayTicks;

/* What the hooks below saw at the moment they were called */
static uint32       UT_Seen_Users;
static uint32       UT_Seen_UsersAtDelay;
static uint32       UT_Seen_UsersAtFree;
static TaskHandle_t UT_Seen_Waiter;

/* Inside xQueueReceive(): the Get has registered, and another Get joins the list in front of it */
static int32
UT_ReceiveSeesRegistered(void *UserObj, int32 StubRetcode, uint32 CallCount, const UT_StubContext_t *Context)
{
    UT_Seen_Users  = UT_QueueTest_GetImplUsers(UT_INDEX_0);
    UT_Seen_Waiter = UT_QueueTest_GetImplWaiterTask(UT_INDEX_0);

    UT_QueueTest_PushWaiter(UT_INDEX_0, UT_OTHER_TASK);

    return StubRetcode;
}

/* Inside xQueueReceive(): the queue is deleted, so the blocked Get is woken without a message */
static int32 UT_ReceiveWhileDeleted(void *UserObj, int32 StubRetcode, uint32 CallCount, const UT_StubContext_t *Context)
{
    UT_QueueTest_SetImplQueueId(UT_INDEX_0, NULL);

    return -1;
}

/* Inside vTaskDelay(): the woken Gets run and leave */
static int32
UT_DelayReleasesWaiters(void *UserObj, int32 StubRetcode, uint32 CallCount, const UT_StubContext_t *Context)
{
    UT_Seen_UsersAtDelay = UT_QueueTest_GetImplUsers(UT_INDEX_0);

    UT_QueueTest_ReleaseWaiters(UT_INDEX_0);

    return StubRetcode;
}

/* Inside vQueueDelete(): who is still using the queue that is being freed */
static int32 UT_FreeSeesUsers(void *UserObj, int32 StubRetcode, uint32 CallCount, const UT_StubContext_t *Context)
{
    UT_Seen_UsersAtFree = UT_QueueTest_GetImplUsers(UT_INDEX_0);

    return StubRetcode;
}

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

    /* nobody was using it -> nothing to wake, nothing to wait for, and the slot no longer holds the handle */
    UtAssert_STUB_COUNT(xTaskAbortDelay, 0);
    UtAssert_STUB_COUNT(vTaskDelay, 0);
    UtAssert_True(UT_QueueTest_GetImplQueueId(UT_INDEX_0) == NULL, "queue handle cleared from impl table");
}

void Test_OS_QueueDelete_Impl_Waiters(void)
{
    OS_object_token_t token = UT_QUEUE_TOKEN(0);

    UT_QueueTest_SetImplQueueId(UT_INDEX_0, UT_QUEUE_HANDLE);

    /* two tasks blocked in OS_QueueGet(); each delay lets them run and leave */
    UT_QueueTest_PushWaiter(UT_INDEX_0, UT_TASK_A);
    UT_QueueTest_PushWaiter(UT_INDEX_0, UT_TASK_B);
    UT_SetHookFunction(UT_KEY(vTaskDelay), UT_DelayReleasesWaiters, NULL);
    UT_SetHookFunction(UT_KEY(vQueueDelete), UT_FreeSeesUsers, NULL);

    UtAssert_INT32_EQ(OS_QueueDelete_Impl(&token), OS_SUCCESS);

    /* both were woken (the newest first, so TASK_A was the last), then it waited one tick for them */
    UtAssert_STUB_COUNT(xTaskAbortDelay, 2);
    UtAssert_True(UT_Stub_LastAbortedTask == UT_TASK_A, "every blocked Get was woken");
    UtAssert_STUB_COUNT(vTaskDelay, 1);
    UtAssert_UINT32_EQ(UT_Stub_QueueLastDelayTicks, 1);
    UtAssert_UINT32_EQ(UT_Seen_UsersAtDelay, 2);

    /* the queue is freed only once nobody uses it any more */
    UtAssert_STUB_COUNT(vQueueDelete, 1);
    UtAssert_True(UT_Stub_LastDeletedQueue == UT_QUEUE_HANDLE, "vQueueDelete() called with queue handle");
    UtAssert_UINT32_EQ(UT_Seen_UsersAtFree, 0);
    UtAssert_True(UT_QueueTest_GetImplQueueId(UT_INDEX_0) == NULL, "queue handle cleared from impl table");
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

    /* millisecond timeout -> converted by OS_FreeRTOS_MsToTicks() (its rounding is covered with the tasks module) */
    UtAssert_INT32_EQ(OS_QueueGet_Impl(&token, buffer, sizeof(buffer), &size_copied, 250), OS_SUCCESS);
    UtAssert_UINT32_EQ(UT_Stub_LastMsToTicksInput, 250);
    UtAssert_UINT32_EQ(UT_Stub_LastReceiveTicks, UT_Stub_MsToTicksResult);

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

void Test_OS_QueueGet_Impl_Registers(void)
{
    OS_object_token_t token = UT_QUEUE_TOKEN(0);
    unsigned char     message[UT_QUEUE_MSG_SIZE];
    unsigned char     buffer[UT_QUEUE_MSG_SIZE];
    size_t            size_copied = 0;

    memset(message, 0xA5, sizeof(message));

    OS_queue_table[0].max_depth = UT_QUEUE_DEPTH;
    OS_queue_table[0].max_size  = UT_QUEUE_MSG_SIZE;
    UT_QueueTest_SetImplQueueId(UT_INDEX_0, UT_QUEUE_HANDLE);
    UT_Stub_SetReceiveData(message, sizeof(message));
    UT_SetHookFunction(UT_KEY(xQueueReceive), UT_ReceiveSeesRegistered, NULL);

    UtAssert_INT32_EQ(OS_QueueGet_Impl(&token, buffer, sizeof(buffer), &size_copied, OS_PEND), OS_SUCCESS);

    /* while blocked, the Get counted as a user and was listed under the task that called it */
    UtAssert_UINT32_EQ(UT_Seen_Users, 1);
    UtAssert_True(UT_Seen_Waiter == UT_Stub_QueueCurrentTask, "blocked Get listed under its task");

    /* it left the list from behind the Get that joined while it was blocked, and that one is untouched */
    UtAssert_UINT32_EQ(UT_QueueTest_GetImplUsers(UT_INDEX_0), 1);
    UtAssert_True(UT_QueueTest_GetImplWaiterTask(UT_INDEX_0) == UT_OTHER_TASK, "other Get still listed");

    UT_QueueTest_ReleaseWaiters(UT_INDEX_0);
    UtAssert_UINT32_EQ(UT_QueueTest_GetImplUsers(UT_INDEX_0), 0);
}

void Test_OS_QueueGet_Impl_Deleted(void)
{
    OS_object_token_t token = UT_QUEUE_TOKEN(0);
    unsigned char     buffer[UT_QUEUE_MSG_SIZE];
    size_t            size_copied;

    OS_queue_table[0].max_depth = UT_QUEUE_DEPTH;
    OS_queue_table[0].max_size  = UT_QUEUE_MSG_SIZE;

    /* the queue is already gone (a delete has started) -> refused before FreeRTOS is touched */
    UT_QueueTest_SetImplQueueId(UT_INDEX_0, NULL);
    size_copied = 99;
    UtAssert_INT32_EQ(OS_QueueGet_Impl(&token, buffer, sizeof(buffer), &size_copied, OS_PEND), OS_ERR_INVALID_ID);
    UtAssert_UINT32_EQ(size_copied, 0);
    UtAssert_STUB_COUNT(xQueueReceive, 0);
    UtAssert_UINT32_EQ(UT_QueueTest_GetImplUsers(UT_INDEX_0), 0);
    UtAssert_True(UT_QueueTest_GetImplWaiterTask(UT_INDEX_0) == NULL, "refused Get not listed");

    /* deleted while blocked: the Get is woken without a message and reports that, not a timeout */
    UT_QueueTest_SetImplQueueId(UT_INDEX_0, UT_QUEUE_HANDLE);
    UT_SetHookFunction(UT_KEY(xQueueReceive), UT_ReceiveWhileDeleted, NULL);
    size_copied = 99;
    UtAssert_INT32_EQ(OS_QueueGet_Impl(&token, buffer, sizeof(buffer), &size_copied, OS_PEND), OS_ERROR);
    UtAssert_STUB_COUNT(xQueueReceive, 1);
    UtAssert_UINT32_EQ(size_copied, 0);
    UtAssert_UINT32_EQ(UT_QueueTest_GetImplUsers(UT_INDEX_0), 0);
    UtAssert_True(UT_QueueTest_GetImplWaiterTask(UT_INDEX_0) == NULL, "woken Get no longer listed");
}

void Test_OS_QueuePut_Impl_Deleted(void)
{
    OS_object_token_t token = UT_QUEUE_TOKEN(0);
    unsigned char     message[UT_QUEUE_MSG_SIZE];

    memset(message, 0x5A, sizeof(message));

    OS_queue_table[0].max_depth = UT_QUEUE_DEPTH;
    OS_queue_table[0].max_size  = UT_QUEUE_MSG_SIZE;

    /* the queue is already gone (a delete has started) -> refused before FreeRTOS is touched */
    UT_QueueTest_SetImplQueueId(UT_INDEX_0, NULL);
    UtAssert_INT32_EQ(OS_QueuePut_Impl(&token, message, UT_QUEUE_MSG_SIZE, 0), OS_ERR_INVALID_ID);
    UtAssert_STUB_COUNT(xQueueSend, 0);
    UtAssert_UINT32_EQ(UT_QueueTest_GetImplUsers(UT_INDEX_0), 0);

    /* a Put that finishes leaves no trace for a delete to wait on */
    UT_QueueTest_SetImplQueueId(UT_INDEX_0, UT_QUEUE_HANDLE);
    UtAssert_INT32_EQ(OS_QueuePut_Impl(&token, message, UT_QUEUE_MSG_SIZE, 0), OS_SUCCESS);
    UtAssert_UINT32_EQ(UT_QueueTest_GetImplUsers(UT_INDEX_0), 0);
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
    ADD_TEST(OS_QueueDelete_Impl_Waiters);
    ADD_TEST(OS_QueueGet_Impl);
    ADD_TEST(OS_QueueGet_Impl_Registers);
    ADD_TEST(OS_QueueGet_Impl_Deleted);
    ADD_TEST(OS_QueuePut_Impl);
    ADD_TEST(OS_QueuePut_Impl_Deleted);
    ADD_TEST(OS_QueueGetInfo_Impl);
}
