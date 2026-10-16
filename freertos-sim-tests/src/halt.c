/*
 * File:  halt.c
 *
 * Purpose:
 *   Firmware that ends in the BSP's halt, one way per HALT_CASE_* definition:
 *
 *     ASSERT    configASSERT fails
 *     OVERFLOW  a task overflows its stack
 *     EXIT      the application returns (Cortex-M only: the POSIX port ends the
 *               scheduler and the process exits instead of halting)
 *     BUSY      never halts, it spins. This is the control: the harness
 *               measures the CPU the emulator uses, and BUSY is what a halt
 *               loop that does not sleep would look like
 *
 *   The halt must sleep the core rather than spin (OS_BSP_HaltWait), which is
 *   seen from outside by how much CPU the emulator uses once the firmware has
 *   halted, see gdb/run_halt_test.py and gdb/run_halt_test_posix.py.
 */

#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "osapi-bsp.h"
#include "osapi-common.h"
#include "osapi-error.h"

#if defined(HALT_CASE_OVERFLOW)

#define HALT_TASK_PRIORITY ((configMAX_PRIORITIES / 2) + 1)

// The parameter must stay non-const to match FreeRTOS's TaskFunction_t signature
// cppcheck-suppress constParameterCallback
static void Halt_OverflowTask(void *Parameter)
{
    volatile uint8_t Buffer[configMINIMAL_STACK_SIZE * sizeof(StackType_t)];
    size_t           i;

    (void)Parameter;

    /* A buffer as big as the whole stack, so the task is past its limit */
    for (i = 0; i < sizeof(Buffer); ++i)
    {
        Buffer[i] = 0xA5U;
    }

    /* The kernel checks the stack when it switches away from the task */
    vTaskDelay(1);
    vTaskDelay(1);

    for (;;)
    {
    }
}

#endif

void OS_Application_Startup(void)
{
#if defined(HALT_CASE_ASSERT)
    volatile uint32_t Zero = 0U;

    configASSERT(Zero != 0U);
#elif defined(HALT_CASE_OVERFLOW)
    (void)xTaskCreate(Halt_OverflowTask, "overflow", configMINIMAL_STACK_SIZE, NULL, HALT_TASK_PRIORITY, NULL);
    vTaskDelay(portMAX_DELAY);
#elif defined(HALT_CASE_BUSY)
    for (;;)
    {
    }
#elif defined(HALT_CASE_EXIT)
    OS_BSP_SetExitCode(OS_SUCCESS);
#else
#error "halt.c needs one of HALT_CASE_ASSERT, HALT_CASE_OVERFLOW, HALT_CASE_EXIT or HALT_CASE_BUSY"
#endif
}

/* Nothing runs in the background: returning ends the application */
void OS_Application_Run(void)
{
}
