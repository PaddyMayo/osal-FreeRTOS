/*
 * File: ut-adaptor-mutex.h
 *
 * Purpose:
 *   Test-only accessors into os-impl-mutex.c's internal state, for coverage
 *   test cases that need to set up (or inspect) a precondition the module's
 *   public _Impl API cannot reach directly. See ut-adaptor-tasks.h for the
 *   rationale behind keeping these in a dedicated adaptor.
 */

#ifndef UT_ADAPTOR_MUTEX_H
#define UT_ADAPTOR_MUTEX_H

#include "common_types.h"

#include "FreeRTOS.h"
#include "semphr.h"

/* Opaque handle to OS_impl_mutex_table, for resetting it between test cases */
extern void *const  UT_Ref_OS_impl_mutex_table;
extern size_t const UT_Ref_OS_impl_mutex_table_SIZE;

void              UT_MutexTest_SetImplMutexId(osal_index_t local_id, SemaphoreHandle_t MutexId);
SemaphoreHandle_t UT_MutexTest_GetImplMutexId(osal_index_t local_id);

#endif /* UT_ADAPTOR_MUTEX_H */
