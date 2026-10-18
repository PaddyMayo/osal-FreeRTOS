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
 *   scheduler never starts. A stack overflow halts instead of returning, so
 *   the hook is checked for what it calls rather than for an exit status.
 */

#include <setjmp.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>

#include "utassert.h"
#include "uttest.h"
#include "utstubs.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "osapi-common.h"
#include "bsp-impl.h"

#include "generic_freertos_bsp_internal.h"

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

/* configASSERT's handler, declared by FreeRTOSConfig.h in a real build */
void OS_FreeRTOS_AssertFailed(const char *File, int Line);

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
extern jmp_buf                UT_Stub_HaltJump;
extern uint32_t               UT_Stub_PauseCount;
extern bool                   UT_Stub_PauseSignalsBlocked;
extern bool                   UT_Stub_PauseReturnsOnce;
extern char                   UT_Stub_Console[];
extern size_t                 UT_Stub_ConsoleLen;
extern uint32_t               UT_Stub_ConsoleInitCount;
extern size_t                 UT_Stub_ConsoleLenAtInit;
extern int                    UT_Stub_ConsoleInitResult;

/* What the stubbed storage functions report, declared in freertos-bsp-start-stubs.c */
extern lfs_size_t UT_Stub_StorageReadSize;
extern lfs_size_t UT_Stub_StorageProgSize;
extern lfs_size_t UT_Stub_StorageBlockSize;
extern lfs_size_t UT_Stub_StorageBlockCount;
extern lfs_size_t UT_Stub_StorageCacheSize;
extern lfs_size_t UT_Stub_StorageLookaheadSize;
extern void      *UT_Stub_StorageReadBuffer;
extern void      *UT_Stub_StorageProgBuffer;
extern void      *UT_Stub_StorageLookaheadBuffer;
void              UT_Stub_StorageReset(void);

/* The status the application sets from inside OS_Application_Run() */
static int32 AppExitCode;

/*
 * The fatal paths halt instead of returning, so a case that reaches one runs it
 * through UT_HALTING: the stand-in pause() in the stubs jumps back to the setjmp
 * here instead of sleeping for good. The halt masked every signal on this thread on
 * the way, so the mask is put back afterwards.
 */
#define UT_HALTING(Call)                                  \
    do                                                    \
    {                                                     \
        sigset_t Saved;                                   \
                                                          \
        (void)pthread_sigmask(SIG_SETMASK, NULL, &Saved); \
        if (setjmp(UT_Stub_HaltJump) == 0)                \
        {                                                 \
            Call;                                         \
        }                                                 \
        (void)pthread_sigmask(SIG_SETMASK, &Saved, NULL); \
    } while (0)

/* What UT_Main reports when the BSP halted instead of returning */
#define UT_HALTED (-1000)

/*
 * How the application ends depends on the port. One that implements
 * vPortEndScheduler defines OSAL_FREERTOS_PORT_ENDS_SCHEDULER and main() returns
 * the exit status once the BSP has ended the scheduler; one that does not (the
 * Cortex-M ports) leaves it undefined and the BSP reports the translated exit
 * status and halts, so main() never returns.
 */
#ifdef OSAL_FREERTOS_PORT_ENDS_SCHEDULER
#define UT_RETURNED(Status) (Status)
#else
#define UT_RETURNED(Status) UT_HALTED
#endif

/* Runs main() and returns what it returned, or UT_HALTED if it halted. The console starts empty. */
static int UT_Main(int argc, char *argv[])
{
    int Result = UT_HALTED;

    UT_Stub_ConsoleLen = 0;

    UT_HALTING(Result = OS_BSP_TestMain(argc, argv));

    return Result;
}

static void AssertConsole(const char *Expected)
{
    UtAssert_STRINGBUF_EQ(UT_Stub_Console, UT_Stub_ConsoleLen, Expected, strlen(Expected));
}

/* Either way, Count is how many times the application has ended so far */
static void AssertAppEnded(uint32 Count, int Status)
{
#ifdef OSAL_FREERTOS_PORT_ENDS_SCHEDULER
    (void)Status;

    UtAssert_STUB_COUNT(vTaskEndScheduler, Count);
    UtAssert_UINT32_EQ(UT_Stub_PauseCount, 0);
#else
    UtAssert_UINT32_EQ(UT_Stub_PauseCount, Count);
    UtAssert_STUB_COUNT(vTaskEndScheduler, 0);

    if (Count > 0)
    {
        char Report[40];

        (void)snprintf(Report, sizeof(Report), "APPLICATION EXIT %d\n", Status);
        AssertConsole(Report);
    }
#endif
}

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

    UtAssert_INT32_EQ(UT_Main(2, argv), UT_RETURNED(EXIT_SUCCESS));

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
    AssertAppEnded(1, EXIT_SUCCESS);
    UtAssert_INT32_EQ(OS_BSP_Global.AppStatus, OS_SUCCESS);
}

