/*
 * File: os-impl-countsem.h
 *
 * Purpose:
 *   Internal counting semaphore table definitions for the FreeRTOS OSAL port.
 *
 *   These live in a header rather than staying file-local to
 *   os-impl-countsem.c so the coverage-test adaptor can reach the table
 *   through the real type instead of a duplicate declaration that could
 *   silently drift out of sync with it - the same reason OSAL's own ports
 *   expose theirs this way (see third_party/osal/src/os/posix/inc/
 *   os-impl-countsem.h).
 */

#ifndef OS_IMPL_COUNTSEM_H
#define OS_IMPL_COUNTSEM_H

#include "osconfig.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include "os-freertos.h"

/*
 * The highest value a counting semaphore can hold, and the most that a create
 * accepts. FreeRTOS wants the limit when the semaphore is made. OS_CountSemGetInfo()
 * reports the value as an int32, so a count above INT32_MAX could not be
 * reported; the POSIX port stops at the same point (UINT32_MAX / 2).
 */
#define OS_FREERTOS_SEM_VALUE_MAX 2147483647U

/*
 * OS_CountSemGive(), OS_CountSemTake() and OS_CountSemTimedWait() look the
 * semaphore up with OS_LOCK_MODE_NONE, which does not raise the refcount that
 * OS_CountSemDelete() waits on, so they pass through the gate, which the delete
 * closes and waits on (os-freertos.h).
 */
typedef struct
{
    SemaphoreHandle_t id;
    OS_impl_gate_t    gate;
} OS_impl_countsem_internal_record_t;

/* Table where the OS object information is stored */
extern OS_impl_countsem_internal_record_t OS_impl_count_sem_table[OS_MAX_COUNT_SEMAPHORES];

#endif /* OS_IMPL_COUNTSEM_H */
