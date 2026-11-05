/*
 * File: ut-adaptor-binsem.h
 *
 * Purpose:
 *   Test-only accessors into os-impl-binsem.c's internal state, for coverage
 *   test cases that need to set up (or inspect) a precondition the module's
 *   public _Impl API cannot reach directly. See ut-adaptor-tasks.h for the
 *   rationale behind keeping these in a dedicated adaptor.
 */

#ifndef UT_ADAPTOR_BINSEM_H
#define UT_ADAPTOR_BINSEM_H

#include "common_types.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include "os-freertos.h"

/* Opaque handle to OS_impl_bin_sem_table, for resetting it between test cases */
extern void *const  UT_Ref_OS_impl_bin_sem_table;
extern size_t const UT_Ref_OS_impl_bin_sem_table_SIZE;

void              UT_BinSemTest_SetImplId(osal_index_t local_id, SemaphoreHandle_t SemId);
SemaphoreHandle_t UT_BinSemTest_GetImplId(osal_index_t local_id);

/* The gate of a semaphore, for checking that the right one was entered, left, opened or closed */
OS_impl_gate_t *UT_BinSemTest_GetImplGate(osal_index_t local_id);

/* The number of flushes a semaphore has seen, which a test changes to stand in for a flush by another task */
void   UT_BinSemTest_SetFlushCount(osal_index_t local_id, uint32 count);
uint32 UT_BinSemTest_GetFlushCount(osal_index_t local_id);

#endif /* UT_ADAPTOR_BINSEM_H */