void Test_Main_AppExitStatus(void)
{
    /* the application's status wins over the default, and is translated into a process exit code */
    UT_SetHookFunction(UT_KEY(OS_Application_Run), SetAppExitCodeHook, NULL);

    AppExitCode = 7;
    UtAssert_INT32_EQ(UT_Main(0, NULL), UT_RETURNED(7));
    AssertAppEnded(1, 7);

    AppExitCode = OS_ERROR;
    UtAssert_INT32_EQ(UT_Main(0, NULL), UT_RETURNED(EXIT_FAILURE));
    AssertAppEnded(2, EXIT_FAILURE);

    /* only the low 7 bits pass through */
    AppExitCode = 0x85;
    UtAssert_INT32_EQ(UT_Main(0, NULL), UT_RETURNED(0x05));
    AssertAppEnded(3, 0x05);
}

void Test_Main_SchedulerFailsToStart(void)
{
    /* vTaskStartScheduler() returns at once, so the startup task never runs */
    UT_Stub_SchedulerStarts = false;

    UtAssert_INT32_EQ(UT_Main(0, NULL), EXIT_FAILURE);

    UtAssert_STUB_COUNT(vTaskStartScheduler, 1);
    UtAssert_STUB_COUNT(OS_Application_Startup, 0);
    UtAssert_STUB_COUNT(OS_Application_Run, 0);
    AssertAppEnded(0, 0);
    UtAssert_INT32_EQ(OS_BSP_Global.AppStatus, OS_ERROR);
}

void Test_Main_TaskCreateFails(void)
{
    UT_SetDefaultReturnValue(UT_KEY(xTaskCreate), -1);

    UtAssert_INT32_EQ(UT_Main(0, NULL), EXIT_FAILURE);

    /* without the startup task there is nothing for a scheduler to run */
    UtAssert_STUB_COUNT(vTaskStartScheduler, 0);
}

void Test_StackOverflowHook(void)
{
    /*
     * The kernel calls the hook from inside its task switch, so it must halt
     * and leave the scheduler alone: ending it there would delete the idle task
     * the switch is about to pick from.
     */
    UT_HALTING(vApplicationStackOverflowHook(NULL, "task"));

    AssertConsole("STACK OVERFLOW task\n");
    UtAssert_UINT32_EQ(UT_Stub_PauseCount, 1);
    UtAssert_True(UT_Stub_PauseSignalsBlocked, "signals masked before halting");
    UtAssert_STUB_COUNT(vTaskEndScheduler, 0);
    UtAssert_INT32_EQ(OS_BSP_Global.AppStatus, OS_SUCCESS);

    /* a second overflow while halting skips the report but still halts */
    UT_HALTING(vApplicationStackOverflowHook(NULL, "other"));
    AssertConsole("STACK OVERFLOW task\n");
    UtAssert_UINT32_EQ(UT_Stub_PauseCount, 2);
}

void Test_AssertFailed(void)
{
    /* reports the file and line, masks interrupts and halts */
    UT_HALTING(OS_FreeRTOS_AssertFailed("file.c", 12345));
    AssertConsole("ASSERT file.c:12345\n");
    UtAssert_UINT32_EQ(UT_Stub_PauseCount, 1);
    UtAssert_True(UT_Stub_PauseSignalsBlocked, "signals masked before halting");

    /* a fatal error raised while reporting another skips the report but still halts */
    UT_HALTING(OS_FreeRTOS_AssertFailed("other.c", 1));
    AssertConsole("ASSERT file.c:12345\n");
    UtAssert_UINT32_EQ(UT_Stub_PauseCount, 2);

    /* line zero is one digit */
    OS_BSP_GenericFreeRTOSGlobal.Halting = false;
    UT_Stub_ConsoleLen                   = 0;
    UT_HALTING(OS_FreeRTOS_AssertFailed("f", 0));
    AssertConsole("ASSERT f:0\n");

    /* a negative line is reported as zero */
    OS_BSP_GenericFreeRTOSGlobal.Halting = false;
    UT_Stub_ConsoleLen                   = 0;
    UT_HALTING(OS_FreeRTOS_AssertFailed("f", -5));
    AssertConsole("ASSERT f:0\n");
}

void Test_HaltKeepsSleeping(void)
{
    /* a sleep that ends is not the end of the halt: it goes back to sleep */
    UT_Stub_PauseReturnsOnce = true;

    UT_HALTING(OS_FreeRTOS_AssertFailed("file.c", 1));

    UtAssert_UINT32_EQ(UT_Stub_PauseCount, 2);
    UtAssert_True(!UT_Stub_PauseReturnsOnce, "the first sleep returned");
}

