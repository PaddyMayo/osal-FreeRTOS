/*
 * File: ut-adaptor-binsem.c
 *
 * Purpose:
 *   Implements the test-only accessors declared in ut-adaptor-binsem.h,
 *   against the real OS_impl_bin_sem_table type from the port's own
 *   freertos/inc/os-impl-binsem.h.
 */

#include "osconfig.h"

#include "ut-adaptor-binsem.h"

#include "os-impl-binsem.h"

void *const  UT_Ref_OS_impl_bin_sem_table      = OS_impl_bin_sem_table;
size_t const UT_Ref_OS_impl_bin_sem_table_SIZE = sizeof(OS_impl_bin_sem_table);

void UT_BinSemTest_SetImplId(osal_index_t local_id, SemaphoreHandle_t SemId)
{
    OS_impl_bin_sem_table[local_id].id = SemId;
}

SemaphoreHandle_t UT_BinSemTest_GetImplId(osal_index_t local_id)
{
    return OS_impl_bin_sem_table[local_id].id;
}

OS_impl_gate_t *UT_BinSemTest_GetImplGate(osal_index_t local_id)
{
    return &OS_impl_bin_sem_table[local_id].gate;
}

void UT_BinSemTest_SetFlushCount(osal_index_t local_id, uint32 count)
{
    OS_impl_bin_sem_table[local_id].flush_count = count;
}

uint32 UT_BinSemTest_GetFlushCount(osal_index_t local_id)
{
    return OS_impl_bin_sem_table[local_id].flush_count;
}
