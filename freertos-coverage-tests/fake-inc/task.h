/*
 * File: task.h
 *
 * Purpose:
 *   Fake FreeRTOS task.h for white-box coverage testing of
 *   freertos/src/os-impl-tasks.c. Declares only the task API that file
 *   actually calls - not a real port.
 *
 *   Declared under an OCS_ prefix and redirected via #define, mirroring
 *   OSAL's own override_inc/OCS_ convention: linking ut_assert pulls in
 *   osal_bsp, which (in this project) pulls in the real freertos_kernel, so
 *   os-impl-tasks.c's calls must not resolve to the real symbol names or
 *   they collide with the real kernel's own definitions at link time.
 */

#ifndef TASK_H
#define TASK_H

#include "FreeRTOS.h"

typedef void (*TaskFunction_t)(void *);

TaskHandle_t OCS_xTaskGetCurrentTaskHandle(void);

TaskHandle_t OCS_xTaskCreateStatic(TaskFunction_t               pxTaskCode,
                                   const char *const            pcName,
                                   const configSTACK_DEPTH_TYPE ulStackDepth,
                                   void *const                  pvParameters,
                                   UBaseType_t                  uxPriority,
                                   StackType_t *const           puxStackBuffer,
                                   StaticTask_t *const          pxTaskBuffer);

void OCS_vTaskDelete(TaskHandle_t xTaskToDelete);

void OCS_vTaskDelay(TickType_t xTicksToDelay);

void OCS_vTaskPrioritySet(TaskHandle_t xTask, UBaseType_t uxNewPriority);

void *OCS_pvTaskGetThreadLocalStoragePointer(TaskHandle_t xTaskToQuery, int xIndex);

void OCS_vTaskSetThreadLocalStoragePointer(TaskHandle_t xTaskToSet, int xIndex, void *pvValue);

#define xTaskGetCurrentTaskHandle          OCS_xTaskGetCurrentTaskHandle
#define xTaskCreateStatic                  OCS_xTaskCreateStatic
#define vTaskDelete                        OCS_vTaskDelete
#define vTaskDelay                         OCS_vTaskDelay
#define vTaskPrioritySet                   OCS_vTaskPrioritySet
#define pvTaskGetThreadLocalStoragePointer OCS_pvTaskGetThreadLocalStoragePointer
#define vTaskSetThreadLocalStoragePointer  OCS_vTaskSetThreadLocalStoragePointer

#endif /* TASK_H */
