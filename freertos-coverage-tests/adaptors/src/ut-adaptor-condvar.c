/*
 * File: ut-adaptor-condvar.c
 *
 * Purpose:
 *   Implements the test-only accessors declared in ut-adaptor-condvar.h,
 *   against the real OS_impl_condvar_table type from the port's own
 *   freertos/inc/os-impl-condvar.h.
 */

#include "osconfig.h"

#include "ut-adaptor-condvar.h"

#include "os-impl-condvar.h"

void *const  UT_Ref_OS_impl_condvar_table      = OS_impl_condvar_table;
size_t const UT_Ref_OS_impl_condvar_table_SIZE = sizeof(OS_impl_condvar_table);

void UT_CondVarTest_SetImplMutex(osal_index_t local_id, SemaphoreHandle_t MutexId)
{
    OS_impl_condvar_table[local_id].mutex = MutexId;
}

SemaphoreHandle_t UT_CondVarTest_GetImplMutex(osal_index_t local_id)
{
    return OS_impl_condvar_table[local_id].mutex;
}

OS_impl_gate_t *UT_CondVarTest_GetImplGate(osal_index_t local_id)
{
    return &OS_impl_condvar_table[local_id].gate;
}

void UT_CondVarTest_AddWaiter(osal_index_t local_id, OS_impl_condvar_waiter_t *waiter)
{
    OS_impl_condvar_waiter_t **link;

    link = &OS_impl_condvar_table[local_id].waiters;
    while (*link != NULL)
    {
        link = &(*link)->next;
    }

    *link = waiter;
}

OS_impl_condvar_waiter_t *UT_CondVarTest_GetWaiters(osal_index_t local_id)
{
    return OS_impl_condvar_table[local_id].waiters;
}
