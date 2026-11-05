/*
 * File: ut-adaptor-countsem.c
 *
 * Purpose:
 *   Implements the test-only accessors declared in ut-adaptor-countsem.h,
 *   against the real OS_impl_count_sem_table type from the port's own
 *   freertos/inc/os-impl-countsem.h.
 */

#include "osconfig.h"

#include "ut-adaptor-countsem.h"

#include "os-impl-countsem.h"

void *const  UT_Ref_OS_impl_count_sem_table      = OS_impl_count_sem_table;
size_t const UT_Ref_OS_impl_count_sem_table_SIZE = sizeof(OS_impl_count_sem_table);

void UT_CountSemTest_SetImplId(osal_index_t local_id, SemaphoreHandle_t SemId)
{
    OS_impl_count_sem_table[local_id].id = SemId;
}

SemaphoreHandle_t UT_CountSemTest_GetImplId(osal_index_t local_id)
{
    return OS_impl_count_sem_table[local_id].id;
}

OS_impl_gate_t *UT_CountSemTest_GetImplGate(osal_index_t local_id)
{
    return &OS_impl_count_sem_table[local_id].gate;
}
