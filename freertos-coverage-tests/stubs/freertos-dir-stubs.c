/*
 * File: freertos-dir-stubs.c
 *
 * Purpose:
 *   UT-controllable stand-ins for littlefs's directory calls and for the lock
 *   in os-impl-filesys.c, used to white-box test freertos/src/os-impl-dir.c
 *   without a real library. Same pattern as freertos-files-stubs.c:
 *   UT_DEFAULT_IMPL() lets a test script a result with
 *   UT_SetDeferredRetcode() or UT_SetDefaultReturnValue(), otherwise 0 (so
 *   success, and the end of the directory for a read) is the default.
 *   Arguments worth asserting on are captured into the extern globals below.
 */

#include <string.h>

#include "utstubs.h"

#include "lfs.h"

/* Defined in os-impl-filesys.c, which this runner does not link */
lfs_t OS_FreeRTOS_Lfs;

uint32 UT_Stub_LockCount;
uint32 UT_Stub_UnlockCount;

void OS_FreeRTOS_LfsLock(void)
{
    ++UT_Stub_LockCount;
}

void OS_FreeRTOS_LfsUnlock(void)
{
    ++UT_Stub_UnlockCount;
}

/* Captured arguments */
lfs_t      *UT_Stub_LastLfs;
lfs_dir_t  *UT_Stub_LastDir;
const char *UT_Stub_LastPath;

/* What lfs_stat() reports */
struct lfs_info UT_Stub_StatInfo;

/* What lfs_dir_read() reports when it yields an entry */
struct lfs_info UT_Stub_ReadInfo;

int lfs_mkdir(lfs_t *lfs, const char *path)
{
    UT_Stub_LastLfs  = lfs;
    UT_Stub_LastPath = path;

    return UT_DEFAULT_IMPL(lfs_mkdir);
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

int lfs_remove(lfs_t *lfs, const char *path)
{
    UT_Stub_LastLfs  = lfs;
    UT_Stub_LastPath = path;

    return UT_DEFAULT_IMPL(lfs_remove);
}

int lfs_dir_open(lfs_t *lfs, lfs_dir_t *dir, const char *path)
{
    UT_Stub_LastLfs  = lfs;
    UT_Stub_LastDir  = dir;
    UT_Stub_LastPath = path;

    return UT_DEFAULT_IMPL(lfs_dir_open);
}

int lfs_dir_close(lfs_t *lfs, lfs_dir_t *dir)
{
    UT_Stub_LastLfs = lfs;
    UT_Stub_LastDir = dir;

    return UT_DEFAULT_IMPL(lfs_dir_close);
}

int lfs_dir_read(lfs_t *lfs, lfs_dir_t *dir, struct lfs_info *info)
{
    int status;

    UT_Stub_LastLfs = lfs;
    UT_Stub_LastDir = dir;

    status = UT_DEFAULT_IMPL(lfs_dir_read);

    if (status > 0)
    {
        memcpy(info, &UT_Stub_ReadInfo, sizeof(*info));
    }

    return status;
}

int lfs_dir_rewind(lfs_t *lfs, lfs_dir_t *dir)
{
    UT_Stub_LastLfs = lfs;
    UT_Stub_LastDir = dir;

    return UT_DEFAULT_IMPL(lfs_dir_rewind);
}
