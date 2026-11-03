/*
 * File: ut-adaptor-mutex.c
 *
 * Purpose:
 *   Implements the test-only accessors declared in ut-adaptor-mutex.h,
 *   against the real OS_impl_mutex_table type from the port's own
 *   freertos/inc/os-impl-mutex.h.
 */

#include "osconfig.h"

#include "ut-adaptor-mutex.h"

#include "os-impl-mutex.h"

void *const  UT_Ref_OS_impl_mutex_table      = OS_impl_mutex_table;
size_t const UT_Ref_OS_impl_mutex_table_SIZE = sizeof(OS_impl_mutex_table);

void UT_MutexTest_SetImplMutexId(osal_index_t local_id, SemaphoreHandle_t MutexId)
{
    OS_impl_mutex_table[local_id].id = MutexId;
}

SemaphoreHandle_t UT_MutexTest_GetImplMutexId(osal_index_t local_id)
{
    return OS_impl_mutex_table[local_id].id;
}
