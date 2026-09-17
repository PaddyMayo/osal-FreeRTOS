/*
 * File: os-impl-tasks.h
 *
 * Purpose:
 *   Internal task table definitions for the FreeRTOS OSAL port.
 *
 *   These live in a header rather than staying file-local to os-impl-tasks.c
 *   so the coverage-test adaptor can reach the table through the real type
 *   instead of a duplicate declaration that could silently drift out of sync
 *   with it - the same reason OSAL's own ports expose theirs this way (see
 *   third_party/osal/src/os/vxworks/inc/os-impl-tasks.h).
 */

#ifndef OS_IMPL_TASKS_H
#define OS_IMPL_TASKS_H

#include "osconfig.h"

#include "FreeRTOS.h"
#include "task.h"

typedef struct
{
    TaskHandle_t id;
    StaticTask_t tcb_buffer;
} OS_impl_task_internal_record_t;

/* Table where the OS object information is stored */
extern OS_impl_task_internal_record_t OS_impl_task_table[OS_MAX_TASKS];

#endif /* OS_IMPL_TASKS_H */
