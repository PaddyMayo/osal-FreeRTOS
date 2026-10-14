/*
 * File: timers.h
 *
 * Purpose:
 *   Fake FreeRTOS timers.h for white-box coverage testing of
 *   generic-freertos/src/bsp_start.c, which includes it but calls nothing
 *   from the timer API - it only needs the header to exist.
 */

#ifndef TIMERS_H
#define TIMERS_H

#include "FreeRTOS.h"

#endif /* TIMERS_H */