void Test_Shutdown_Impl(void)
{
    /* the status handed to the exit is the translated one the application set */
    OS_BSP_SetExitCode(3);

    UT_HALTING(OS_BSP_Shutdown_Impl());

    AssertAppEnded(1, 3);

#ifndef OSAL_FREERTOS_PORT_ENDS_SCHEDULER
    /* a second exit while halting skips the report but still halts */
    UT_HALTING(OS_BSP_Shutdown_Impl());

    AssertAppEnded(2, 3);
#endif
}

void Test_Lock_Unlock(void)
{
    /* main() creates the console mutex that the lock functions use */
    UT_Main(0, NULL);

    OS_BSP_Lock_Impl();
    UtAssert_True(UT_Stub_LastTakeSem == (SemaphoreHandle_t)UT_Stub_LastCreateMutexBuffer,
                  "lock takes the console mutex");
    UtAssert_True(UT_Stub_LastTakeTicks == portMAX_DELAY, "lock waits forever");

    OS_BSP_Unlock_Impl();
    UtAssert_True(UT_Stub_LastGiveSem == (SemaphoreHandle_t)UT_Stub_LastCreateMutexBuffer,
                  "unlock gives the console mutex");
}

void Test_Main_StorageConfig(void)
{
    const struct lfs_config *Config;

    UT_Main(0, NULL);

    Config = OS_BSP_GenericFreeRTOSGlobal.StorageConfig;
    UtAssert_True(Config != NULL, "main sets up the storage configuration");

    UtAssert_True(Config->read == OS_BSP_Storage_Read, "read callback");
    UtAssert_True(Config->prog == OS_BSP_Storage_Prog, "prog callback");
    UtAssert_True(Config->erase == OS_BSP_Storage_Erase, "erase callback");
    UtAssert_True(Config->sync == OS_BSP_Storage_Sync, "sync callback");

    UtAssert_UINT32_EQ(Config->read_size, OS_BSP_Storage_ReadSize());
    UtAssert_UINT32_EQ(Config->prog_size, OS_BSP_Storage_ProgSize());
    UtAssert_UINT32_EQ(Config->block_size, OS_BSP_Storage_BlockSize());
    UtAssert_UINT32_EQ(Config->block_count, OS_BSP_Storage_BlockCount());
    UtAssert_INT32_EQ(Config->block_cycles, OS_BSP_Storage_BlockCycles());
    UtAssert_UINT32_EQ(Config->cache_size, OS_BSP_Storage_CacheSize());
    UtAssert_UINT32_EQ(Config->lookahead_size, OS_BSP_Storage_LookaheadSize());

    UtAssert_True(Config->read_buffer == OS_BSP_Storage_ReadBuffer(), "read buffer");
    UtAssert_True(Config->prog_buffer == OS_BSP_Storage_ProgBuffer(), "prog buffer");
    UtAssert_True(Config->lookahead_buffer == OS_BSP_Storage_LookaheadBuffer(), "lookahead buffer");
}

/*
 * Each case breaks the first littlefs rule that main() checks for it, with every
 * rule before it kept: the default geometry is 16/16/256/12/128/16 (see
 * UT_Stub_StorageReset)
 */
void Test_Main_StorageConfigInvalid(void)
{
    static const struct
    {
        lfs_size_t  ReadSize;
        lfs_size_t  ProgSize;
        lfs_size_t  BlockSize;
        lfs_size_t  BlockCount;
        lfs_size_t  CacheSize;
        lfs_size_t  LookaheadSize;
        const char *Broken;
    } Cases[] = {
        { 0,  16, 256, 12, 128, 16, "read size is zero"                             },
        { 16, 0,  256, 12, 128, 16, "prog size is zero"                             },
        { 16, 16, 0,   12, 128, 16, "block size is zero"                            },
        { 16, 16, 256, 0,  128, 16, "block count is zero"                           },
        { 16, 16, 256, 12, 0,   16, "cache size is zero"                            },
        { 16, 16, 256, 12, 128, 0,  "lookahead size is zero"                        },
        { 3,  16, 256, 12, 128, 16, "block size is not a multiple of the read size" },
        { 16, 3,  256, 12, 128, 16, "block size is not a multiple of the prog size" },
        { 32, 16, 96,  12, 48,  16, "cache size is not a multiple of the read size" },
        { 16, 32, 96,  12, 48,  16, "cache size is not a multiple of the prog size" },
        { 16, 16, 96,  12, 64,  16, "cache size does not divide the block size"     },
        { 16, 16, 256, 12, 128, 12, "lookahead size is not a multiple of 8"         },
    };
    size_t Index;

    for (Index = 0; Index < sizeof(Cases) / sizeof(Cases[0]); Index++)
    {
        UT_Stub_StorageReset();
        UT_Stub_StorageReadSize      = Cases[Index].ReadSize;
        UT_Stub_StorageProgSize      = Cases[Index].ProgSize;
        UT_Stub_StorageBlockSize     = Cases[Index].BlockSize;
        UT_Stub_StorageBlockCount    = Cases[Index].BlockCount;
        UT_Stub_StorageCacheSize     = Cases[Index].CacheSize;
        UT_Stub_StorageLookaheadSize = Cases[Index].LookaheadSize;

        UtAssert_True(UT_Main(0, NULL) == EXIT_FAILURE, "main fails when the %s", Cases[Index].Broken);
        AssertConsole("STORAGE CONFIG INVALID\n");
    }

    /* A buffer the vendor did not supply */
    UT_Stub_StorageReset();
    UT_Stub_StorageReadBuffer = NULL;
    UtAssert_INT32_EQ(UT_Main(0, NULL), EXIT_FAILURE);
    AssertConsole("STORAGE CONFIG INVALID\n");

    UT_Stub_StorageReset();
    UT_Stub_StorageProgBuffer = NULL;
    UtAssert_INT32_EQ(UT_Main(0, NULL), EXIT_FAILURE);
    AssertConsole("STORAGE CONFIG INVALID\n");

    UT_Stub_StorageReset();
    UT_Stub_StorageLookaheadBuffer = NULL;
    UtAssert_INT32_EQ(UT_Main(0, NULL), EXIT_FAILURE);
    AssertConsole("STORAGE CONFIG INVALID\n");

    /* Failing before the task is created means the scheduler never starts */
    UtAssert_STUB_COUNT(vTaskStartScheduler, 0);
}

