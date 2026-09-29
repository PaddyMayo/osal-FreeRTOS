/*
 * File: FreeRTOSPlatformContract.h
 *
 * Purpose:
 *   The contract between the shared FreeRTOSConfig.h and each BSP's
 *   FreeRTOSConfigPlatform.h. The shared config only ever consumes the
 *   OSAL_FREERTOS_PLATFORM_* macros below; a platform sets itself up by
 *   defining them, and never defines config* values directly.
 *
 *   Required:
 *     OSAL_FREERTOS_PLATFORM_TICK_RATE_HZ               -> configTICK_RATE_HZ
 *     OSAL_FREERTOS_PLATFORM_MINIMAL_STACK_SIZE         -> configMINIMAL_STACK_SIZE, in StackType_t words
 *     OSAL_FREERTOS_PLATFORM_TOTAL_HEAP_SIZE            -> configTOTAL_HEAP_SIZE, in bytes
 *     OSAL_FREERTOS_PLATFORM_ASSERT_FAILED(file, line)  action taken when configASSERT fails
 *
 *   Optional:
 *     OSAL_FREERTOS_PLATFORM_OPTIMISED_TASK_SELECTION   -> configUSE_PORT_OPTIMISED_TASK_SELECTION (default 0)
 *     OSAL_FREERTOS_PLATFORM_WEAK                       weak-symbol attribute (default __attribute__((weak)))
 *     OSAL_FREERTOS_PLATFORM_PORT_CLEAN_UP_TCB(pxTCB)   the port's own TCB cleanup; define only when the
 *                                                       port's portmacro.h already defines portCLEAN_UP_TCB
 *     OSAL_FREERTOS_PLATFORM_CPU_CLOCK_HZ               -> configCPU_CLOCK_HZ
 *     OSAL_FREERTOS_PLATFORM_KERNEL_INTERRUPT_PRIORITY  -> configKERNEL_INTERRUPT_PRIORITY
 *     OSAL_FREERTOS_PLATFORM_MAX_SYSCALL_INTERRUPT_PRIORITY -> configMAX_SYSCALL_INTERRUPT_PRIORITY
 */

#ifndef FREERTOS_PLATFORM_CONTRACT_H
#define FREERTOS_PLATFORM_CONTRACT_H

#ifndef OSAL_FREERTOS_PLATFORM_TICK_RATE_HZ
#error "FreeRTOSConfigPlatform.h must define OSAL_FREERTOS_PLATFORM_TICK_RATE_HZ"
#endif

#ifndef OSAL_FREERTOS_PLATFORM_MINIMAL_STACK_SIZE
#error "FreeRTOSConfigPlatform.h must define OSAL_FREERTOS_PLATFORM_MINIMAL_STACK_SIZE"
#endif

#ifndef OSAL_FREERTOS_PLATFORM_TOTAL_HEAP_SIZE
#error "FreeRTOSConfigPlatform.h must define OSAL_FREERTOS_PLATFORM_TOTAL_HEAP_SIZE"
#endif

#ifndef OSAL_FREERTOS_PLATFORM_ASSERT_FAILED
#error "FreeRTOSConfigPlatform.h must define OSAL_FREERTOS_PLATFORM_ASSERT_FAILED(file, line)"
#endif

#ifndef OSAL_FREERTOS_PLATFORM_OPTIMISED_TASK_SELECTION
#define OSAL_FREERTOS_PLATFORM_OPTIMISED_TASK_SELECTION 0
#endif

#ifndef OSAL_FREERTOS_PLATFORM_WEAK
#define OSAL_FREERTOS_PLATFORM_WEAK __attribute__((weak))
#endif

#endif /* FREERTOS_PLATFORM_CONTRACT_H */
