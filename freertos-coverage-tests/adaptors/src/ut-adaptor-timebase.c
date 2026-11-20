/*
 * File: ut-adaptor-timebase.c
 *
 * Purpose:
 *   Implements the test-only accessors declared in ut-adaptor-timebase.h,
 *   against the real OS_impl_timebase_table type from the port's own
 *   freertos/inc/os-impl-timebase.h.
 */

#include "osconfig.h"

#include "ut-adaptor-timebase.h"

#include "os-impl-timebase.h"

void *const  UT_Ref_OS_impl_timebase_table      = OS_impl_timebase_table;
size_t const UT_Ref_OS_impl_timebase_table_SIZE = sizeof(OS_impl_timebase_table);

void *UT_TimeBaseTest_GetImplTask(osal_index_t local_id)
{
    return &OS_impl_timebase_table[local_id].task;
}

StackType_t *UT_TimeBaseTest_GetImplStack(osal_index_t local_id)
{
    return OS_impl_timebase_table[local_id].stack;
}

TaskHandle_t UT_TimeBaseTest_GetImplHandle(osal_index_t local_id)
{
    return OS_impl_timebase_table[local_id].handle;
}

void UT_TimeBaseTest_SetImplLastTick(osal_index_t local_id, TickType_t tick)
{
    OS_impl_timebase_table[local_id].last_tick = tick;
}

TickType_t UT_TimeBaseTest_GetImplLastTick(osal_index_t local_id)
{
    return OS_impl_timebase_table[local_id].last_tick;
}

void UT_TimeBaseTest_SetImplArm(osal_index_t local_id, bool armed, TickType_t deadline, TickType_t interval)
{
    OS_impl_timebase_table[local_id].armed    = armed;
    OS_impl_timebase_table[local_id].deadline = deadline;
    OS_impl_timebase_table[local_id].interval = interval;
}

bool UT_TimeBaseTest_GetImplArmed(osal_index_t local_id)
{
    return OS_impl_timebase_table[local_id].armed;
}

TickType_t UT_TimeBaseTest_GetImplDeadline(osal_index_t local_id)
{
    return OS_impl_timebase_table[local_id].deadline;
}

TickType_t UT_TimeBaseTest_GetImplInterval(osal_index_t local_id)
{
    return OS_impl_timebase_table[local_id].interval;
}
