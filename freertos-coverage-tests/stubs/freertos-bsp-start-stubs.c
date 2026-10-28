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

#include <setjmp.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <pthread.h>

#include "utstubs.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "osapi-common.h"

#include "lfs.h"
#include "bsp_storage.h"

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

/*
 * The fatal paths in bsp_start.c halt and never return: they mask every signal and
 * then sleep in pause() for good. This stands in for pause() in the test
 * executable, which the halt loop calls in place of the C library's, so a test can
 * run a fatal path and get control back: it records that the BSP halted, whether the
 * signals were masked by then, and jumps back to where the test set UT_Stub_HaltJump.
 */
jmp_buf  UT_Stub_HaltJump;
uint32_t UT_Stub_PauseCount;
bool     UT_Stub_PauseSignalsBlocked;

/* A real pause() returns when a signal is handled; this makes the next call do so, once */
bool UT_Stub_PauseReturnsOnce;

int pause(void)
{
    sigset_t Current;

    ++UT_Stub_PauseCount;

    (void)pthread_sigmask(SIG_SETMASK, NULL, &Current);
    UT_Stub_PauseSignalsBlocked = (sigismember(&Current, SIGALRM) == 1) && (sigismember(&Current, SIGUSR1) == 1);

    if (UT_Stub_PauseReturnsOnce)
    {
        UT_Stub_PauseReturnsOnce = false;

        return -1;
    }

    longjmp(UT_Stub_HaltJump, 1);
}

/* The console is the only way a fatal path reports, so what it writes is kept for the tests */
char   UT_Stub_Console[256];
size_t UT_Stub_ConsoleLen;

void OS_BSP_ConsoleOutput_Impl(const char *Str, size_t DataLen)
{
    if (DataLen > sizeof(UT_Stub_Console) - UT_Stub_ConsoleLen)
    {
        DataLen = sizeof(UT_Stub_Console) - UT_Stub_ConsoleLen;
    }

    memcpy(&UT_Stub_Console[UT_Stub_ConsoleLen], Str, DataLen);
    UT_Stub_ConsoleLen += DataLen;
}

/* main() starts the vendor's console device before anything is written to it, so how much it had written by then is
 * kept */
uint32_t UT_Stub_ConsoleInitCount;
size_t   UT_Stub_ConsoleLenAtInit;

/* What the stubbed OS_BSP_Console_DeviceInit returns, 0 when the console is usable */
int UT_Stub_ConsoleInitResult;

int OS_BSP_Console_DeviceInit(void)
{
    UT_Stub_ConsoleInitCount++;
    UT_Stub_ConsoleLenAtInit = UT_Stub_ConsoleLen;

    return UT_Stub_ConsoleInitResult;
}

/*
 * The vendor's storage callbacks, which main() puts in the littlefs configuration,
 * and the buffers its bsp_storage.h names. The callbacks are never called here, so
 * they only return success. The geometry is the macros of the bsp_storage.h that
 * the build picks up (platforms/common/storage-ram).
 */
uint8_t OS_BSP_StorageReadBuffer[OS_BSP_STORAGE_CACHE_SIZE];
uint8_t OS_BSP_StorageProgBuffer[OS_BSP_STORAGE_CACHE_SIZE];
uint8_t OS_BSP_StorageLookaheadBuffer[OS_BSP_STORAGE_LOOKAHEAD_SIZE];

// buffer must stay non-const to match the vendor's read function
// cppcheck-suppress constParameterPointer
int OS_BSP_Storage_Read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size)
{
    return 0;
}

int OS_BSP_Storage_Prog(const struct lfs_config *c,
                        lfs_block_t              block,
                        lfs_off_t                off,
                        const void              *buffer,
                        lfs_size_t               size)
{
    return 0;
}

int OS_BSP_Storage_Erase(const struct lfs_config *c, lfs_block_t block)
{
    return 0;
}

int OS_BSP_Storage_Sync(const struct lfs_config *c)
{
    return 0;
}

void OS_Application_Startup(void)
{
    UT_DEFAULT_IMPL(OS_Application_Startup);
}

void OS_Application_Run(void)
{
    UT_DEFAULT_IMPL(OS_Application_Run);
}
