/*
 * File:  task_reuse.c
 *
 * Purpose:
 *   Task slot and TCB reuse. A task that exits gives its OSAL table slot back
 *   at once, but the kernel frees its TCB later. The port has the timer task
 *   do that for a task that exits (OS_FreeRTOS_DeleteSelf), because the idle
 *   task, which the kernel leaves it to, can be kept off the CPU by a task that
 *   never blocks. So this checks that creating tasks right after others have
 *   gone never fails and never hangs, however the old ones went:
 *
 *     exit     the task returns through OS_TaskExit()
 *     self     the task deletes itself with OS_TaskDelete(), delete hook runs
 *     other    another task deletes it, the kernel frees the TCB at once
 *     full     every slot is taken, so a further create must fail cleanly
 *     starved  the idle task never runs, and creates keep succeeding
 *
 *   The exiting tasks run above the startup task so they are gone before the
 *   next create, and the startup task never lets the idle task run in between:
 *   if an exited task's TCB waited for idle, the pool would be empty after
 *   OS_MAX_TASKS creates and the next one would time out.
 *
 *   A hang is not reported by the firmware, the harness times the run out.
 *   Otherwise the exit status is the number of the first check that failed,
 *   1 if OSAL did not initialise.
 */

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

#include "osapi.h"
#include "osapi-bsp.h"

/*
 * Every task not yet cleaned up still holds its heap stack, and the churn
 * leaves OS_MAX_TASKS of them waiting at once, so the stack is the smallest
 * that works: OS_MAX_TASKS of them must fit the platform's heap.
 */
#define REUSE_STACK_SIZE (configMINIMAL_STACK_SIZE * sizeof(StackType_t))
#define REUSE_ROUNDS     (3U * OS_MAX_TASKS)
#define REUSE_WAIT_MS    10U
#define REUSE_TIMEOUT_MS 2000U
#define REUSE_FAST       1 /* OSAL priority, above the startup task */

/* The check that failed first, 0 for none */
static int Failed;

/* Written by the tasks, read by the startup task */
static volatile uint32 TasksFinished;
static volatile uint32 DeleteHookCalls;
static volatile uint32 TasksParked;

static void Reuse_Check(bool Passed, int Number)
{
    if (!Passed && Failed == 0)
    {
        Failed = Number;
    }
}

static bool Reuse_WaitFor(const volatile uint32 *Counter, uint32 Target)
{
    uint32 Waited = 0;

    while (*Counter < Target && Waited < REUSE_TIMEOUT_MS)
    {
        (void)OS_TaskDelay(REUSE_WAIT_MS);
        Waited += REUSE_WAIT_MS;
    }

    return *Counter >= Target;
}

/*
 * Wait for a counter without sleeping. Sleeping would let the idle task run
 * and free the TCBs, which is what the churn must not wait for. Yielding hands
 * the CPU only to tasks of the same or higher priority, never to idle.
 */
static bool Reuse_SpinFor(const volatile uint32 *Counter, uint32 Target)
{
    TickType_t Start = xTaskGetTickCount();

    while (*Counter < Target && (xTaskGetTickCount() - Start) < pdMS_TO_TICKS(REUSE_TIMEOUT_MS))
    {
        taskYIELD();
    }

    return *Counter >= Target;
}

static void Reuse_ExitTask(void)
{
    TasksFinished++;
    OS_TaskExit();
}

static void Reuse_DeleteHook(void)
{
    DeleteHookCalls++;
}

static void Reuse_SelfDeleteTask(void)
{
    (void)OS_TaskInstallDeleteHandler(Reuse_DeleteHook);
    TasksFinished++;
    (void)OS_TaskDelete(OS_TaskGetId());
}

/* Stays alive until something deletes it */
static void Reuse_ParkedTask(void)
{
    TasksParked++;
    for (;;)
    {
        (void)OS_TaskDelay(1000U);
    }
}

static int32 Reuse_Create(osal_id_t *Id, uint32 Index, osal_task_entry Entry, osal_priority_t Priority)
{
    /* "reuse" and the index in decimal, by hand: snprintf would pull the heap into newlib */
    char   Name[OS_MAX_API_NAME] = "reuse";
    char   Digits[10];
    uint32 Count = 0;
    uint32 Len   = 5;

    do
    {
        Digits[Count++]  = (char)('0' + (Index % 10U));
        Index           /= 10U;
    } while (Index != 0U && Count < sizeof(Digits));

    while (Count > 0U && Len < sizeof(Name) - 1U)
    {
        Name[Len++] = Digits[--Count];
    }
    Name[Len] = '\0';

    return OS_TaskCreate(Id, Name, Entry, OSAL_TASK_STACK_ALLOCATE, REUSE_STACK_SIZE, Priority, 0);
}

