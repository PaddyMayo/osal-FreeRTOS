/*
 * File: freertos-queue-stubs.c
 *
 * Purpose:
 *   UT-controllable stub implementations of the fake FreeRTOS queue API
 *   (fake-inc/queue.h), used to white-box test freertos/src/os-impl-queue.c
 *   without a real FreeRTOS kernel. Follows the same pattern as
 *   freertos-tasks-stubs.c: UT_DEFAULT_IMPL() lets a test script failure via
 *   UT_SetDeferredRetcode(), otherwise success is the default. Arguments
 *   worth asserting on are captured into the extern globals below.
 */

#include <string.h>

#include "utstubs.h"

#include "FreeRTOS.h"
#include "queue.h"

#define UT_STUB_MAX_ITEM_SIZE 64

/* Handle returned by a successful xQueueCreate() */
QueueHandle_t UT_Stub_CreatedQueueHandle = (QueueHandle_t)0x2000;

UBaseType_t UT_Stub_LastCreateLength;
UBaseType_t UT_Stub_LastCreateItemSize;

QueueHandle_t UT_Stub_LastDeletedQueue;

QueueHandle_t UT_Stub_LastSendQueue;
const void   *UT_Stub_LastSendItem;
TickType_t    UT_Stub_LastSendTicks;

QueueHandle_t UT_Stub_LastReceiveQueue;
TickType_t    UT_Stub_LastReceiveTicks;

static unsigned char UT_Stub_ReceiveData[UT_STUB_MAX_ITEM_SIZE];
static size_t        UT_Stub_ReceiveDataSize;

/*
 * os-impl-queue.c converts its timeout through OS_FreeRTOS_MsToTicks(), which
 * lives in os-impl-tasks.c and is covered there. This runner links only the
 * queue module, so it gets a stand-in that records the input and returns a
 * marker, letting the test check the call is made and its result is used.
 */
uint32     UT_Stub_LastMsToTicksInput;
TickType_t UT_Stub_MsToTicksResult = 0x1234;

TickType_t OS_FreeRTOS_MsToTicks(uint32 millisecond)
{
    UT_Stub_LastMsToTicksInput = millisecond;

    return UT_Stub_MsToTicksResult;
}

QueueHandle_t xQueueCreate(UBaseType_t uxQueueLength, UBaseType_t uxItemSize)
{
    int32 status;

    UT_Stub_LastCreateLength   = uxQueueLength;
    UT_Stub_LastCreateItemSize = uxItemSize;

    status = UT_DEFAULT_IMPL(xQueueCreate);

    if (status < 0)
    {
        return NULL;
    }

    return UT_Stub_CreatedQueueHandle;
}

void vQueueDelete(QueueHandle_t xQueue)
{
    UT_Stub_LastDeletedQueue = xQueue;

    UT_DEFAULT_IMPL(vQueueDelete);
}

BaseType_t xQueueSend(QueueHandle_t xQueue, const void *pvItemToQueue, TickType_t xTicksToWait)
{
    int32 status;

    UT_Stub_LastSendQueue = xQueue;
    UT_Stub_LastSendItem  = pvItemToQueue;
    UT_Stub_LastSendTicks = xTicksToWait;

    status = UT_DEFAULT_IMPL(xQueueSend);

    if (status < 0)
    {
        return pdFAIL;
    }

    return pdPASS;
}

BaseType_t xQueueReceive(QueueHandle_t xQueue, void *pvBuffer, TickType_t xTicksToWait)
{
    int32 status;

    UT_Stub_LastReceiveQueue = xQueue;
    UT_Stub_LastReceiveTicks = xTicksToWait;

    status = UT_DEFAULT_IMPL(xQueueReceive);

    if (status < 0)
    {
        return pdFAIL;
    }

    /* Real FreeRTOS copies a whole item into the caller's buffer on success */
    memcpy(pvBuffer, UT_Stub_ReceiveData, UT_Stub_ReceiveDataSize);

    return pdPASS;
}

/*
 * Test-only helpers (not fakes of any real FreeRTOS API).
 */

/* Sets the item the next successful xQueueReceive() will copy out */
void UT_Stub_SetReceiveData(const void *data, size_t size)
{
    if (size > UT_STUB_MAX_ITEM_SIZE)
    {
        size = UT_STUB_MAX_ITEM_SIZE;
    }

    memcpy(UT_Stub_ReceiveData, data, size);
    UT_Stub_ReceiveDataSize = size;
}

/* Clears the captured receive item - it is file-static state that
 * UT_ResetState() knows nothing about. */
void UT_Stub_ResetReceiveData(void)
{
    memset(UT_Stub_ReceiveData, 0, sizeof(UT_Stub_ReceiveData));
    UT_Stub_ReceiveDataSize = 0;
}
