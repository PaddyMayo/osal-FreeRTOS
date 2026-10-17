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

#include <string.h>

#include "utstubs.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include "osapi-error.h"

#include "lfs.h"

#include "generic_freertos_bsp_internal.h"

/* The storage configuration the module under test uses; a test may change it */
struct lfs_config UT_Stub_StorageConfig;

OS_BSP_GenericFreeRTOSGlobalData_t OS_BSP_GenericFreeRTOSGlobal = {
    .StorageConfig = &UT_Stub_StorageConfig,
};

/* What lfs_fs_stat() and lfs_fs_size() report */
lfs_size_t  UT_Stub_FsBlockSize;
lfs_size_t  UT_Stub_FsBlockCount;
lfs_ssize_t UT_Stub_FsBlocksUsed;

/* What lfs_stat() reports */
struct lfs_info UT_Stub_StatInfo;

/* Captured arguments */
lfs_t                   *UT_Stub_LastLfs;
const struct lfs_config *UT_Stub_LastConfig;
const char              *UT_Stub_LastPath;

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

int lfs_stat(lfs_t *lfs, const char *path, struct lfs_info *info)
{
    int status;

    UT_Stub_LastLfs  = lfs;
    UT_Stub_LastPath = path;

    status = UT_DEFAULT_IMPL(lfs_stat);

    if (status >= 0)
    {
        memcpy(info, &UT_Stub_StatInfo, sizeof(*info));
    }

    return status;
}

int lfs_mkdir(lfs_t *lfs, const char *path)
{
    UT_Stub_LastLfs  = lfs;
    UT_Stub_LastPath = path;

    return UT_DEFAULT_IMPL(lfs_mkdir);
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