/*
 * Create task after task that goes away on its own, each create as soon as the
 * last has gone. A new OSAL task waits for its creator to finish registering
 * it before it runs, so it takes a while to exit; creating faster than that
 * would just fill the table. Waiting for each one to go (without letting idle
 * run) leaves its slot free, and its TCB must come back without idle's help,
 * or after OS_MAX_TASKS rounds a create finds the pool empty and times out.
 */
static void Reuse_Churn(osal_task_entry Entry, uint32 Rounds, int FirstCheck)
{
    osal_id_t Id;
    uint32    Round;
    uint32    Created = 0;

    TasksFinished = 0;

    for (Round = 0; Round < Rounds; ++Round)
    {
        if (Reuse_Create(&Id, Round, Entry, REUSE_FAST) == OS_SUCCESS)
        {
            ++Created;
            Reuse_Check(Reuse_SpinFor(&TasksFinished, Created), FirstCheck + 1);
        }
    }

    Reuse_Check(Created == Rounds, FirstCheck);
}

/* Never blocks, so the idle task never runs while it is the highest task that can */
// The parameter must stay non-const to match FreeRTOS's TaskFunction_t signature
// cppcheck-suppress constParameterCallback
static void Reuse_SpinnerTask(void *Parameter)
{
    (void)Parameter;

    for (;;)
    {
    }
}

/*
 * A task that never blocks and runs above the idle task keeps it off the CPU,
 * and the kernel leaves the freeing of a task that deleted itself to idle. The
 * port has the timer task do it instead, so more tasks than the pool holds can
 * come and go, back to back, without idle ever running, and every create works.
 */
static void Reuse_StarvedIdle(void)
{
    TaskHandle_t Spinner = NULL;

    /* Above idle, below this task: it takes the CPU whenever this task sleeps, so idle never gets it */
    Reuse_Check(xTaskCreate(Reuse_SpinnerTask, "spin", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, &Spinner)
                    == pdPASS,
                50);

    Reuse_Churn(Reuse_ExitTask, 2U * OS_MAX_TASKS, 51);

    if (Spinner != NULL)
    {
        vTaskDelete(Spinner);
    }
}

/* Fill every slot with a task that stays, see a further create refused, then delete them */
static void Reuse_FullAndDeleteOther(void)
{
    osal_id_t Ids[OS_MAX_TASKS];
    osal_id_t Extra;
    uint32    i;
    uint32    Created = 0;

    TasksParked = 0;

    for (i = 0; i < OS_MAX_TASKS; ++i)
    {
        if (Reuse_Create(&Ids[i], i, Reuse_ParkedTask, REUSE_FAST) == OS_SUCCESS)
        {
            ++Created;
        }
    }

    Reuse_Check(Created == OS_MAX_TASKS, 40);
    Reuse_Check(Reuse_WaitFor(&TasksParked, Created), 41);

    /* Nothing is free, and the TCB pool must not turn that into a wait */
    Reuse_Check(Reuse_Create(&Extra, OS_MAX_TASKS, Reuse_ExitTask, REUSE_FAST) == OS_ERR_NO_FREE_IDS, 42);

    /* Delete them all, then fill the table again straight away */
    for (i = 0; i < Created; ++i)
    {
        Reuse_Check(OS_TaskDelete(Ids[i]) == OS_SUCCESS, 43);
    }

    Created = 0;
    for (i = 0; i < OS_MAX_TASKS; ++i)
    {
        if (Reuse_Create(&Ids[i], i, Reuse_ParkedTask, REUSE_FAST) == OS_SUCCESS)
        {
            ++Created;
        }
    }

    Reuse_Check(Created == OS_MAX_TASKS, 44);

    for (i = 0; i < Created; ++i)
    {
        (void)OS_TaskDelete(Ids[i]);
    }
}

void OS_Application_Startup(void)
{
    Failed = 0;

    /* The BSP leaves OSAL to the application, as OSAL's default startup does */
    if (OS_API_Init() != OS_SUCCESS)
    {
        OS_BSP_SetExitCode(1);
        return;
    }

    Reuse_Churn(Reuse_ExitTask, REUSE_ROUNDS, 10);

    DeleteHookCalls = 0;
    Reuse_Churn(Reuse_SelfDeleteTask, REUSE_ROUNDS, 20);
    Reuse_Check(DeleteHookCalls == REUSE_ROUNDS, 30);

    Reuse_StarvedIdle();
    Reuse_FullAndDeleteOther();

    OS_BSP_SetExitCode(Failed == 0 ? OS_SUCCESS : Failed);
}

/* Nothing runs in the background: returning ends the application */
void OS_Application_Run(void)
{
}