void Test_Main_ConsoleInit(void)
{
    /* The report of an invalid storage configuration is the first thing main() writes to the console */
    UT_Stub_StorageBlockSize = 0;
    UtAssert_INT32_EQ(UT_Main(0, NULL), EXIT_FAILURE);
    AssertConsole("STORAGE CONFIG INVALID\n");

    UtAssert_UINT32_EQ(UT_Stub_ConsoleInitCount, 1);
    UtAssert_True(UT_Stub_ConsoleLenAtInit == 0, "the console device is started before anything is written to it");

    /* A normal start starts it too, once */
    UT_Stub_StorageReset();
    UT_Stub_ConsoleInitCount = 0;
    UtAssert_INT32_EQ(UT_Main(0, NULL), UT_RETURNED(EXIT_SUCCESS));
    UtAssert_UINT32_EQ(UT_Stub_ConsoleInitCount, 1);
}

void Test_Main_ConsoleInitFails(void)
{
    /* A console that fails its check stops the boot after reporting the code, before anything else is set up */
    UT_Stub_ConsoleInitResult = OS_BSP_CONSOLE_ERR_RANGE;
    UtAssert_INT32_EQ(UT_Main(0, NULL), EXIT_FAILURE);
    AssertConsole("CONSOLE INIT FAILED 3\n");

    UtAssert_UINT32_EQ(UT_Stub_ConsoleInitCount, 1);
    UtAssert_True(UT_Stub_ConsoleLenAtInit == 0, "the console is checked before anything is reported");
    UtAssert_STUB_COUNT(xTaskCreate, 0);
    UtAssert_STUB_COUNT(vTaskStartScheduler, 0);
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
    UT_Stub_StorageReset();
    memset(&OS_BSP_Global, 0, sizeof(OS_BSP_Global));
    UT_Stub_StartupTask                  = NULL;
    UT_Stub_LastCreateName               = NULL;
    UT_Stub_LastCreateStackDepth         = 0;
    UT_Stub_LastCreatePriority           = 0;
    UT_Stub_LastCreateMutexBuffer        = NULL;
    UT_Stub_LastTakeSem                  = NULL;
    UT_Stub_LastTakeTicks                = 0;
    UT_Stub_LastGiveSem                  = NULL;
    UT_Stub_SchedulerStarts              = true;
    UT_Stub_PauseCount                   = 0;
    UT_Stub_PauseSignalsBlocked          = false;
    UT_Stub_ConsoleLen                   = 0;
    UT_Stub_ConsoleInitCount             = 0;
    UT_Stub_ConsoleLenAtInit             = SIZE_MAX;
    UT_Stub_ConsoleInitResult            = 0;
    OS_BSP_GenericFreeRTOSGlobal.Halting = false;
    AppExitCode                          = 0;
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
    ADD_TEST(Main_StorageConfig);
    ADD_TEST(Main_StorageConfigInvalid);
    ADD_TEST(Main_ConsoleInit);
    ADD_TEST(Main_ConsoleInitFails);
    ADD_TEST(StackOverflowHook);
    ADD_TEST(AssertFailed);
    ADD_TEST(HaltKeepsSleeping);
    ADD_TEST(Shutdown_Impl);
    ADD_TEST(Lock_Unlock);
    ADD_TEST(StaticAllocationHooks);
}
