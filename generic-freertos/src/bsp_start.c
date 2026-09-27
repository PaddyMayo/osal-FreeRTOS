/*
 * File:  bsp_start.c
 *
 * Purpose:
 *   Host-independent FreeRTOS OSAL BSP entry point and support code, shared
 *   across every FreeRTOS host-specific BSP directory (e.g. generic-freertos-posix).
 *   Each consumer compiles this file itself against its own FreeRTOSConfig.h,
 *   since some of the code below sizes static buffers from config values that
 *   legitimately differ per host.
 */

#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"

#include "osapi-common.h"
#include "bsp-impl.h"

/*
 * Console output may be locked/unlocked from any FreeRTOS task, so a
 * statically-allocated mutex is used (avoids relying on the heap, and is
 * always valid even if referenced before the scheduler is started).
 */
static StaticSemaphore_t OS_BSP_ConsoleMutexBuffer;
static SemaphoreHandle_t OS_BSP_ConsoleMutex;

/*----------------------------------------------------------------
   OS_BSP_Lock_Impl
   See full description in header
 ------------------------------------------------------------------*/
void OS_BSP_Lock_Impl(void)
{
    xSemaphoreTake(OS_BSP_ConsoleMutex, portMAX_DELAY);
}

/*----------------------------------------------------------------
   OS_BSP_Unlock_Impl
   See full description in header
 ------------------------------------------------------------------*/
void OS_BSP_Unlock_Impl(void)
{
    xSemaphoreGive(OS_BSP_ConsoleMutex);
}

/*----------------------------------------------------------------
   OS_BSP_GetReturnStatus()

    Helper function to convert an OSAL status code into
    a code suitable for returning to the OS.
 ------------------------------------------------------------------*/
static int OS_BSP_GetReturnStatus(void)
{
    int retcode;

    switch (OS_BSP_Global.AppStatus)
    {
        case OS_SUCCESS:
            retcode = EXIT_SUCCESS;
            break;

        case OS_ERROR:
            retcode = EXIT_FAILURE;
            break;

        default:
            retcode = OS_BSP_Global.AppStatus & 0x7F;
            break;
    }

    return retcode;
}

/*----------------------------------------------------------------
   OS_BSP_Shutdown_Impl
   See full description in header
 ------------------------------------------------------------------*/
void OS_BSP_Shutdown_Impl(void)
{
    vTaskEndScheduler();
}

/*----------------------------------------------------------------
   FreeRTOS static-allocation hooks

   Required because FreeRTOSConfig.h sets configSUPPORT_STATIC_ALLOCATION
   and configCHECK_FOR_STACK_OVERFLOW - FreeRTOS calls these rather than
   allocating the idle/timer task memory itself, and reports a detected
   stack overflow through the hook instead of a fixed action.
 ------------------------------------------------------------------*/
void vApplicationGetIdleTaskMemory(StaticTask_t          **ppxIdleTaskTCBBuffer,
                                   StackType_t           **ppxIdleTaskStackBuffer,
                                   configSTACK_DEPTH_TYPE *puxIdleTaskStackSize)
{
    static StaticTask_t IdleTaskTCBBuffer;
    static StackType_t  IdleTaskStackBuffer[configMINIMAL_STACK_SIZE];

    *ppxIdleTaskTCBBuffer   = &IdleTaskTCBBuffer;
    *ppxIdleTaskStackBuffer = IdleTaskStackBuffer;
    *puxIdleTaskStackSize   = configMINIMAL_STACK_SIZE;
}

void vApplicationGetTimerTaskMemory(StaticTask_t          **ppxTimerTaskTCBBuffer,
                                    StackType_t           **ppxTimerTaskStackBuffer,
                                    configSTACK_DEPTH_TYPE *puxTimerTaskStackSize)
{
    static StaticTask_t TimerTaskTCBBuffer;
    static StackType_t  TimerTaskStackBuffer[configTIMER_TASK_STACK_DEPTH];

    *ppxTimerTaskTCBBuffer   = &TimerTaskTCBBuffer;
    *ppxTimerTaskStackBuffer = TimerTaskStackBuffer;
    *puxTimerTaskStackSize   = configTIMER_TASK_STACK_DEPTH;
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;

    OS_BSP_Shutdown_Impl();
}

/*----------------------------------------------------------------
   OS_BSP_StartupTask

   FreeRTOS does not run any created task until vTaskStartScheduler()
   is called, and that call does not return during normal operation -
   so the application entry points must run from within a task of
   their own, rather than directly from main().
 ------------------------------------------------------------------*/
// pvParameters must stay non-const to match FreeRTOS's TaskFunction_t signature
// cppcheck-suppress constParameterCallback
static void OS_BSP_StartupTask(void *pvParameters)
{
    /*
     * Call application specific entry point.
     * This should set up all user tasks and resources, then return
     */
    OS_Application_Startup();

    /*
     * OS_Application_Run() implements the background task.
     * The user application may provide this, or a default implementation
     * is used which just calls OS_IdleLoop().
     */
    OS_Application_Run();

    /* Should typically never get here */
    vTaskEndScheduler();
}

/******************************************************************************
**
**  Purpose:
**    BSP Application entry point.
**
**  Arguments:
**    (none)
**
**  Return:
**    (none)
*/
int main(int argc, char *argv[])
{
    /*
     * Initially clear the global objects
     */
    memset(&OS_BSP_Global, 0, sizeof(OS_BSP_Global));

    /*
     * Save the argc/argv arguments for future use.
     */
    OS_BSP_Global.ArgC = argc;
    OS_BSP_Global.ArgV = argv;

    OS_BSP_ConsoleMutex = xSemaphoreCreateMutexStatic(&OS_BSP_ConsoleMutexBuffer);

    /*
     * Create the initial task, which performs application startup and then
     * runs the application's main loop.  vTaskStartScheduler() below is what
     * actually begins executing it.
     */
    if (xTaskCreate(OS_BSP_StartupTask,
                    "OSAL_Startup",
                    (configMINIMAL_STACK_SIZE * 4),
                    NULL,
                    (configMAX_PRIORITIES / 2),
                    NULL)
        != pdPASS)
    {
        /* Without this task the scheduler would run forever with nothing to do */
        return EXIT_FAILURE;
    }

    vTaskStartScheduler();

    /*
     * Only reached if the scheduler could not start (e.g. insufficient heap
     * for the idle/timer tasks), or if OS_BSP_Shutdown_Impl() ended it.
     */
    return OS_BSP_GetReturnStatus();
}
