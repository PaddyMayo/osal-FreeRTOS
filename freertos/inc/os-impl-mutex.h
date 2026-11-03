/*
 * File: os-impl-mutex.h
 *
 * Purpose:
 *   Internal mutex table definitions for the FreeRTOS OSAL port.
 *
 *   These live in a header rather than staying file-local to os-impl-mutex.c
 *   so the coverage-test adaptor can reach the table through the real type
 *   instead of a duplicate declaration that could silently drift out of sync
 *   with it - the same reason OSAL's own ports expose theirs this way (see
 *   third_party/osal/src/os/vxworks/inc/os-impl-mutex.h).
 */

#ifndef OS_IMPL_MUTEX_H
#define OS_IMPL_MUTEX_H

#include "osconfig.h"

#include "FreeRTOS.h"
#include "semphr.h"

typedef struct
{
    SemaphoreHandle_t id;
} OS_impl_mutex_internal_record_t;

/* Table where the OS object information is stored */
extern OS_impl_mutex_internal_record_t OS_impl_mutex_table[OS_MAX_MUTEXES];

#endif /* OS_IMPL_MUTEX_H */
