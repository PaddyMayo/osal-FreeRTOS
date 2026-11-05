/*
 * File: ut-adaptor-condvar.h
 *
 * Purpose:
 *   Test-only accessors into os-impl-condvar.c's internal state, for coverage
 *   test cases that need to set up (or inspect) a precondition the module's
 *   public _Impl API cannot reach directly. See ut-adaptor-tasks.h for the
 *   rationale behind keeping these in a dedicated adaptor.
 */

#ifndef UT_ADAPTOR_CONDVAR_H
#define UT_ADAPTOR_CONDVAR_H

#include "common_types.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include "os-freertos.h"
#include "os-impl-condvar.h"

/* Opaque handle to OS_impl_condvar_table, for resetting it between test cases */
extern void *const  UT_Ref_OS_impl_condvar_table;
extern size_t const UT_Ref_OS_impl_condvar_table_SIZE;

void              UT_CondVarTest_SetImplMutex(osal_index_t local_id, SemaphoreHandle_t MutexId);
SemaphoreHandle_t UT_CondVarTest_GetImplMutex(osal_index_t local_id);

/* The gate of a condition variable, for checking that the right one was entered, left, opened or closed */
OS_impl_gate_t *UT_CondVarTest_GetImplGate(osal_index_t local_id);

/* The wait list: a test puts nodes of its own on the end of it, and looks at what is left */
void                      UT_CondVarTest_AddWaiter(osal_index_t local_id, OS_impl_condvar_waiter_t *waiter);
OS_impl_condvar_waiter_t *UT_CondVarTest_GetWaiters(osal_index_t local_id);

#endif /* UT_ADAPTOR_CONDVAR_H */
