/*
 * File: semphr.h
 *
 * Purpose:
 *   Fake FreeRTOS semphr.h for white-box coverage testing of the
 *   freertos/src/os-impl-*.c modules. Declares only the semaphore API those
 *   files actually call - not a real port.
 *
 *   Declared under an OCS_ prefix and redirected via #define - see task.h for
 *   why (avoids colliding with the real freertos_kernel pulled in transitively
 *   through ut_assert -> osal_bsp).
 */

#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include "FreeRTOS.h"

typedef void *SemaphoreHandle_t;

/* Real FreeRTOS keeps this opaque and much larger; the coverage build never
 * inspects its contents, only takes its address, so a minimal body is enough. */
typedef struct
{
    // cppcheck-suppress unusedStructMember ; only the struct's address is ever used
    unsigned char reserved[4];
} StaticSemaphore_t;

SemaphoreHandle_t OCS_xSemaphoreCreateBinaryStatic(StaticSemaphore_t *pxSemaphoreBuffer);

SemaphoreHandle_t OCS_xSemaphoreCreateMutexStatic(StaticSemaphore_t *pxMutexBuffer);

BaseType_t OCS_xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xBlockTime);

BaseType_t OCS_xSemaphoreGive(SemaphoreHandle_t xSemaphore);

#define xSemaphoreCreateBinaryStatic OCS_xSemaphoreCreateBinaryStatic
#define xSemaphoreCreateMutexStatic  OCS_xSemaphoreCreateMutexStatic
#define xSemaphoreTake               OCS_xSemaphoreTake
#define xSemaphoreGive               OCS_xSemaphoreGive

#endif /* SEMAPHORE_H */
