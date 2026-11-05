/*
 * File: os-impl-binsem.h
 *
 * Purpose:
 *   Internal binary semaphore table definitions for the FreeRTOS OSAL port.
 *
 *   These live in a header rather than staying file-local to os-impl-binsem.c
 *   so the coverage-test adaptor can reach the table through the real type
 *   instead of a duplicate declaration that could silently drift out of sync
 *   with it - the same reason OSAL's own ports expose theirs this way (see
 *   third_party/osal/src/os/posix/inc/os-impl-binsem.h).
 */

#ifndef OS_IMPL_BINSEM_H
#define OS_IMPL_BINSEM_H

#include "osconfig.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include "os-freertos.h"

/*
 * OS_BinSemGive(), OS_BinSemFlush(), OS_BinSemTake() and OS_BinSemTimedWait()
 * look the semaphore up with OS_LOCK_MODE_NONE, which does not raise the
 * refcount that OS_BinSemDelete() waits on, so they pass through the gate,
 * which the delete closes and waits on (os-freertos.h).
 *
 * flush_count is how a Take that was woken tells a flush from a delete: a
 * flush adds one, and every Take that sees it change was flushed. It is
 * volatile because a Take reads it across the wait, while another task writes.
 */
typedef struct
{
    SemaphoreHandle_t id;
    OS_impl_gate_t    gate;
    volatile uint32   flush_count;
} OS_impl_binsem_internal_record_t;

/* Table where the OS object information is stored */
extern OS_impl_binsem_internal_record_t OS_impl_bin_sem_table[OS_MAX_BIN_SEMAPHORES];

#endif /* OS_IMPL_BINSEM_H */
