/**
 * \file
 *
 * Purpose: OSAL file system implementation on top of littlefs.
 *
 *          The port has one volume, the BSP's storage device
 *          (OS_BSP_StorageConfig), so only one OSAL file system can be
 *          mounted at a time. Every call into littlefs goes through the lock
 *          in this file, because the library is not thread safe and the
 *          storage configuration is const, so it cannot supply its own.
 */

#include "FreeRTOS.h"
#include "semphr.h"

#include "os-shared-filesys.h"
#include "os-shared-idmap.h"

#include "os-impl-filesys.h"

lfs_t OS_FreeRTOS_Lfs;

static StaticSemaphore_t OS_FreeRTOS_LfsMutexBuffer;
static SemaphoreHandle_t OS_FreeRTOS_LfsMutex;
static bool              OS_FreeRTOS_LfsMounted;

/*----------------------------------------------------------------
 *
 *  Purpose: Port-internal helper routine, not part of OSAL API.
 *           See prototype in os-impl-filesys.h.
 *
 *-----------------------------------------------------------------*/
int32 OS_FreeRTOS_FileSysInit(void)
{
    OS_FreeRTOS_LfsMounted = false;

    OS_FreeRTOS_LfsMutex = xSemaphoreCreateMutexStatic(&OS_FreeRTOS_LfsMutexBuffer);
    if (OS_FreeRTOS_LfsMutex == NULL)
    {
        return OS_ERROR;
    }

    return OS_SUCCESS;
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
 *           There is nothing to start: the device belongs to the BSP. This
 *           only checks that the storage configuration fits what the port
 *           reserves for each open file.
 *
 *-----------------------------------------------------------------*/
int32 OS_FileSysStartVolume_Impl(const OS_object_token_t *token)
{
    if (OS_BSP_StorageConfig->cache_size > OS_FREERTOS_LFS_FILE_CACHE_SIZE)
    {
        return OS_FS_ERR_DRIVE_NOT_CREATED;
    }

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_FileSysStopVolume_Impl(const OS_object_token_t *token)
{
    /* Nothing was started, see OS_FileSysStartVolume_Impl */
    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *           Refused while the volume is mounted: formatting would write
 *           under the mounted state in OS_FreeRTOS_Lfs.
 *
 *-----------------------------------------------------------------*/
int32 OS_FileSysFormatVolume_Impl(const OS_object_token_t *token)
{
    int err;

    if (OS_FreeRTOS_LfsMounted)
    {
        return OS_FS_ERR_DEVICE_NOT_FREE;
    }

    OS_FreeRTOS_LfsLock();
    err = lfs_format(&OS_FreeRTOS_Lfs, OS_BSP_StorageConfig);
    OS_FreeRTOS_LfsUnlock();

    return (err < 0) ? OS_ERROR : OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *           There is one lfs_t, so a second mount is refused.
 *
 *-----------------------------------------------------------------*/
int32 OS_FileSysMountVolume_Impl(const OS_object_token_t *token)
{
    int err;

    if (OS_FreeRTOS_LfsMounted)
    {
        return OS_FS_ERR_DEVICE_NOT_FREE;
    }

    OS_FreeRTOS_LfsLock();
    err = lfs_mount(&OS_FreeRTOS_Lfs, OS_BSP_StorageConfig);
    OS_FreeRTOS_LfsUnlock();

    if (err < 0)
    {
        return OS_ERROR;
    }

    OS_FreeRTOS_LfsMounted = true;

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_FileSysUnmountVolume_Impl(const OS_object_token_t *token)
{
    int err;

    OS_FreeRTOS_LfsLock();
    err = lfs_unmount(&OS_FreeRTOS_Lfs);
    OS_FreeRTOS_LfsUnlock();

    if (err < 0)
    {
        return OS_ERROR;
    }

    OS_FreeRTOS_LfsMounted = false;

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
