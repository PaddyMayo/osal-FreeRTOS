/*
 * File: FreeRTOS.h
 *
 * Purpose:
 *   Fake FreeRTOS.h for white-box coverage testing of the freertos/src/os-impl-*.c
 *   modules. Declares only the types/macros those files actually use - not a real port.
 */

#ifndef FREERTOS_H
#define FREERTOS_H

#include <stddef.h>

typedef long          BaseType_t;
typedef unsigned long UBaseType_t;
typedef unsigned long TickType_t;
typedef void         *TaskHandle_t;
typedef unsigned long StackType_t;
typedef unsigned long configSTACK_DEPTH_TYPE;

/* Real FreeRTOS keeps this opaque and much larger; the coverage build never
 * inspects its contents, only takes its address, so a minimal body is enough. */
typedef struct
{
    // cppcheck-suppress unusedStructMember ; only the struct's address is ever used
    unsigned char reserved[4];
} StaticTask_t;

#define pdTRUE  1
#define pdFALSE 0
#define pdPASS  pdTRUE
#define pdFAIL  pdFALSE

#define portMAX_DELAY (~(TickType_t)0)

#define configMAX_PRIORITIES            8
/* Mirrors generic-freertos/config/FreeRTOSConfig.h */
#define OSAL_FREERTOS_MAX_TASK_PRIORITY (configMAX_PRIORITIES - 2)
#define configTICK_RATE_HZ              100
#define configMINIMAL_STACK_SIZE        64
#define configTIMER_TASK_STACK_DEPTH    (configMINIMAL_STACK_SIZE * 2)

/* Two indexes: 0 for application code, 1 for the port's condition variables */
#define configTASK_NOTIFICATION_ARRAY_ENTRIES 2

/* Truncates like the real one, which is what OS_FreeRTOS_MsToTicks() compensates for */
#define pdMS_TO_TICKS(xTimeInMs) ((TickType_t)(((unsigned long long)(xTimeInMs) * configTICK_RATE_HZ) / 1000U))

/*
 * Declared under an OCS_ prefix and redirected via #define - see task.h for
 * why (avoids colliding with the real freertos_kernel pulled in transitively
 * through ut_assert -> osal_bsp).
 */
void *OCS_pvPortMalloc(size_t xSize);
void  OCS_vPortFree(void *pv);

/* The real layout; the members are used from other files (os-impl-heap.c, the heap test) */
// cppcheck-suppress-begin unusedStructMember
typedef struct xHeapStats
{
    size_t xAvailableHeapSpaceInBytes;
    size_t xSizeOfLargestFreeBlockInBytes;
    size_t xSizeOfSmallestFreeBlockInBytes;
    size_t xNumberOfFreeBlocks;
    size_t xMinimumEverFreeBytesRemaining;
    size_t xNumberOfSuccessfulAllocations;
    size_t xNumberOfSuccessfulFrees;
} HeapStats_t;
// cppcheck-suppress-end unusedStructMember

void OCS_vPortGetHeapStats(HeapStats_t *pxHeapStats);

#define pvPortMalloc      OCS_pvPortMalloc
#define vPortFree         OCS_vPortFree
#define vPortGetHeapStats OCS_vPortGetHeapStats

#endif /* FREERTOS_H */
