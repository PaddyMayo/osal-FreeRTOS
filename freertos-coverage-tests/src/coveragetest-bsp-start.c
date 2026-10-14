/*
 * File: coveragetest-bsp-start.c
 *
 * Purpose:
 *   White-box coverage test cases for generic-freertos/src/bsp_start.c, run
 *   against the fake FreeRTOS API in ../fake-inc and the stubs in
 *   ../stubs/freertos-bsp-start-stubs.c - no real FreeRTOS kernel involved.
 *
 *   main() is built under the name OS_BSP_TestMain (see ../CMakeLists.txt) so
 *   it does not collide with this executable's own main(). The stubbed
 *   vTaskStartScheduler() runs the startup task, so each case sees the exit
 *   status main() would hand back to the OS: success after a normal shutdown,
 *   the application's own status when it set one, and failure when the
 *   scheduler never starts or a task overflows its stack.
 */

#include <stdlib.h>
#include <string.h>

#include "utassert.h"
#include "uttest.h"
#include "utstubs.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "osapi-common.h"
#include "bsp-impl.h"

/* bsp_start.c's main(), renamed at build time */
int OS_BSP_TestMain(int argc, char *argv[]);

/* FreeRTOS hooks defined in bsp_start.c, declared by the kernel's own headers in a real build */
void vApplicationGetIdleTaskMemory(StaticTask_t          **ppxIdleTaskTCBBuffer,
                                   StackType_t           **ppxIdleTaskStackBuffer,
                                   configSTACK_DEPTH_TYPE *puxIdleTaskStackSize);
void vApplicationGetTimerTaskMemory(StaticTask_t          **ppxTimerTaskTCBBuffer,
                                    StackType_t           **ppxTimerTaskStackBuffer,
                                    configSTACK_DEPTH_TYPE *puxTimerTaskStackSize);
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName);

/* Captured stub call arguments and controls, declared in freertos-bsp-start-stubs.c */
extern TaskFunction_t         UT_Stub_StartupTask;
extern const char            *UT_Stub_LastCreateName;
extern configSTACK_DEPTH_TYPE UT_Stub_LastCreateStackDepth;
extern UBaseType_t            UT_Stub_LastCreatePriority;
extern StaticSemaphore_t     *UT_Stub_LastCreateMutexBuffer;
extern SemaphoreHandle_t      UT_Stub_LastTakeSem;
extern TickType_t             UT_Stub_LastTakeTicks;
extern SemaphoreHandle_t      UT_Stub_LastGiveSem;
extern bool                   UT_Stub_SchedulerStarts;

/* The status the application sets from inside OS_Application_Run() */
static int32 AppExitCode;

static int32 SetAppExitCodeHook(void *UserObj, int32 StubRetcode, uint32 CallCount, const UT_StubContext_t *Context)
{
    (void)UserObj;
    (void)CallCount;
    (void)Context;

    OS_BSP_SetExitCode(AppExitCode);

    return StubRetcode;
}

/*
**********************************************************************************
**          TEST CASES
**********************************************************************************
*/

void Test_Main_NormalShutdown(void)
{
    char *argv[] = { "prog", "arg" };

    UtAssert_INT32_EQ(OS_BSP_TestMain(2, argv), EXIT_SUCCESS);

    /* arguments saved, mutex and startup task created, the application ran, then the scheduler ended */
    UtAssert_UINT32_EQ(OS_BSP_Global.ArgC, 2);
    UtAssert_True(OS_BSP_Global.ArgV == argv, "argv saved");
    UtAssert_STUB_COUNT(xSemaphoreCreateMutexStatic, 1);
    UtAssert_STUB_COUNT(xTaskCreate, 1);
    UtAssert_True(strcmp(UT_Stub_LastCreateName, "OSAL_Startup") == 0, "startup task named OSAL_Startup");
    UtAssert_UINT32_EQ(UT_Stub_LastCreateStackDepth, configMINIMAL_STACK_SIZE * 4);
    UtAssert_UINT32_EQ(UT_Stub_LastCreatePriority, configMAX_PRIORITIES / 2);
    UtAssert_STUB_COUNT(vTaskStartScheduler, 1);
    UtAssert_STUB_COUNT(OS_Application_Startup, 1);
    UtAssert_STUB_COUNT(OS_Application_Run, 1);
    UtAssert_STUB_COUNT(vTaskEndScheduler, 1);
    UtAssert_INT32_EQ(OS_BSP_Global.AppStatus, OS_SUCCESS);
}

void Test_Main_AppExitStatus(void)
{
    /* the application's status wins over the default, and is translated into a process exit code */
    UT_SetHookFunction(UT_KEY(OS_Application_Run), SetAppExitCodeHook, NULL);

    AppExitCode = 7;
    UtAssert_INT32_EQ(OS_BSP_TestMain(0, NULL), 7);

    AppExitCode = OS_ERROR;
    UtAssert_INT32_EQ(OS_BSP_TestMain(0, NULL), EXIT_FAILURE);

    /* only the low 7 bits pass through */
    AppExitCode = 0x85;
    UtAssert_INT32_EQ(OS_BSP_TestMain(0, NULL), 0x05);
}

