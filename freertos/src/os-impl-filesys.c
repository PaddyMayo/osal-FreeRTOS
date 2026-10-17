/**
 * \file
 *
 * Purpose: OSAL file system implementation on top of littlefs.
 *
 *          The port has one littlefs instance on the BSP's storage device
 *          (OS_BSP_GenericFreeRTOSGlobal.StorageConfig), mounted once by
 *          OS_FreeRTOS_FileSysInit. Each OSAL volume is a directory in the
 *          root of it, named after the volume, like the volatile volumes of
 *          the POSIX and RTEMS ports: the volumes share the free space of the
 *          device, and starting, formatting, mounting or unmounting one leaves
 *          the others alone. Every call into littlefs goes through the lock in
 *          this file, because the library is not thread safe and the storage
 *          configuration has no lock of its own.
 */

#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"

#include "os-shared-filesys.h"
#include "os-shared-idmap.h"

#include "os-impl-filesys.h"

lfs_t OS_FreeRTOS_Lfs;

static StaticSemaphore_t OS_FreeRTOS_LfsMutexBuffer;
static SemaphoreHandle_t OS_FreeRTOS_LfsMutex;

/*----------------------------------------------------------------
 *
 *  Purpose: Port-internal helper routine, not part of OSAL API.
 *           See prototype in os-impl-filesys.h.
 *
 *           Mounts littlefs for the life of the system, the way the POSIX
 *           and RTEMS ports find their file systems mounted before OSAL
 *           starts. A device without a file system (the superblock cannot be
 *           read) is formatted first; any other mount error is left alone,
 *           so that a device that is merely failing is not wiped.
 *
 *-----------------------------------------------------------------*/
