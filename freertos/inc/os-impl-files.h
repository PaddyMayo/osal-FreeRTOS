/*
 * File: os-impl-files.h
 *
 * Purpose:
 *   Internal file handle table for the FreeRTOS OSAL port.
 *
 *   Lives in a header, like the other tables, so the coverage-test adaptor
 *   can reach it through the real type.
 */

#ifndef OS_IMPL_FILES_H
#define OS_IMPL_FILES_H

#include "osconfig.h"
#include "common_types.h"

#include "lfs.h"

#include "os-impl-filesys.h"

/*
 * LFS_NO_MALLOC is set, so every open file brings the cache littlefs would
 * otherwise allocate: the config points at it and both stay with the handle.
 */
typedef struct
{
    lfs_file_t             file;
    struct lfs_file_config config;
    uint8_t                cache[OS_FREERTOS_LFS_FILE_CACHE_SIZE];
} OS_impl_file_internal_record_t;

/* The global file handle table, indexed like OS_stream_table */
extern OS_impl_file_internal_record_t OS_impl_filehandle_table[OS_MAX_NUM_OPEN_FILES];

/*----------------------------------------------------------------
 *
 *  Purpose: Port-internal helper routine, not part of OSAL API.
 *           Clears the file handle table. Called from OS_API_Impl_Init.
 *
 *  Returns: OS_SUCCESS
 *
 *-----------------------------------------------------------------*/
int32 OS_FreeRTOS_FileInit(void);

#endif /* OS_IMPL_FILES_H */
