/*
 * File: freertos-files-stubs.c
 *
 * Purpose:
 *   UT-controllable stand-ins for littlefs's file calls and for the lock in
 *   os-impl-filesys.c, used to white-box test freertos/src/os-impl-files.c
 *   without a real library. Same pattern as freertos-queue-stubs.c:
 *   UT_DEFAULT_IMPL() lets a test script a result with
 *   UT_SetDeferredRetcode() or UT_SetDefaultReturnValue(), otherwise 0 (so
 *   success, and no bytes for a read or write) is the default. Arguments worth
 *   asserting on are captured into the extern globals below.
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
lfs_t                        *UT_Stub_LastLfs;
lfs_file_t                   *UT_Stub_LastFile;
const char                   *UT_Stub_LastPath;
const char                   *UT_Stub_LastNewPath;
int                           UT_Stub_LastOpenFlags;
const struct lfs_file_config *UT_Stub_LastFileConfig;
const void                   *UT_Stub_LastBuffer;
lfs_size_t                    UT_Stub_LastSize;
lfs_soff_t                    UT_Stub_LastSeekOffset;
int                           UT_Stub_LastSeekWhence;

/* What lfs_stat() reports */
struct lfs_info UT_Stub_StatInfo;

int lfs_file_opencfg(lfs_t *lfs, lfs_file_t *file, const char *path, int flags, const struct lfs_file_config *config)
{
    UT_Stub_LastLfs        = lfs;
    UT_Stub_LastFile       = file;
    UT_Stub_LastPath       = path;
    UT_Stub_LastOpenFlags  = flags;
    UT_Stub_LastFileConfig = config;

    return UT_DEFAULT_IMPL(lfs_file_opencfg);
}

int lfs_file_close(lfs_t *lfs, lfs_file_t *file)
{
    UT_Stub_LastLfs  = lfs;
    UT_Stub_LastFile = file;

    return UT_DEFAULT_IMPL(lfs_file_close);
}

// The signature is littlefs's, the buffer parameter cannot be const
// cppcheck-suppress constParameterCallback
lfs_ssize_t lfs_file_read(lfs_t *lfs, lfs_file_t *file, void *buffer, lfs_size_t size)
{
    UT_Stub_LastLfs    = lfs;
    UT_Stub_LastFile   = file;
    UT_Stub_LastBuffer = buffer;
    UT_Stub_LastSize   = size;

    return UT_DEFAULT_IMPL(lfs_file_read);
}

lfs_ssize_t lfs_file_write(lfs_t *lfs, lfs_file_t *file, const void *buffer, lfs_size_t size)
{
    UT_Stub_LastLfs    = lfs;
    UT_Stub_LastFile   = file;
    UT_Stub_LastBuffer = buffer;
    UT_Stub_LastSize   = size;

    return UT_DEFAULT_IMPL(lfs_file_write);
}

lfs_soff_t lfs_file_seek(lfs_t *lfs, lfs_file_t *file, lfs_soff_t off, int whence)
{
    UT_Stub_LastLfs        = lfs;
    UT_Stub_LastFile       = file;
    UT_Stub_LastSeekOffset = off;
    UT_Stub_LastSeekWhence = whence;

    return UT_DEFAULT_IMPL(lfs_file_seek);
}

int lfs_file_truncate(lfs_t *lfs, lfs_file_t *file, lfs_off_t size)
{
    UT_Stub_LastLfs  = lfs;
    UT_Stub_LastFile = file;
    UT_Stub_LastSize = size;

    return UT_DEFAULT_IMPL(lfs_file_truncate);
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

int lfs_rename(lfs_t *lfs, const char *oldpath, const char *newpath)
{
    UT_Stub_LastLfs     = lfs;
    UT_Stub_LastPath    = oldpath;
    UT_Stub_LastNewPath = newpath;

    return UT_DEFAULT_IMPL(lfs_rename);
}