int32 OS_FreeRTOS_FileSysInit(void)
{
    int err;

    OS_FreeRTOS_LfsMutex = xSemaphoreCreateMutexStatic(&OS_FreeRTOS_LfsMutexBuffer);
    if (OS_FreeRTOS_LfsMutex == NULL)
    {
        return OS_ERROR;
    }

    OS_FreeRTOS_LfsLock();
    err = lfs_mount(&OS_FreeRTOS_Lfs, OS_BSP_GenericFreeRTOSGlobal.StorageConfig);
    if (err == LFS_ERR_CORRUPT)
    {
        err = lfs_format(&OS_FreeRTOS_Lfs, OS_BSP_GenericFreeRTOSGlobal.StorageConfig);
        if (err >= 0)
        {
            err = lfs_mount(&OS_FreeRTOS_Lfs, OS_BSP_GenericFreeRTOSGlobal.StorageConfig);
        }
    }
    OS_FreeRTOS_LfsUnlock();

    return (err < 0) ? OS_ERROR : OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Port-internal helper routine, not part of OSAL API.
 *           See prototype in os-impl-filesys.h.
 *
 *-----------------------------------------------------------------*/
void OS_FreeRTOS_LfsLock(void)
{
    (void)xSemaphoreTake(OS_FreeRTOS_LfsMutex, portMAX_DELAY);
}

/*----------------------------------------------------------------
 *
 *  Purpose: Port-internal helper routine, not part of OSAL API.
 *           See prototype in os-impl-filesys.h.
 *
 *-----------------------------------------------------------------*/
void OS_FreeRTOS_LfsUnlock(void)
{
    (void)xSemaphoreGive(OS_FreeRTOS_LfsMutex);
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *           The device belongs to the BSP and is mounted already, so there
 *           is nothing to start. This checks that the storage configuration
 *           fits what the port reserves for each open file, then names the
 *           directory of the volume: /<volume name>, unless a system mount
 *           point was given (OS_FileSysAddFixedMap). The name must be one
 *           path component, or the volume would resolve outside its directory.
 *
 *-----------------------------------------------------------------*/
int32 OS_FileSysStartVolume_Impl(const OS_object_token_t *token)
{
    OS_filesys_internal_record_t *filesys;

    if (OS_BSP_GenericFreeRTOSGlobal.StorageConfig->cache_size > OS_FREERTOS_LFS_FILE_CACHE_SIZE)
    {
        return OS_FS_ERR_DRIVE_NOT_CREATED;
    }

    filesys = OS_OBJECT_TABLE_GET(OS_filesys_table, *token);
    if (filesys->system_mountpt[0] == 0)
    {
        if (filesys->volume_name[0] == 0 || strchr(filesys->volume_name, '/') != NULL)
        {
            return OS_FS_ERR_PATH_INVALID;
        }

        filesys->system_mountpt[0] = '/';
        strncpy(&filesys->system_mountpt[1], filesys->volume_name, sizeof(filesys->system_mountpt) - 2);
        filesys->system_mountpt[sizeof(filesys->system_mountpt) - 1] = 0;
    }

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *           Nothing was started, see OS_FileSysStartVolume_Impl.
 *
 *-----------------------------------------------------------------*/
int32 OS_FileSysStopVolume_Impl(const OS_object_token_t *token)
{
    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *           A no-op, as on the POSIX and RTEMS ports for a volume that is a
 *           directory in a file system that already exists: clearing the
 *           directory could remove files that should stay, and the device
 *           is shared with the other volumes.
 *
 *-----------------------------------------------------------------*/
int32 OS_FileSysFormatVolume_Impl(const OS_object_token_t *token)
{
    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *           Makes sure the directory of the volume exists, creating it if
 *           it does not, as the POSIX and RTEMS ports do for their mount
 *           point. littlefs itself stays mounted.
 *
 *-----------------------------------------------------------------*/
int32 OS_FileSysMountVolume_Impl(const OS_object_token_t *token)
{
    OS_filesys_internal_record_t *filesys;
    struct lfs_info               info;
    int32                         return_code;
    int                           err;

    filesys     = OS_OBJECT_TABLE_GET(OS_filesys_table, *token);
    return_code = OS_SUCCESS;

    OS_FreeRTOS_LfsLock();
    err = lfs_stat(&OS_FreeRTOS_Lfs, filesys->system_mountpt, &info);
    if (err < 0)
    {
        err = lfs_mkdir(&OS_FreeRTOS_Lfs, filesys->system_mountpt);
        if (err < 0)
        {
            return_code = OS_FS_ERR_DRIVE_NOT_CREATED;
        }
    }
    else if (info.type != LFS_TYPE_DIR)
    {
        return_code = OS_FS_ERR_DRIVE_NOT_CREATED;
    }
    OS_FreeRTOS_LfsUnlock();

    return return_code;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *           A no-op, as on the POSIX and RTEMS ports: the directory stays
 *           for the next mount, and littlefs stays mounted for the other
 *           volumes.
 *
 *-----------------------------------------------------------------*/
int32 OS_FileSysUnmountVolume_Impl(const OS_object_token_t *token)
{
    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *           littlefs has no check of the whole volume. Repairing makes the
 *           file system consistent after an interrupted write, which is the
 *           closest it gets; a check without repair is not available.
 *
 *-----------------------------------------------------------------*/
int32 OS_FileSysCheckVolume_Impl(const OS_object_token_t *token, bool repair)
{
    int err;

    if (!repair)
    {
        return OS_ERR_NOT_IMPLEMENTED;
    }

    OS_FreeRTOS_LfsLock();
    err = lfs_fs_mkconsistent(&OS_FreeRTOS_Lfs);
    OS_FreeRTOS_LfsUnlock();

    return (err < 0) ? OS_ERROR : OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_FileSysStatVolume_Impl(const OS_object_token_t *token, OS_statvfs_t *result)
{
    struct lfs_fsinfo info;
    lfs_ssize_t       used;
    int               err;

    OS_FreeRTOS_LfsLock();
    err  = lfs_fs_stat(&OS_FreeRTOS_Lfs, &info);
    used = (err < 0) ? err : lfs_fs_size(&OS_FreeRTOS_Lfs);
    OS_FreeRTOS_LfsUnlock();

    if (used < 0)
    {
        return OS_ERROR;
    }

    result->block_size   = info.block_size;
    result->total_blocks = info.block_count;
    result->blocks_free  = info.block_count - (lfs_size_t)used;

    return OS_SUCCESS;
}
