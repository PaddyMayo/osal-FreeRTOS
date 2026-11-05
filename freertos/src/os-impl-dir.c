/**
 * \file
 *
 * Purpose: OSAL directory implementation on top of littlefs.
 *
 *          Each open OSAL directory is an entry of OS_impl_dir_table, found by
 *          the index of the directory's token, and holds the lfs_dir_t that
 *          littlefs keeps linked into the mounted file system until the
 *          directory is closed. The paths arrive already translated to the
 *          littlefs namespace (see os-impl-filesys.c). Every call into
 *          littlefs is made under the lock from os-impl-filesys.c.
 *
 *          Like POSIX's readdir(), a directory yields "." and ".." first.
 *          littlefs keeps no permissions, so the access mode of OS_mkdir() has
 *          nothing to apply to and is ignored.
 */

#include <string.h>

#include "os-shared-dir.h"
#include "os-shared-idmap.h"

#include "os-impl-dir.h"

OS_impl_dir_internal_record_t OS_impl_dir_table[OS_MAX_NUM_OPEN_DIRS];

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *           A directory that is already there is a success, as it is for the
 *           POSIX port, but a file of that name is not.
 *
 *-----------------------------------------------------------------*/
int32 OS_DirCreate_Impl(const char *local_path, uint32 access)
{
    struct lfs_info info;
    int             err;

    OS_FreeRTOS_LfsLock();
    err = lfs_mkdir(&OS_FreeRTOS_Lfs, local_path);
    if (err == LFS_ERR_EXIST)
    {
        err = lfs_stat(&OS_FreeRTOS_Lfs, local_path, &info);
        if (err >= 0 && info.type != LFS_TYPE_DIR)
        {
            err = LFS_ERR_NOTDIR;
        }
    }
    OS_FreeRTOS_LfsUnlock();

    return (err < 0) ? OS_ERROR : OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_DirOpen_Impl(const OS_object_token_t *token, const char *local_path)
{
    OS_impl_dir_internal_record_t *impl;
    int                            err;

    impl = OS_OBJECT_TABLE_GET(OS_impl_dir_table, *token);

    OS_FreeRTOS_LfsLock();
    err = lfs_dir_open(&OS_FreeRTOS_Lfs, &impl->dir, local_path);
    OS_FreeRTOS_LfsUnlock();

    return (err < 0) ? OS_ERROR : OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_DirClose_Impl(const OS_object_token_t *token)
{
    OS_impl_dir_internal_record_t *impl;
    int                            err;

    impl = OS_OBJECT_TABLE_GET(OS_impl_dir_table, *token);

    OS_FreeRTOS_LfsLock();
    err = lfs_dir_close(&OS_FreeRTOS_Lfs, &impl->dir);
    OS_FreeRTOS_LfsUnlock();

    return (err < 0) ? OS_ERROR : OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *           A name longer than the caller's buffer is cut to fit, as the
 *           POSIX port does.
 *
 *-----------------------------------------------------------------*/
int32 OS_DirRead_Impl(const OS_object_token_t *token, os_dirent_t *dirent)
{
    OS_impl_dir_internal_record_t *impl;
    struct lfs_info                info;
    int                            result;

    impl = OS_OBJECT_TABLE_GET(OS_impl_dir_table, *token);

    OS_FreeRTOS_LfsLock();
    result = lfs_dir_read(&OS_FreeRTOS_Lfs, &impl->dir, &info);
    OS_FreeRTOS_LfsUnlock();

    /* Zero is the end of the directory, which OSAL reports as an error too */
    if (result <= 0)
    {
        return OS_ERROR;
    }

    strncpy(dirent->FileName, info.name, sizeof(dirent->FileName) - 1);
    dirent->FileName[sizeof(dirent->FileName) - 1] = 0;

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_DirRewind_Impl(const OS_object_token_t *token)
{
    OS_impl_dir_internal_record_t *impl;
    int                            err;

    impl = OS_OBJECT_TABLE_GET(OS_impl_dir_table, *token);

    OS_FreeRTOS_LfsLock();
    err = lfs_dir_rewind(&OS_FreeRTOS_Lfs, &impl->dir);
    OS_FreeRTOS_LfsUnlock();

    return (err < 0) ? OS_ERROR : OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *           lfs_remove() takes away a file as readily as an empty directory,
 *           so the entry is checked to be a directory first, under the same
 *           lock so that nothing can replace it in between.
 *
 *-----------------------------------------------------------------*/
int32 OS_DirRemove_Impl(const char *local_path)
{
    struct lfs_info info;
    int             err;

    OS_FreeRTOS_LfsLock();
    err = lfs_stat(&OS_FreeRTOS_Lfs, local_path, &info);
    if (err >= 0)
    {
        if (info.type == LFS_TYPE_DIR)
        {
            err = lfs_remove(&OS_FreeRTOS_Lfs, local_path);
        }
        else
        {
            err = LFS_ERR_NOTDIR;
        }
    }
    OS_FreeRTOS_LfsUnlock();

    return (err < 0) ? OS_ERROR : OS_SUCCESS;
}
