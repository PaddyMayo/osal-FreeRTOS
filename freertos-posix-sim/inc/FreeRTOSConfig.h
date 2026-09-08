#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <pthread.h>

/* Scheduling */
#define configUSE_PREEMPTION                   1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configUSE_TICKLESS_IDLE                0
#define configTICK_RATE_HZ                     100
#define configMAX_PRIORITIES                   7
#define configMINIMAL_STACK_SIZE               ( PTHREAD_STACK_MIN )
#define configMAX_TASK_NAME_LEN                16
#define configIDLE_SHOULD_YIELD                1
#define configUSE_16_BIT_TICKS                 0

/* Memory allocation */
#define configSUPPORT_STATIC_ALLOCATION        1
#define configSUPPORT_DYNAMIC_ALLOCATION       1
#define configTOTAL_HEAP_SIZE                  ( ( size_t ) ( 1024 * 1024 ) )

/* Synchronization primitives OSAL needs */
#define configUSE_MUTEXES                      1
#define configUSE_RECURSIVE_MUTEXES            1
#define configUSE_COUNTING_SEMAPHORES          1
#define configUSE_QUEUE_SETS                   0
#define configUSE_TASK_NOTIFICATIONS           1
#define configQUEUE_REGISTRY_SIZE              20

/* Software timers */
#define configUSE_TIMERS                       1
#define configTIMER_TASK_PRIORITY              ( configMAX_PRIORITIES - 1 )
#define configTIMER_QUEUE_LENGTH                20
#define configTIMER_TASK_STACK_DEPTH           ( configMINIMAL_STACK_SIZE * 2 )

/* Hooks — off, since we haven't implemented any */
#define configUSE_IDLE_HOOK                    0
#define configUSE_TICK_HOOK                    0
#define configUSE_MALLOC_FAILED_HOOK           0
#define configUSE_DAEMON_TASK_STARTUP_HOOK     0

/* Debugging */
#define configCHECK_FOR_STACK_OVERFLOW         2
#define configUSE_TRACE_FACILITY               0
#define configGENERATE_RUN_TIME_STATS          0
#define configASSERT( x )                                  \
    if( ( x ) == 0 )                                        \
    {                                                        \
        taskDISABLE_INTERRUPTS();                           \
        for( ; ; );                                          \
    }

/* Co-routines — unused, legacy FreeRTOS feature */
#define configUSE_CO_ROUTINES                  0
#define configMAX_CO_ROUTINE_PRIORITIES        1

/* API inclusions OSAL's task/queue/semaphore Impl functions rely on */
#define