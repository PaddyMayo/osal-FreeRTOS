/*
 * File: firmware-handlers.c
 *
 * Purpose:
 *   What the test runners need to be Cortex-M firmware: default exception
 *   handlers, and the point where a run ends.
 *
 *   The shared vector table (platforms/common/cortex_m_startup.c) names the
 *   FreeRTOS port's SVC, PendSV and SysTick handlers. In the product the port
 *   supplies them. These runners link no kernel, only fakes, so something has to
 *   stand in, and the runners never start the scheduler or a timer, so none of
 *   the three can fire. The names follow the same rule as the vector table.
 */

#include "firmware.h"

#if defined(__ARM_ARCH_8M_BASE__) || defined(__ARM_ARCH_8M_MAIN__)
#define FIRMWARE_SVC_HANDLER     SVC_Handler
#define FIRMWARE_PENDSV_HANDLER  PendSV_Handler
#define FIRMWARE_SYSTICK_HANDLER SysTick_Handler
#else
#define FIRMWARE_SVC_HANDLER     vPortSVCHandler
#define FIRMWARE_PENDSV_HANDLER  xPortPendSVHandler
#define FIRMWARE_SYSTICK_HANDLER xPortSysTickHandler
#endif

void FIRMWARE_SVC_HANDLER(void);
void FIRMWARE_PENDSV_HANDLER(void);
void FIRMWARE_SYSTICK_HANDLER(void);

/* Spin, so a debugger stops here if one of them ever does fire */
void FIRMWARE_SVC_HANDLER(void)
{
    for (;;)
    {
    }
}

void FIRMWARE_PENDSV_HANDLER(void)
{
    for (;;)
    {
    }
}

void FIRMWARE_SYSTICK_HANDLER(void)
{
    for (;;)
    {
    }
}

/* Not inlined, so the debugger has an address to stop at, with the status still in r0 */
__attribute__((noinline)) void CoverageTestExit(int Status)
{
    (void)Status;

    for (;;)
    {
    }
}
