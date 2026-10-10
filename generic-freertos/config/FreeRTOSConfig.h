#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* The values arrive as OSAL_FREERTOS_* compile definitions from osal_freertos_platform() */
#ifdef OSAL_FREERTOS_PORT_HEADER
/* cppcheck-suppress syntaxError */
#include OSAL_FREERTOS_PORT_HEADER
#endif

#ifndef OSAL_FREERTOS_TICK_RATE_HZ
#error "OSAL_FREERTOS_TICK_RATE_HZ is not defined - set it in the -C config file"
#endif
#ifndef OSAL_FREERTOS_MINIMAL_STACK_SIZE
#error "OSAL_FREERTOS_MINIMAL_STACK_SIZE is not defined - set it in the -C config file"
#endif
#ifndef OSAL_FREERTOS_TOTAL_HEAP_SIZE
#error "OSAL_FREERTOS_TOTAL_HEAP_SIZE is not defined - set it in the -C config file"
#endif

/* Scheduling */
#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configUSE_TICKLESS_IDLE                 0
#define configTICK_RATE_HZ                      OSAL_FREERTOS_TICK_RATE_HZ
#define configMAX_PRIORITIES                    7
/* The vendor gives bytes; FreeRTOS counts words */
#define configMINIMAL_STACK_SIZE                (OSAL_FREERTOS_MINIMAL_STACK_SIZE / sizeof(StackType_t))
#define configMAX_TASK_NAME_LEN                 16
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_16_BIT_TICKS                  0
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 1

#if defined(OSAL_FREERTOS_CPU_CLOCK_HZ)
#define configCPU_CLOCK_HZ OSAL_FREERTOS_CPU_CLOCK_HZ
#endif

/* Interrupt priorities */
#if defined(OSAL_FREERTOS_KERNEL_INTERRUPT_PRIORITY)
#define configKERNEL_INTERRUPT_PRIORITY      OSAL_FREERTOS_KERNEL_INTERRUPT_PRIORITY
#define configMAX_SYSCALL_INTERRUPT_PRIORITY OSAL_FREERTOS_MAX_SYSCALL_INTERRUPT_PRIORITY
#endif

/* Memory allocation */
#define configSUPPORT_STATIC_ALLOCATION  1
#define configSUPPORT_DYNAMIC_ALLOCATION 1
#define configTOTAL_HEAP_SIZE            OSAL_FREERTOS_TOTAL_HEAP_SIZE

/* Synchronization primitives OSAL needs */
#define configUSE_MUTEXES             1
#define configUSE_RECURSIVE_MUTEXES   1
#define configUSE_COUNTING_SEMAPHORES 1
#define configUSE_QUEUE_SETS          0
#define configUSE_TASK_NOTIFICATIONS  1
#define configQUEUE_REGISTRY_SIZE     20

/* Software timers */
#define configUSE_TIMERS             1
#define configTIMER_TASK_PRIORITY    (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH     20
#define configTIMER_TASK_STACK_DEPTH (configMINIMAL_STACK_SIZE * 2)

/* Hooks — off, since we haven't implemented any */
#define configUSE_IDLE_HOOK                0
#define configUSE_TICK_HOOK                0
#define configUSE_MALLOC_FAILED_HOOK       0
#define configUSE_DAEMON_TASK_STARTUP_HOOK 0

/* Debugging */
#define configCHECK_FOR_STACK_OVERFLOW 2
#define configUSE_TRACE_FACILITY       0
#define configGENERATE_RUN_TIME_STATS  0
#ifndef __ASSEMBLER__
/* Reports the failure on the BSP console, then halts (generic-freertos/src/bsp_console.c) */
void OS_FreeRTOS_AssertFailed(const char *File, int Line);
#endif
#define configASSERT(x)                                   \
    do                                                    \
    {                                                     \
        if ((x) == 0)                                     \
        {                                                 \
            OS_FreeRTOS_AssertFailed(__FILE__, __LINE__); \
        }                                                 \
    } while (0)

/* Co-routines — unused, legacy FreeRTOS feature */
#define configUSE_CO_ROUTINES           0
#define configMAX_CO_ROUTINE_PRIORITIES 1

/* API inclusions OSAL's task/queue/semaphore Impl functions rely on */
#define INCLUDE_vTaskDelete                 1
#define INCLUDE_vTaskDelay                  1
#define INCLUDE_vTaskDelayUntil             1
#define INCLUDE_vTaskSuspend                1
#define INCLUDE_vTaskPrioritySet            1
#define INCLUDE_uxTaskPriorityGet           1
#define INCLUDE_eTaskGetState               1
#define INCLUDE_xTaskAbortDelay             1
#define INCLUDE_xTaskGetHandle              1
#define INCLUDE_xTaskGetSchedulerState      1
#define INCLUDE_xTaskGetCurrentTaskHandle   1
#define INCLUDE_xTaskGetIdleTaskHandle      1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define INCLUDE_xSemaphoreGetMutexHolder    1
#define INCLUDE_xTimerPendFunctionCall      1

/* Port hooks */
#ifndef __ASSEMBLER__
/* weak: binaries that link the kernel without OSAL's FreeRTOS layer (e.g. other OS coverage tests) leave it NULL */
__attribute__((weak)) void OS_FreeRTOS_TaskCleanup(const void *tcb);
#endif

#define OSAL_FREERTOS_TASK_CLEANUP(pxTCB)    \
    do                                       \
    {                                        \
        if (OS_FreeRTOS_TaskCleanup != NULL) \
        {                                    \
            OS_FreeRTOS_TaskCleanup(pxTCB);  \
        }                                    \
    } while (0)

/* A port whose portmacro.h defines portCLEAN_UP_TCB runs its own cleanup first */
#if defined(OSAL_FREERTOS_PORT_CLEAN_UP_TCB)
#include "portmacro.h"
#define OSAL_FREERTOS_CLEAN_UP_TCB(pxTCB)                 \
    do                                                    \
    {                                                     \
        OSAL_FREERTOS_PORT_CLEAN_UP_TCB((void *)(pxTCB)); \
        OSAL_FREERTOS_TASK_CLEANUP(pxTCB);                \
    } while (0)
#undef portCLEAN_UP_TCB
#else
#define OSAL_FREERTOS_CLEAN_UP_TCB(pxTCB) OSAL_FREERTOS_TASK_CLEANUP(pxTCB)
#endif
#define portCLEAN_UP_TCB(pxTCB) OSAL_FREERTOS_CLEAN_UP_TCB(pxTCB)

#endif /* FREERTOS_CONFIG_H */
