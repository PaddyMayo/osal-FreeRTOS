/*
 * File: os-impl-dir.h
 *
 * Purpose:
 *   Internal directory handle table for the FreeRTOS OSAL port.
 *
 *   Lives in a header, like the other tables, so the coverage-test can reach
 *   it through the real type.
 */

#ifndef OS_IMPL_DIR_H
#define OS_IMPL_DIR_H

#include "osconfig.h"
#include "common_types.h"

#include "lfs.h"

#include "os-impl-filesys.h"

/*
 * littlefs links every open directory into the mounted file system, so the
 * lfs_dir_t has to stay where it was opened until it is closed: it lives in
 * this table, never on the stack of the caller.
 */
typedef struct
{
    lfs_dir_t dir;
} OS_impl_dir_internal_record_t;

/* The global directory handle table, indexed like OS_dir_table */
extern OS_impl_dir_internal_record_t OS_impl_dir_table[OS_MAX_NUM_OPEN_DIRS];

#endif /* OS_IMPL_DIR_H */
