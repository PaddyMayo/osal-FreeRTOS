/**
 * \file
 *
 * Purpose: Startup code shared by every Cortex-M platform: vector table, reset
 * handler and the fault handlers. A platform names it in OSAL_FREERTOS_SOURCES.
 *
 * One vector table serves ARMv6-M to ARMv8-M. The configurable fault vectors
 * (MemManage, BusFault, UsageFault) stay empty: those faults are not enabled,
 * so they escalate to HardFault. The fault handlers spin, as the comment on
 * OS_BSP_MaskAllInterrupts in bsp_console.c asks, so a debugger can stop on
 * HardFault_Handler and read the faulting frame.
 */

#include <stdint.h>

extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;
extern uint32_t _estack;

/* The BSP entry point, see generic-freertos/src/bsp_start.c */
extern int main(int argc, char *argv[]);

/*
 * The FreeRTOS port's handlers. The Armv6-M and Armv8-M ports define them under
 * their standard names, the Armv7-M ports under FreeRTOS's own.
 */
#if defined(__ARM_ARCH_6M__) || defined(__ARM_ARCH_8M_BASE__) || defined(__ARM_ARCH_8M_MAIN__)
#define OSAL_SVC_HANDLER     SVC_Handler
#define OSAL_PENDSV_HANDLER  PendSV_Handler
#define OSAL_SYSTICK_HANDLER SysTick_Handler
#else
#define OSAL_SVC_HANDLER     vPortSVCHandler
#define OSAL_PENDSV_HANDLER  xPortPendSVHandler
#define OSAL_SYSTICK_HANDLER xPortSysTickHandler
#endif

extern void OSAL_SVC_HANDLER(void);
extern void OSAL_PENDSV_HANDLER(void);
extern void OSAL_SYSTICK_HANDLER(void);

void Reset_Handler(void);
void NMI_Handler(void);
void HardFault_Handler(void);

/* Coprocessor access control register, enables the FPU on cores that have one */
#define CPACR (*(volatile uint32_t *)0xE000ED88UL)

// The table is read by the core, never by C, and the linker script keeps its section
// cppcheck-suppress unusedStructMember
__attribute__((section(".isr_vector"), used)) void (*const OSAL_VectorTable[])(void) = {
    (void (*)(void)) & _estack, /* initial stack pointer */
    Reset_Handler,              /* reset */
    NMI_Handler,                /* NMI */
    HardFault_Handler,          /* HardFault */
    0,                          /* MemManage */
    0,                          /* BusFault */
    0,                          /* UsageFault */
    0,                          /* SecureFault (ARMv8-M) or reserved */
    0,                          /* reserved */
    0,                          /* reserved */
    0,                          /* reserved */
    OSAL_SVC_HANDLER,           /* SVCall */
    0,                          /* DebugMonitor */
    0,                          /* reserved */
    OSAL_PENDSV_HANDLER,        /* PendSV */
    OSAL_SYSTICK_HANDLER,       /* SysTick */
};

void Reset_Handler(void)
{
    const uint32_t *Source;
    uint32_t       *Destination;

    /* The initial values of .data sit in flash, right after the code */
    Source = &_sidata;
    for (Destination = &_sdata; (uintptr_t)Destination < (uintptr_t)&_edata; Destination++)
    {
        *Destination = *Source;
        Source++;
    }

    for (Destination = &_sbss; (uintptr_t)Destination < (uintptr_t)&_ebss; Destination++)
    {
        *Destination = 0U;
    }

#if defined(__ARM_FP)
    /* Full access to CP10 and CP11 (the FPU), before any code can use it */
    CPACR |= (0xFUL << 20);
    __asm volatile("dsb" ::: "memory");
    __asm volatile("isb" ::: "memory");
#endif

    (void)main(0, 0);

    /* main only returns if the scheduler never started */
    for (;;)
    {
    }
}

void NMI_Handler(void)
{
    for (;;)
    {
    }
}

void HardFault_Handler(void)
{
    for (;;)
    {
    }
}
