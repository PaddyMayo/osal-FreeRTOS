/**
 * \file
 *
 * Purpose: OSAL file implementation on top of littlefs.
 *
 *          Each open OSAL file is an entry of OS_impl_filehandle_table, found
 *          by the index of the file's token, and holds the lfs_file_t and the
 *          cache littlefs needs for it (LFS_NO_MALLOC is set, so littlefs
 *          never allocates one). Regular files only: the timeout of read and
 *          write has no meaning for them and is ignored. Every call into
 *          littlefs is made under the lock from os-impl-filesys.c.
 */

#include <string.h>

#include "os-shared-file.h"
#include "os-shared-idmap.h"

#include "os-impl-files.h"

OS_impl_file_internal_record_t OS_impl_filehandle_table[OS_MAX_NUM_OPEN_FILES];

/* Indexed by the OSAL access mode: OS_READ_ONLY, OS_WRITE_ONLY, OS_READ_WRITE */
static const int OS_FreeRTOS_LfsAccess[] = { LFS_O_RDONLY, LFS_O_WRONLY, LFS_O_RDWR };

/*----------------------------------------------------------------
 *
 *  Purpose: Port-internal helper routine, not part of OSAL API.
 *           See prototype in os-impl-files.h.
 *
 *-----------------------------------------------------------------*/
