#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <pthread.h>

/* Scheduling */
#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configUSE_TICKLESS_IDLE                 0
#define configTICK_RATE_HZ                      100
#define configMAX_PRIORITIES                    7
#define configMINIMAL_STACK_SIZE                (PTHREAD_STACK_MIN)
#define configMAX_TASK_NAME_LEN                 16
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_16_BIT_TICKS                  0
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 1

/* Memory allocation */
#define configSUPPORT_STATIC_ALLOCATION  1
#define configSUPPORT_DYNAMIC_ALLOCATION 1
/* Sized for PTHREAD_STACK_MIN-based task stacks: the OSAL startup task alone needs 4 MiB on aarch64 */
#define configTOTAL_HEAP_SIZE            ((size_t)(16 * 1024 * 1024))

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
#define configASSERT(x)           \
    if ((x) == 0)                 \
    {                             \
        taskDISABLE_INTERRUPTS(); \
        for (;;)                  \
            ;                     \
    }

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
void OS_FreeRTOS_TaskCleanup(const void *tcb) __attribute__((weak));
#endif

#define OSAL_FREERTOS_TASK_CLEANUP(pxTCB)    \
    do                                       \
    {                                        \
        if (OS_FreeRTOS_TaskCleanup != NULL) \
        {                                    \
            OS_FreeRTOS_TaskCleanup(pxTCB);  \
        }                                    \
    } while (0)

#if defined(OSAL_FREERTOS_PORT_POSIX)
#include "portmacro.h"
#undef portCLEAN_UP_TCB
#define portCLEAN_UP_TCB(pxTCB)            \
    do                                     \
    {                                      \
        vPortCancelThread(pxTCB);          \
        OSAL_FREERTOS_TASK_CLEANUP(pxTCB); \
    } while (0)
#else
#define portCLEAN_UP_TCB(pxTCB) OSAL_FREERTOS_TASK_CLEANUP(pxTCB)
#endif

#endif /* FREERTOS_CONFIG_H */
