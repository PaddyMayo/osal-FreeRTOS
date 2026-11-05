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

SemaphoreHandle_t OCS_xSemaphoreCreateBinary(void);

SemaphoreHandle_t OCS_xSemaphoreCreateBinaryStatic(StaticSemaphore_t *pxSemaphoreBuffer);

SemaphoreHandle_t OCS_xSemaphoreCreateMutexStatic(StaticSemaphore_t *pxMutexBuffer);

SemaphoreHandle_t OCS_xSemaphoreCreateCountingStatic(UBaseType_t        uxMaxCount,
                                                     UBaseType_t        uxInitialCount,
                                                     StaticSemaphore_t *pxSemaphoreBuffer);

BaseType_t OCS_xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xBlockTime);

BaseType_t OCS_xSemaphoreGive(SemaphoreHandle_t xSemaphore);

SemaphoreHandle_t OCS_xSemaphoreCreateRecursiveMutex(void);

void OCS_vSemaphoreDelete(SemaphoreHandle_t xSemaphore);

BaseType_t OCS_xSemaphoreTakeRecursive(SemaphoreHandle_t xMutex, TickType_t xBlockTime);

BaseType_t OCS_xSemaphoreGiveRecursive(SemaphoreHandle_t xMutex);

TaskHandle_t OCS_xSemaphoreGetMutexHolder(SemaphoreHandle_t xSemaphore);

UBaseType_t OCS_uxSemaphoreGetCount(SemaphoreHandle_t xSemaphore);

#define xSemaphoreCreateBinary         OCS_xSemaphoreCreateBinary
#define xSemaphoreCreateBinaryStatic   OCS_xSemaphoreCreateBinaryStatic
#define xSemaphoreCreateMutexStatic    OCS_xSemaphoreCreateMutexStatic
#define xSemaphoreCreateCountingStatic OCS_xSemaphoreCreateCountingStatic
#define xSemaphoreTake                 OCS_xSemaphoreTake
#define xSemaphoreGive                 OCS_xSemaphoreGive
#define xSemaphoreCreateRecursiveMutex OCS_xSemaphoreCreateRecursiveMutex
#define vSemaphoreDelete               OCS_vSemaphoreDelete
#define xSemaphoreTakeRecursive        OCS_xSemaphoreTakeRecursive
#define xSemaphoreGiveRecursive        OCS_xSemaphoreGiveRecursive
#define xSemaphoreGetMutexHolder       OCS_xSemaphoreGetMutexHolder
#define uxSemaphoreGetCount            OCS_uxSemaphoreGetCount

#endif /* SEMAPHORE_H */
