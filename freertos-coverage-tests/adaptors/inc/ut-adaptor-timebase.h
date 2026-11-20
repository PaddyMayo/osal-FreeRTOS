/*
 * File: ut-adaptor-timebase.h
 *
 * Purpose:
 *   Test-only accessors into os-impl-timebase.c's internal state, for coverage
 *   test cases that need to set up a precondition the module's public _Impl
 *   API cannot reach directly, or to check what it left behind.
 *
 *   Keeping these here (rather than in the test case file) confines all
 *   knowledge of the internal table to one place, and mirrors the adaptor
 *   layout OSAL's own coverage tests use - see
 *   third_party/osal/src/unit-test-coverage/vxworks/adaptors/.
 */

#ifndef UT_ADAPTOR_TIMEBASE_H
#define UT_ADAPTOR_TIMEBASE_H

#include "common_types.h"

#include "FreeRTOS.h"
#include "task.h"

/* Opaque handle to OS_impl_timebase_table, for resetting it between test cases */
extern void *const  UT_Ref_OS_impl_timebase_table;
extern size_t const UT_Ref_OS_impl_timebase_table_SIZE;

/* The static TCB and stack a time base's servicing task is built on, to check the right ones were handed to the kernel
 */
void        *UT_TimeBaseTest_GetImplTask(osal_index_t local_id);
StackType_t *UT_TimeBaseTest_GetImplStack(osal_index_t local_id);

TaskHandle_t UT_TimeBaseTest_GetImplHandle(osal_index_t local_id);

void       UT_TimeBaseTest_SetImplLastTick(osal_index_t local_id, TickType_t tick);
TickType_t UT_TimeBaseTest_GetImplLastTick(osal_index_t local_id);

/* What is armed: whether a tick is due, at which kernel tick, and how often after that */
void       UT_TimeBaseTest_SetImplArm(osal_index_t local_id, bool armed, TickType_t deadline, TickType_t interval);
bool       UT_TimeBaseTest_GetImplArmed(osal_index_t local_id);
TickType_t UT_TimeBaseTest_GetImplDeadline(osal_index_t local_id);
TickType_t UT_TimeBaseTest_GetImplInterval(osal_index_t local_id);

#endif /* UT_ADAPTOR_TIMEBASE_H */
