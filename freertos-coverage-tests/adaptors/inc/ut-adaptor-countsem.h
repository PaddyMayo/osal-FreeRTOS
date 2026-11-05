/*
 * File: ut-adaptor-countsem.h
 *
 * Purpose:
 *   Test-only accessors into os-impl-countsem.c's internal state, for coverage
 *   test cases that need to set up (or inspect) a precondition the module's
 *   public _Impl API cannot reach directly. See ut-adaptor-tasks.h for the
 *   rationale behind keeping these in a dedicated adaptor.
 */

#ifndef UT_ADAPTOR_COUNTSEM_H
#define UT_ADAPTOR_COUNTSEM_H

#include "common_types.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include "os-freertos.h"

/* Opaque handle to OS_impl_count_sem_table, for resetting it between test cases */
extern void *const  UT_Ref_OS_impl_count_sem_table;
extern size_t const UT_Ref_OS_impl_count_sem_table_SIZE;

void              UT_CountSemTest_SetImplId(osal_index_t local_id, SemaphoreHandle_t SemId);
SemaphoreHandle_t UT_CountSemTest_GetImplId(osal_index_t local_id);

/* The gate of a semaphore, for checking that the right one was entered, left, opened or closed */
OS_impl_gate_t *UT_CountSemTest_GetImplGate(osal_index_t local_id);

#endif /* UT_ADAPTOR_COUNTSEM_H */