int32 OS_FreeRTOS_FileInit(void)
{
    memset(OS_impl_filehandle_table, 0, sizeof(OS_impl_filehandle_table));

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_FileOpen_Impl(const OS_object_token_t *token, const char *local_path, int32 flags, int32 access_mode)
{
    OS_impl_file_internal_record_t *impl;
    int                             lfs_flags;
    int                             err;

    impl = OS_OBJECT_TABLE_GET(OS_impl_filehandle_table, *token);

    /* OS_OpenCreate has already checked access_mode, so it indexes the table */
    lfs_flags = OS_FreeRTOS_LfsAccess[access_mode];

    if ((flags & OS_FILE_FLAG_CREATE) != 0)
    {
        lfs_flags |= LFS_O_CREAT;
    }

    if ((flags & OS_FILE_FLAG_TRUNCATE) != 0)
    {
        lfs_flags |= LFS_O_TRUNC;
    }

    memset(&impl->config, 0, sizeof(impl->config));
    impl->config.buffer = impl->cache;

    OS_FreeRTOS_LfsLock();
    err = lfs_file_opencfg(&OS_FreeRTOS_Lfs, &impl->file, local_path, lfs_flags, &impl->config);
    OS_FreeRTOS_LfsUnlock();

    return (err < 0) ? OS_ERROR : OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_FileAllocate_Impl(const OS_object_token_t *token, osal_offset_t offset, osal_offset_t len)
{
    /* littlefs cannot reserve space ahead of a write */
    return OS_ERR_NOT_IMPLEMENTED;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_FileTruncate_Impl(const OS_object_token_t *token, osal_offset_t len)
{
    OS_impl_file_internal_record_t *impl;
    int                             err;

    impl = OS_OBJECT_TABLE_GET(OS_impl_filehandle_table, *token);

    OS_FreeRTOS_LfsLock();
    err = lfs_file_truncate(&OS_FreeRTOS_Lfs, &impl->file, (lfs_off_t)len);
    OS_FreeRTOS_LfsUnlock();

    return (err < 0) ? OS_ERROR : OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_ShellOutputToFile_Impl(const OS_object_token_t *token, const char *Cmd)
{
    /* There is no shell */
    return OS_ERR_NOT_IMPLEMENTED;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_GenericSeek_Impl(const OS_object_token_t *token, osal_offset_t offset, uint32 whence)
{
    OS_impl_file_internal_record_t *impl;
    lfs_soff_t                      position;

    /* The OS_SEEK_ values are littlefs's own, but anything else is not a whence */
    if (whence > OS_SEEK_END)
    {
        return OS_ERROR;
    }

    impl = OS_OBJECT_TABLE_GET(OS_impl_filehandle_table, *token);

    OS_FreeRTOS_LfsLock();
    position = lfs_file_seek(&OS_FreeRTOS_Lfs, &impl->file, (lfs_soff_t)offset, (int)whence);
    OS_FreeRTOS_LfsUnlock();

    if (position < 0)
    {
        return OS_ERROR;
    }

    return (int32)position;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_GenericRead_Impl(const OS_object_token_t *token, void *buffer, size_t nbytes, OS_time_t abs_timeout)
{
    OS_impl_file_internal_record_t *impl;
    lfs_ssize_t                     count;

    impl = OS_OBJECT_TABLE_GET(OS_impl_filehandle_table, *token);

    OS_FreeRTOS_LfsLock();
    count = lfs_file_read(&OS_FreeRTOS_Lfs, &impl->file, buffer, (lfs_size_t)nbytes);
    OS_FreeRTOS_LfsUnlock();

    if (count < 0)
    {
        return OS_ERROR;
    }

    return (int32)count;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_GenericWrite_Impl(const OS_object_token_t *token, const void *buffer, size_t nbytes, OS_time_t abs_timeout)
{
    OS_impl_file_internal_record_t *impl;
    lfs_ssize_t                     count;

    impl = OS_OBJECT_TABLE_GET(OS_impl_filehandle_table, *token);

    OS_FreeRTOS_LfsLock();
    count = lfs_file_write(&OS_FreeRTOS_Lfs, &impl->file, buffer, (lfs_size_t)nbytes);
    OS_FreeRTOS_LfsUnlock();

    if (count < 0)
    {
        return OS_ERROR;
    }

    return (int32)count;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *           Closing writes out what the cache still holds, as a sync would.
 *
 *-----------------------------------------------------------------*/
int32 OS_GenericClose_Impl(const OS_object_token_t *token)
{
    OS_impl_file_internal_record_t *impl;
    int                             err;

    impl = OS_OBJECT_TABLE_GET(OS_impl_filehandle_table, *token);

    OS_FreeRTOS_LfsLock();
    err = lfs_file_close(&OS_FreeRTOS_Lfs, &impl->file);
    OS_FreeRTOS_LfsUnlock();

    return (err < 0) ? OS_ERROR : OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *           littlefs keeps no permissions and no times, so only the size and
 *           whether the entry is a directory are real.
 *
 *-----------------------------------------------------------------*/
int32 OS_FileStat_Impl(const char *local_path, os_fstat_t *filestat)
{
    struct lfs_info info;
    int             err;

    OS_FreeRTOS_LfsLock();
    err = lfs_stat(&OS_FreeRTOS_Lfs, local_path, &info);
    OS_FreeRTOS_LfsUnlock();

    if (err < 0)
    {
        return OS_ERROR;
    }

    memset(filestat, 0, sizeof(*filestat));
    filestat->FileSize     = info.size;
    filestat->FileModeBits = OS_FILESTAT_MODE_READ | OS_FILESTAT_MODE_WRITE;

    if (info.type == LFS_TYPE_DIR)
    {
        filestat->FileModeBits |= OS_FILESTAT_MODE_DIR;
    }

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_FileRemove_Impl(const char *local_path)
{
    int err;

    OS_FreeRTOS_LfsLock();
    err = lfs_remove(&OS_FreeRTOS_Lfs, local_path);
    OS_FreeRTOS_LfsUnlock();

    return (err < 0) ? OS_ERROR : OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_FileRename_Impl(const char *old_path, const char *new_path)
{
    int err;

    OS_FreeRTOS_LfsLock();
    err = lfs_rename(&OS_FreeRTOS_Lfs, old_path, new_path);
    OS_FreeRTOS_LfsUnlock();

    return (err < 0) ? OS_ERROR : OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_FileChmod_Impl(const char *local_path, uint32 access_mode)
{
    /* littlefs keeps no permissions; OSAL's tests accept this */
    return OS_ERR_NOT_IMPLEMENTED;
}
