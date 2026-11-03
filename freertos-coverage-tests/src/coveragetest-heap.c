/*
 * File: coveragetest-heap.c
 *
 * Purpose:
 *   White-box coverage test cases for freertos/src/os-impl-heap.c, run against
 *   the fake vPortGetHeapStats() in ../fake-inc/FreeRTOS.h, stubbed in
 *   ../stubs/freertos-heap-stubs.c - no real FreeRTOS kernel involved.
 *   (The black box tests for this port are OSAL's own /src/tests and
 *   /src/unit-tests, which link the real OSAL library instead.)
 */

#include <string.h>

#include "utassert.h"
#include "uttest.h"
#include "utstubs.h"

#include "FreeRTOS.h"

#include "os-shared-heap.h"

/* Declared in freertos-heap-stubs.c */
extern HeapStats_t UT_Stub_HeapStats;

/*
**********************************************************************************
**          TEST CASES
**********************************************************************************
*/

void Test_OS_HeapGetInfo_Impl(void)
{
    OS_heap_prop_t heap_prop;

    /* Distinct values, so a pair of swapped fields cannot pass */
    UT_Stub_HeapStats.xAvailableHeapSpaceInBytes      = 4096;
    UT_Stub_HeapStats.xSizeOfLargestFreeBlockInBytes  = 1024;
    UT_Stub_HeapStats.xSizeOfSmallestFreeBlockInBytes = 16;
    UT_Stub_HeapStats.xNumberOfFreeBlocks             = 7;

    memset(&heap_prop, 0, sizeof(heap_prop));

    UtAssert_INT32_EQ(OS_HeapGetInfo_Impl(&heap_prop), OS_SUCCESS);
    UtAssert_STUB_COUNT(vPortGetHeapStats, 1);
    UtAssert_UINT32_EQ(heap_prop.free_bytes, 4096);
    UtAssert_UINT32_EQ(heap_prop.free_blocks, 7);
    UtAssert_UINT32_EQ(heap_prop.largest_free_block, 1024);
}

/* ------------------- End of test cases --------------------------------------*/

void Osapi_Test_Setup(void)
{
    UT_ResetState(0);
    memset(&UT_Stub_HeapStats, 0, sizeof(UT_Stub_HeapStats));
}

void Osapi_Test_Teardown(void)
{
}

#define ADD_TEST(test) UtTest_Add((Test_##test), Osapi_Test_Setup, Osapi_Test_Teardown, #test)

void UtTest_Setup(void)
{
    ADD_TEST(OS_HeapGetInfo_Impl);
}
