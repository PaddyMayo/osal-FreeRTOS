/*
 * File:  bsp_console.c
 *
 * Purpose:
 *   OSAL BSP debug console implementation, shared across every FreeRTOS
 *   BSP.
 *
 *   TODO: no console backend yet, OS_BSP_ConsoleOutput_Impl and
 *   OS_BSP_ConsoleSetMode_Impl are templates.
 */

#include <stdbool.h>
#include <string.h>

#include "FreeRTOS.h"

#include "bsp-impl.h"

/*----------------------------------------------------------------
   OS_BSP_ConsoleOutput_Impl
   See full description in header
 ------------------------------------------------------------------*/
void OS_BSP_ConsoleOutput_Impl(const char *Str, size_t DataLen)
{
    /* TODO: write DataLen bytes of Str to the console */
}

/*----------------------------------------------------------------
   OS_BSP_ConsoleSetMode_Impl
   See full description in header
 ------------------------------------------------------------------*/
void OS_BSP_ConsoleSetMode_Impl(uint32 ModeBits)
{
    /* TODO: console color/attribute control, if the console supports it */
}

/*----------------------------------------------------------------
   OS_BSP_MaskAllInterrupts

   Stops every maskable interrupt (on a POSIX host, every signal) on the
   calling CPU/thread, and is never undone. taskDISABLE_INTERRUPTS() is not
   used because each FreeRTOS port defines it differently: the ARMv7-M and
   later ports only raise BASEPRI, which leaves higher-priority interrupts
   running.

   The port's family comes from the macro it names in
   generic-freertos/config/CMakeLists.txt. A port that names neither family
   fails the build, so a port can never silently mask less than the others.
   NMI and HardFault cannot be masked on any ARM core: the halt loop cannot
   cause a HardFault, and each ARM platform should point both vectors at a
   spin loop in its startup code.
 ------------------------------------------------------------------*/
#if defined(OSAL_FREERTOS_PORT_CORTEX_M)

static inline void OS_BSP_MaskAllInterrupts(void)
{
    /* PRIMASK masks every configurable interrupt, whatever its priority */
    __asm volatile("cpsid i" ::: "memory");
}

#elif defined(OSAL_FREERTOS_PORT_POSIX)

#include <pthread.h>
#include <signal.h>

static inline void OS_BSP_MaskAllInterrupts(void)
{
    sigset_t AllSignals;

    /* The GCC_POSIX port's tick is SIGALRM, so blocking every signal on this thread stops it */
    (void)sigfillset(&AllSignals);
    (void)pthread_sigmask(SIG_BLOCK, &AllSignals, NULL);
}

#else
#error "OS_BSP_MaskAllInterrupts: unsupported architecture"
#endif

/*----------------------------------------------------------------
   OS_BSP_ReportString / OS_BSP_ReportNumber

   Write to the console through OS_BSP_ConsoleOutput_Impl only: no
   OS_printf, no console lock and no heap, since the assert handler can run
   from an ISR, a critical section or the middle of a semaphore operation.
 ------------------------------------------------------------------*/
static inline void OS_BSP_ReportString(const char *Str)
{
    OS_BSP_ConsoleOutput_Impl(Str, strlen(Str));
}

static inline void OS_BSP_ReportNumber(int Number)
{
    char         Digits[11];
    size_t       Pos   = sizeof(Digits);
    unsigned int Value = (Number < 0) ? 0U : (unsigned int)Number;

    do
    {
        Pos--;
        Digits[Pos]  = (char)('0' + (Value % 10U));
        Value       /= 10U;
    } while (Value != 0U);

    OS_BSP_ConsoleOutput_Impl(&Digits[Pos], sizeof(Digits) - Pos);
}

/*----------------------------------------------------------------
   OS_BSP_BeginHalt / OS_BSP_Halt

   Shared by every fatal path. BeginHalt masks interrupts and reports whether
   this is the first fatal error: one raised while reporting another skips the
   report and goes straight to Halt, so the halt is always reached. The flag is
   set before anything is reported.
 ------------------------------------------------------------------*/
static inline bool OS_BSP_BeginHalt(void)
{
    static volatile bool Halting = false;
    bool                 First;

    OS_BSP_MaskAllInterrupts();

    First   = !Halting;
    Halting = true;

    return First;
}

static inline void OS_BSP_Halt(void)
{
    for (;;)
    {
    }
}

/*----------------------------------------------------------------
   OS_FreeRTOS_AssertFailed

   Called by configASSERT (see FreeRTOSConfig.h). Masks interrupts, reports
   File:Line and halts. Never returns.

   TODO: untested. The masking branches (each ARM architecture and POSIX), the
   reported text and the recursion guard need a testing strategy that covers
   every supported target, not only a POSIX host.
 ------------------------------------------------------------------*/
void OS_FreeRTOS_AssertFailed(const char *File, int Line)
{
    if (OS_BSP_BeginHalt())
    {
        OS_BSP_ReportString("ASSERT ");
        OS_BSP_ReportString(File);
        OS_BSP_ReportString(":");
        OS_BSP_ReportNumber(Line);
        OS_BSP_ReportString("\n");
    }

    OS_BSP_Halt();
}

/*----------------------------------------------------------------
   OS_FreeRTOS_StackOverflow

   Called by vApplicationStackOverflowHook (bsp_start.c), which the kernel
   calls from inside its task switch. The kernel's state cannot be changed
   there, so it masks interrupts, reports the task and halts. Never returns.

   TODO: untested, see OS_FreeRTOS_AssertFailed.
 ------------------------------------------------------------------*/
void OS_FreeRTOS_StackOverflow(const char *TaskName)
{
    if (OS_BSP_BeginHalt())
    {
        OS_BSP_ReportString("STACK OVERFLOW ");
        OS_BSP_ReportString(TaskName);
        OS_BSP_ReportString("\n");
    }

    OS_BSP_Halt();
}

/*----------------------------------------------------------------
   OS_FreeRTOS_ApplicationExit

   Called by OS_BSP_Shutdown_Impl (bsp_start.c) on ports that cannot end the
   scheduler (the Cortex-M ports assert in vPortEndScheduler), so there is
   nothing to return to. Masks interrupts, reports the exit status (already
   translated to 0..127) and halts. Never returns.

   TODO: untested, see OS_FreeRTOS_AssertFailed.
 ------------------------------------------------------------------*/
void OS_FreeRTOS_ApplicationExit(int Status)
{
    if (OS_BSP_BeginHalt())
    {
        OS_BSP_ReportString("APPLICATION EXIT ");
        OS_BSP_ReportNumber(Status);
        OS_BSP_ReportString("\n");
    }

    OS_BSP_Halt();
}
