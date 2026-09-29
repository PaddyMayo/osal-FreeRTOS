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

#define configMAX_PRIORITIES 7

#define pdMS_TO_TICKS(xTimeInMs) (xTimeInMs)

/*
 * Declared under an OCS_ prefix and redirected via #define - see task.h for
 * why (avoids colliding with the real freertos_kernel pulled in transitively
 * through ut_assert -> osal_bsp).
 */
void *OCS_pvPortMalloc(size_t xSize);
void  OCS_vPortFree(void *pv);

#define pvPortMalloc OCS_pvPortMalloc
#define vPortFree    OCS_vPortFree

#endif /* FREERTOS_H */
