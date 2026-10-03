/**
 * \file
 *
 * Purpose: OSAL common/global implementation on top of FreeRTOS.
 *
 *          OS_API_Impl_Init() must return OS_SUCCESS for every object type:
 *          OS_API_Init() (third_party/osal/src/os/shared/src/osapi-common.c)
 *          stops its init loop at the first non-success, so anything else
 *          leaves every later subsystem uninitialized. Modules that are still
 *          stubs fail cleanly at their own _Impl calls instead.
 *
 *          The idle loop blocks on a statically allocated binary semaphore
 *          that OS_ApplicationShutdown_Impl() gives. A semaphore (rather than
 *          a task notification) keeps the wakeup even when shutdown runs
 *          before the idle task has blocked, so no wakeup can be lost.
 */

#include "FreeRTOS.h"
#include "semphr.h"

#include "os-shared-common.h"
#include "os-shared-idmap.h"

static StaticSemaphore_t OS_FreeRTOS_IdleSemBuffer;
static SemaphoreHandle_t OS_FreeRTOS_IdleSem;

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_API_Impl_Init(osal_objtype_t idtype)
{
    if (idtype != OS_OBJECT_TYPE_UNDEFINED)
    {
        return OS_SUCCESS;
    }

    OS_SharedGlobalVars.TicksPerSecond  = configTICK_RATE_HZ;
    OS_SharedGlobalVars.MicroSecPerTick = 1000000 / configTICK_RATE_HZ;

    OS_FreeRTOS_IdleSem = xSemaphoreCreateBinaryStatic(&OS_FreeRTOS_IdleSemBuffer);
    if (OS_FreeRTOS_IdleSem == NULL)
    {
        return OS_ERROR;
    }

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
void OS_IdleLoop_Impl(void)
{
    (void)xSemaphoreTake(OS_FreeRTOS_IdleSem, portMAX_DELAY);
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
void OS_ApplicationShutdown_Impl(void)
{
    (void)xSemaphoreGive(OS_FreeRTOS_IdleSem);
}