void Test_Main_SchedulerFailsToStart(void)
{
    /* vTaskStartScheduler() returns at once, so the startup task never runs */
    UT_Stub_SchedulerStarts = false;

    UtAssert_INT32_EQ(OS_BSP_TestMain(0, NULL), EXIT_FAILURE);

    UtAssert_STUB_COUNT(vTaskStartScheduler, 1);
    UtAssert_STUB_COUNT(OS_Application_Startup, 0);
    UtAssert_STUB_COUNT(OS_Application_Run, 0);
    UtAssert_STUB_COUNT(vTaskEndScheduler, 0);
    UtAssert_INT32_EQ(OS_BSP_Global.AppStatus, OS_ERROR);
}

void Test_Main_TaskCreateFails(void)
{
    UT_SetDefaultReturnValue(UT_KEY(xTaskCreate), -1);

    UtAssert_INT32_EQ(OS_BSP_TestMain(0, NULL), EXIT_FAILURE);

    /* without the startup task there is nothing for a scheduler to run */
    UtAssert_STUB_COUNT(vTaskStartScheduler, 0);
}

void Test_StackOverflowHook(void)
{
    /* a stack overflow ends the scheduler with a failure status... */
    vApplicationStackOverflowHook(NULL, "task");
    UtAssert_INT32_EQ(OS_BSP_Global.AppStatus, OS_ERROR);
    UtAssert_STUB_COUNT(vTaskEndScheduler, 1);

    /* ...but never replaces a status the application already chose */
    OS_BSP_Global.AppStatus = 7;
    vApplicationStackOverflowHook(NULL, "task");
    UtAssert_INT32_EQ(OS_BSP_Global.AppStatus, 7);
    UtAssert_STUB_COUNT(vTaskEndScheduler, 2);
}

void Test_Lock_Unlock(void)
{
    /* main() creates the console mutex that the lock functions use */
    OS_BSP_TestMain(0, NULL);

    OS_BSP_Lock_Impl();
    UtAssert_True(UT_Stub_LastTakeSem == (SemaphoreHandle_t)UT_Stub_LastCreateMutexBuffer,
                  "lock takes the console mutex");
    UtAssert_True(UT_Stub_LastTakeTicks == portMAX_DELAY, "lock waits forever");

    OS_BSP_Unlock_Impl();
    UtAssert_True(UT_Stub_LastGiveSem == (SemaphoreHandle_t)UT_Stub_LastCreateMutexBuffer,
                  "unlock gives the console mutex");
}

void Test_StaticAllocationHooks(void)
{
    StaticTask_t          *IdleTcb;
    StackType_t           *IdleStack;
    configSTACK_DEPTH_TYPE IdleSize;
    StaticTask_t          *TimerTcb;
    StackType_t           *TimerStack;
    configSTACK_DEPTH_TYPE TimerSize;

    vApplicationGetIdleTaskMemory(&IdleTcb, &IdleStack, &IdleSize);
    vApplicationGetTimerTaskMemory(&TimerTcb, &TimerStack, &TimerSize);

    UtAssert_True(IdleTcb != NULL && IdleStack != NULL, "idle task memory supplied");
    UtAssert_UINT32_EQ(IdleSize, configMINIMAL_STACK_SIZE);
    UtAssert_True(TimerTcb != NULL && TimerStack != NULL, "timer task memory supplied");
    UtAssert_UINT32_EQ(TimerSize, configTIMER_TASK_STACK_DEPTH);
    UtAssert_True(IdleTcb != TimerTcb && IdleStack != TimerStack, "idle and timer tasks get separate memory");
}

/*
 * Setup function prior to every test
 */
void Osapi_Test_Setup(void)
{
    UT_ResetState(0);
    memset(&OS_BSP_Global, 0, sizeof(OS_BSP_Global));
    UT_Stub_StartupTask           = NULL;
    UT_Stub_LastCreateName        = NULL;
    UT_Stub_LastCreateStackDepth  = 0;
    UT_Stub_LastCreatePriority    = 0;
    UT_Stub_LastCreateMutexBuffer = NULL;
    UT_Stub_LastTakeSem           = NULL;
    UT_Stub_LastTakeTicks         = 0;
    UT_Stub_LastGiveSem           = NULL;
    UT_Stub_SchedulerStarts       = true;
    AppExitCode                   = 0;
}

void Osapi_Test_Teardown(void)
{
}

#define ADD_TEST(test) UtTest_Add((Test_##test), Osapi_Test_Setup, Osapi_Test_Teardown, #test)

void UtTest_Setup(void)
{
    ADD_TEST(Main_NormalShutdown);
    ADD_TEST(Main_AppExitStatus);
    ADD_TEST(Main_SchedulerFailsToStart);
    ADD_TEST(Main_TaskCreateFails);
    ADD_TEST(StackOverflowHook);
    ADD_TEST(Lock_Unlock);
    ADD_TEST(StaticAllocationHooks);
}
