/*
 * File: freertos-filesys-stubs.c
 *
 * Purpose:
 *   UT-controllable stand-ins for littlefs's volume calls and the fake
 *   FreeRTOS mutex API, used to white-box test freertos/src/os-impl-filesys.c
 *   without a real library or kernel. Same pattern as freertos-queue-stubs.c:
 *   UT_DEFAULT_IMPL() lets a test script a failure with
 *   UT_SetDeferredRetcode(), otherwise success is the default. Arguments
 *   worth asserting on are captured into the extern globals below.
 */

#include "utstubs.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include "osapi-error.h"

#include "lfs.h"

/* The storage configuration the module under test uses; a test may change it */
struct lfs_config              UT_Stub_StorageConfig;
const struct lfs_config *const OS_BSP_StorageConfig = &UT_Stub_StorageConfig;

/* What lfs_fs_stat() and lfs_fs_size() report */
lfs_size_t  UT_Stub_FsBlockSize;
lfs_size_t  UT_Stub_FsBlockCount;
lfs_ssize_t UT_Stub_FsBlocksUsed;

/* Captured arguments */
lfs_t                   *UT_Stub_LastLfs;
const struct lfs_config *UT_Stub_LastConfig;

StaticSemaphore_t *UT_Stub_LastCreateMutexBuffer;
SemaphoreHandle_t  UT_Stub_LastTakeSem;
TickType_t         UT_Stub_LastTakeTicks;
SemaphoreHandle_t  UT_Stub_LastGiveSem;

SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *pxMutexBuffer)
{
    int32 status;

    UT_Stub_LastCreateMutexBuffer = pxMutexBuffer;

    status = UT_DEFAULT_IMPL(xSemaphoreCreateMutexStatic);

    if (status < 0)
    {
        return NULL;
    }

    return (SemaphoreHandle_t)pxMutexBuffer;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xBlockTime)
{
    UT_Stub_LastTakeSem   = xSemaphore;
    UT_Stub_LastTakeTicks = xBlockTime;

    UT_DEFAULT_IMPL(xSemaphoreTake);

    return pdTRUE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore)
{
    UT_Stub_LastGiveSem = xSemaphore;

    UT_DEFAULT_IMPL(xSemaphoreGive);

    return pdTRUE;
}

int lfs_format(lfs_t *lfs, const struct lfs_config *config)
{
    UT_Stub_LastLfs    = lfs;
    UT_Stub_LastConfig = config;

    return UT_DEFAULT_IMPL(lfs_format);
}

int lfs_mount(lfs_t *lfs, const struct lfs_config *config)
{
    UT_Stub_LastLfs    = lfs;
    UT_Stub_LastConfig = config;

    return UT_DEFAULT_IMPL(lfs_mount);
}

int lfs_unmount(lfs_t *lfs)
{
    UT_Stub_LastLfs = lfs;

    return UT_DEFAULT_IMPL(lfs_unmount);
}

int lfs_fs_mkconsistent(lfs_t *lfs)
{
    UT_Stub_LastLfs = lfs;

    return UT_DEFAULT_IMPL(lfs_fs_mkconsistent);
}

int lfs_fs_stat(lfs_t *lfs, struct lfs_fsinfo *fsinfo)
{
    int status;

    UT_Stub_LastLfs = lfs;

    status = UT_DEFAULT_IMPL(lfs_fs_stat);

    if (status >= 0)
    {
        fsinfo->block_size  = UT_Stub_FsBlockSize;
        fsinfo->block_count = UT_Stub_FsBlockCount;
    }

    return status;
}

lfs_ssize_t lfs_fs_size(lfs_t *lfs)
{
    int status;

    UT_Stub_LastLfs = lfs;

    status = UT_DEFAULT_IMPL(lfs_fs_size);

    if (status < 0)
    {
        return status;
    }

    return UT_Stub_FsBlocksUsed;
}
