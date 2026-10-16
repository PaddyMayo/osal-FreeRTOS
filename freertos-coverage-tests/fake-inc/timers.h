/*
 * File: timers.h
 *
 * Purpose:
 *   Fake FreeRTOS timers.h for white-box coverage testing of
 *   generic-freertos/src/bsp_start.c, which includes it but calls nothing
 *   from the timer API - it only needs the header to exist - and of
 *   freertos/src/os-impl-tasks.c, which hands the delete of an exiting task to
 *   the timer task.
 *
 *   Declared under an OCS_ prefix and redirected via #define - see task.h for why.
 */

#ifndef TIMERS_H
#define TIMERS_H

#include <stdint.h>

#include "FreeRTOS.h"

typedef void (*PendedFunction_t)(void *pvParameter1, uint32_t ulParameter2);

BaseType_t OCS_xTimerPendFunctionCall(PendedFunction_t xFunctionToPend,
                                      void            *pvParameter1,
                                      uint32_t         ulParameter2,
                                      TickType_t       xTicksToWait);

#define xTimerPendFunctionCall OCS_xTimerPendFunctionCall

#endif /* TIMERS_H */
