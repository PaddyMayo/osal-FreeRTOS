/**
 * \file
 *
 * Purpose: OSAL heap implementation on top of FreeRTOS.
 *
 *          The heap reported is the FreeRTOS heap (configTOTAL_HEAP_SIZE), the
 *          one the kernel objects and the OSAL pvPortMalloc() callers share.
 *          vPortGetHeapStats() exists only in heap_4 and heap_5, and
 *          FreeRTOSConfig.h rejects any other heap.
 */

#include "FreeRTOS.h"

#include "os-shared-heap.h"

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_HeapGetInfo_Impl(OS_heap_prop_t *heap_prop)
{
    HeapStats_t stats;

    vPortGetHeapStats(&stats);

    heap_prop->free_bytes         = stats.xAvailableHeapSpaceInBytes;
    heap_prop->free_blocks        = (osal_blockcount_t)stats.xNumberOfFreeBlocks;
    heap_prop->largest_free_block = stats.xSizeOfLargestFreeBlockInBytes;

    return OS_SUCCESS;
}
