/*
 * File: freertos-bsp-start-stubs.c
 *
 * Purpose:
 *   UT-controllable stand-ins for what generic-freertos/src/bsp_start.c calls
 *   - the fake FreeRTOS task/semaphore API (fake-inc) and the application's
 *   OS_Application_Startup()/OS_Application_Run() - so the exit status
 *   handling can be tested without a real scheduler. Each stub follows the
 *   standard ut_assert pattern: UT_DEFAULT_IMPL() lets a test script failure
 *   via UT_SetDeferredRetcode(), otherwise a successful default is used.
 *
 *   vTaskStartScheduler() stands in for the real scheduler: unless a test
 *   clears UT_Stub_SchedulerStarts, it runs the task that main() created,
 *   which is exactly what a started scheduler does.
 */

#include <stdbool.h>

#include "utstubs.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "osapi-common.h"

/* Captured xTaskCreate arguments */
TaskFunction_t         UT_Stub_StartupTask;
const char            *UT_Stub_LastCreateName;
configSTACK_DEPTH_TYPE UT_Stub_LastCreateStackDepth;
UBaseType_t            UT_Stub_LastCreatePriority;

/* Captured mutex/semaphore arguments */
StaticSemaphore_t *UT_Stub_LastCreateMutexBuffer;
SemaphoreHandle_t  UT_Stub_LastTakeSem;
TickType_t         UT_Stub_LastTakeTicks;
SemaphoreHandle_t  UT_Stub_LastGiveSem;

/* Captured OS_FreeRTOS_StackOverflow and OS_FreeRTOS_ApplicationExit arguments */
const char *UT_Stub_LastOverflowTask;
int         UT_Stub_LastExitStatus;

/* Whether vTaskStartScheduler() runs the startup task (a started scheduler) or returns at once */
bool UT_Stub_SchedulerStarts = true;

BaseType_t xTaskCreate(TaskFunction_t               pxTaskCode,
                       const char *const            pcName,
                       const configSTACK_DEPTH_TYPE uxStackDepth,
                       void *const                  pvParameters,
                       UBaseType_t                  uxPriority,
                       TaskHandle_t *const          pxCreatedTask)
{
    int32 status;

    (void)pvParameters;
    (void)pxCreatedTask;

    UT_Stub_StartupTask          = pxTaskCode;
    UT_Stub_LastCreateName       = pcName;
    UT_Stub_LastCreateStackDepth = uxStackDepth;
    UT_Stub_LastCreatePriority   = uxPriority;

    status = UT_DEFAULT_IMPL(xTaskCreate);

    return (status < 0) ? pdFAIL : pdPASS;
}

void vTaskStartScheduler(void)
{
    UT_DEFAULT_IMPL(vTaskStartScheduler);

    if (UT_Stub_SchedulerStarts && UT_Stub_StartupTask != NULL)
    {
        UT_Stub_StartupTask(NULL);
    }
}

void vTaskEndScheduler(void)
{
    UT_DEFAULT_IMPL(vTaskEndScheduler);
}

SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *pxMutexBuffer)
{
    UT_Stub_LastCreateMutexBuffer = pxMutexBuffer;

    UT_DEFAULT_IMPL(xSemaphoreCreateMutexStatic);

    return (SemaphoreHandle_t)pxMutexBuffer;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xBlockTime)
{
    UT_Stub_LastTakeSem   = xSemaphore;
    UT_Stub_LastTakeTicks = xBlockTime;

    UT_DEFAULT_IMPL(xSemaphoreTake);

    return pdTRUE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore)
{
    UT_Stub_LastGiveSem = xSemaphore;

    UT_DEFAULT_IMPL(xSemaphoreGive);

    return pdTRUE;
}

/* The real one (bsp_console.c) halts and never returns; this one records the call and returns */
void OS_FreeRTOS_StackOverflow(const char *TaskName)
{
    UT_Stub_LastOverflowTask = TaskName;

    UT_DEFAULT_IMPL(OS_FreeRTOS_StackOverflow);
}

void OS_FreeRTOS_ApplicationExit(int Status)
{
    UT_Stub_LastExitStatus = Status;

    UT_DEFAULT_IMPL(OS_FreeRTOS_ApplicationExit);
}

void OS_Application_Startup(void)
{
    UT_DEFAULT_IMPL(OS_Application_Startup);
}

void OS_Application_Run(void)
{
    UT_DEFAULT_IMPL(OS_Application_Run);
}
