/*
 * File: queue.h
 *
 * Purpose:
 *   Fake FreeRTOS queue.h for white-box coverage testing of
 *   freertos/src/os-impl-queue.c. Declares only the queue API that file
 *   actually calls - not a real port.
 *
 *   Declared under an OCS_ prefix and redirected via #define for the same
 *   reason as task.h: the real freertos_kernel is linked in transitively
 *   through ut_assert -> osal_bsp, so these must not resolve to the real
 *   symbol names.
 */

#ifndef QUEUE_H
#define QUEUE_H

#include "FreeRTOS.h"

typedef void *QueueHandle_t;

QueueHandle_t OCS_xQueueCreate(UBaseType_t uxQueueLength, UBaseType_t uxItemSize);

void OCS_vQueueDelete(QueueHandle_t xQueue);

BaseType_t OCS_xQueueSend(QueueHandle_t xQueue, const void *pvItemToQueue, TickType_t xTicksToWait);

BaseType_t OCS_xQueueReceive(QueueHandle_t xQueue, void *pvBuffer, TickType_t xTicksToWait);

#define xQueueCreate  OCS_xQueueCreate
#define vQueueDelete  OCS_vQueueDelete
#define xQueueSend    OCS_xQueueSend
#define xQueueReceive OCS_xQueueReceive

#endif /* QUEUE_H */
