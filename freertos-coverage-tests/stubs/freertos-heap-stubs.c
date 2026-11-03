/*
 * File: freertos-heap-stubs.c
 *
 * Purpose:
 *   UT-controllable stub of vPortGetHeapStats() (declared in the fake
 *   FreeRTOS.h), used to white-box test freertos/src/os-impl-heap.c without a
 *   real FreeRTOS kernel. The stats it hands back are whatever the test put in
 *   UT_Stub_HeapStats.
 */

#include "utstubs.h"

#include "FreeRTOS.h"

/* Handed back by vPortGetHeapStats() */
HeapStats_t UT_Stub_HeapStats;

void vPortGetHeapStats(HeapStats_t *pxHeapStats)
{
    UT_DEFAULT_IMPL(vPortGetHeapStats);

    *pxHeapStats = UT_Stub_HeapStats;
}
