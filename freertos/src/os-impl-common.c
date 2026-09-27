/**
 * \file
 *
 * Purpose: OS_API_Impl_Init() must return OS_SUCCESS for every object type:
 * OS_API_Init() (third_party/osal/src/os/shared/src/osapi-common.c) stops
 * its init loop at the first non-success, so anything else leaves every
 * later subsystem uninitialized. Modules that are still stubs fail cleanly
 * at their own _Impl calls instead.
 */

#include "FreeRTOS.h"

#include "os-shared-common.h"

int32 OS_API_Impl_Init(osal_objtype_t idtype)
{
    OS_SharedGlobalVars.TicksPerSecond  = configTICK_RATE_HZ;
    OS_SharedGlobalVars.MicroSecPerTick = 1000000 / configTICK_RATE_HZ;

    return OS_SUCCESS;
}

void OS_IdleLoop_Impl(void)
{
}

void OS_ApplicationShutdown_Impl(void)
{
}
