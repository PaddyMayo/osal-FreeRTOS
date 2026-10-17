/*
 * File: coveragetest-filesys.c
 *
 * Purpose:
 *   White-box coverage test cases for freertos/src/os-impl-filesys.c, run
 *   against the fake FreeRTOS mutex API in ../fake-inc and the littlefs
 *   stand-ins in ../stubs/freertos-filesys-stubs.c - no real library or
 *   kernel involved. (The black box tests for this port are OSAL's own
 *   /src/tests and /src/unit-tests, which link the real OSAL library.)
 *
 *   Each _Impl function is called directly, and effects are observed via
 *   return codes, the system mount point the volume record ends up with, and
 *   the stubs' captured call arguments and counts.
 */

#include <string.h>

#include "utassert.h"
#include "uttest.h"
#include "utstubs.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include "os-shared-filesys.h"
#include "os-shared-idmap.h"

#include "os-impl-filesys.h"

/* Captured stub call arguments and scripted values, declared in freertos-filesys-stubs.c */
extern struct lfs_config        UT_Stub_StorageConfig;
extern lfs_size_t               UT_Stub_FsBlockSize;
extern lfs_size_t               UT_Stub_FsBlockCount;
extern lfs_ssize_t              UT_Stub_FsBlocksUsed;
extern struct lfs_info          UT_Stub_StatInfo;
extern lfs_t                   *UT_Stub_LastLfs;
extern const struct lfs_config *UT_Stub_LastConfig;
extern const char              *UT_Stub_LastPath;
extern StaticSemaphore_t       *UT_Stub_LastCreateMutexBuffer;
extern SemaphoreHandle_t        UT_Stub_LastTakeSem;
extern TickType_t               UT_Stub_LastTakeTicks;
extern SemaphoreHandle_t        UT_Stub_LastGiveSem;

#define UT_FILESYS_TOKEN(idx)                                                                              \
    (OS_object_token_t)                                                                                    \
    {                                                                                                      \
        .obj_type = OS_OBJECT_TYPE_OS_FILESYS, .obj_id = (osal_id_t) { 0xB0000 + (idx) }, .obj_idx = (idx) \
    }

/*
**********************************************************************************
**          TEST CASES
**********************************************************************************
*/

void Test_OS_FreeRTOS_FileSysInit(void)
{
    /* the lock is created in a static buffer, then the storage is mounted into the one lfs_t under it */
    UtAssert_INT32_EQ(OS_FreeRTOS_FileSysInit(), OS_SUCCESS);
    UtAssert_STUB_COUNT(xSemaphoreCreateMutexStatic, 1);
    UtAssert_True(UT_Stub_LastCreateMutexBuffer != NULL, "lock created in a static buffer");
    UtAssert_STUB_COUNT(lfs_mount, 1);
    UtAssert_True(UT_Stub_LastLfs == &OS_FreeRTOS_Lfs, "lfs_mount() called with the volume");
    UtAssert_True(UT_Stub_LastConfig == OS_BSP_GenericFreeRTOSGlobal.StorageConfig,
                  "lfs_mount() called with the storage configuration");
    UtAssert_STUB_COUNT(xSemaphoreTake, 1);
    UtAssert_STUB_COUNT(xSemaphoreGive, 1);
    UtAssert_STUB_COUNT(lfs_format, 0);

    /* xSemaphoreCreateMutexStatic() failure -> OS_ERROR, nothing mounted */
    UT_ResetState(0);
    UT_SetDeferredRetcode(UT_KEY(xSemaphoreCreateMutexStatic), 1, -1);
    UtAssert_INT32_EQ(OS_FreeRTOS_FileSysInit(), OS_ERROR);
    UtAssert_STUB_COUNT(xSemaphoreCreateMutexStatic, 1);
    UtAssert_STUB_COUNT(lfs_mount, 0);

    /* no file system on the device -> formatted, then mounted again */
    UT_ResetState(0);
    UT_SetDeferredRetcode(UT_KEY(lfs_mount), 1, LFS_ERR_CORRUPT);
    UtAssert_INT32_EQ(OS_FreeRTOS_FileSysInit(), OS_SUCCESS);
    UtAssert_STUB_COUNT(lfs_mount, 2);
    UtAssert_STUB_COUNT(lfs_format, 1);
    UtAssert_True(UT_Stub_LastLfs == &OS_FreeRTOS_Lfs, "lfs_format() called with the volume");

    /* formatting fails -> OS_ERROR, without a second mount */
    UT_ResetState(0);
    UT_SetDeferredRetcode(UT_KEY(lfs_mount), 1, LFS_ERR_CORRUPT);
    UT_SetDeferredRetcode(UT_KEY(lfs_format), 1, LFS_ERR_IO);
    UtAssert_INT32_EQ(OS_FreeRTOS_FileSysInit(), OS_ERROR);
    UtAssert_STUB_COUNT(lfs_mount, 1);
    UtAssert_STUB_COUNT(lfs_format, 1);

    /* mounting after the format fails -> OS_ERROR */
    UT_ResetState(0);
    UT_SetDeferredRetcode(UT_KEY(lfs_mount), 1, LFS_ERR_CORRUPT);
    UT_SetDeferredRetcode(UT_KEY(lfs_mount), 1, LFS_ERR_IO); /* queued behind the first */
    UtAssert_INT32_EQ(OS_FreeRTOS_FileSysInit(), OS_ERROR);
    UtAssert_STUB_COUNT(lfs_mount, 2);
    UtAssert_STUB_COUNT(lfs_format, 1);

    /* any other mount error -> OS_ERROR, and the device is not formatted */
    UT_ResetState(0);
    UT_SetDeferredRetcode(UT_KEY(lfs_mount), 1, LFS_ERR_IO);
    UtAssert_INT32_EQ(OS_FreeRTOS_FileSysInit(), OS_ERROR);
    UtAssert_STUB_COUNT(lfs_mount, 1);
    UtAssert_STUB_COUNT(lfs_format, 0);
}

void Test_OS_FreeRTOS_LfsLock(void)
{
    UtAssert_INT32_EQ(OS_FreeRTOS_FileSysInit(), OS_SUCCESS);
    UT_ResetState(0);

    /* takes the lock created at init and waits as long as it takes */
    OS_FreeRTOS_LfsLock();
    UtAssert_STUB_COUNT(xSemaphoreTake, 1);
    UtAssert_True(UT_Stub_LastTakeSem == (SemaphoreHandle_t)UT_Stub_LastCreateMutexBuffer,
                  "xSemaphoreTake() called with the lock");
    UtAssert_True(UT_Stub_LastTakeTicks == portMAX_DELAY, "lock waits with portMAX_DELAY");

    OS_FreeRTOS_LfsUnlock();
    UtAssert_STUB_COUNT(xSemaphoreGive, 1);
    UtAssert_True(UT_Stub_LastGiveSem == (SemaphoreHandle_t)UT_Stub_LastCreateMutexBuffer,
                  "xSemaphoreGive() called with the lock");
}

void Test_OS_FileSysStartVolume_Impl(void)
{
    OS_object_token_t token = UT_FILESYS_TOKEN(0);

    /* a cache the files can hold, no mount point given -> the directory named after the volume */
    strcpy(OS_filesys_table[0].volume_name, "RAM");
    UT_Stub_StorageConfig.cache_size = OS_FREERTOS_LFS_FILE_CACHE_SIZE;
    UtAssert_INT32_EQ(OS_FileSysStartVolume_Impl(&token), OS_SUCCESS);
    UtAssert_StrCmp(OS_filesys_table[0].system_mountpt, "/RAM", "system mount point is /<volume name>");

    /* a system mount point already given (fixed map, no volume name) -> kept */
    memset(&OS_filesys_table[0], 0, sizeof(OS_filesys_table[0]));
    strcpy(OS_filesys_table[0].system_mountpt, "/fixed");
    UtAssert_INT32_EQ(OS_FileSysStartVolume_Impl(&token), OS_SUCCESS);
    UtAssert_StrCmp(OS_filesys_table[0].system_mountpt, "/fixed", "given system mount point kept");

    /* no volume name -> refused, nothing set */
    memset(&OS_filesys_table[0], 0, sizeof(OS_filesys_table[0]));
    UtAssert_INT32_EQ(OS_FileSysStartVolume_Impl(&token), OS_FS_ERR_PATH_INVALID);
    UtAssert_StrCmp(OS_filesys_table[0].system_mountpt, "", "no system mount point without a volume name");

    /* a volume name that is more than one path component -> refused, nothing set */
    strcpy(OS_filesys_table[0].volume_name, "a/b");
    UtAssert_INT32_EQ(OS_FileSysStartVolume_Impl(&token), OS_FS_ERR_PATH_INVALID);
    UtAssert_StrCmp(OS_filesys_table[0].system_mountpt, "", "no system mount point for a name with a slash");

    /* a cache larger than the one each open file reserves -> refused, nothing set */
    strcpy(OS_filesys_table[0].volume_name, "RAM");
    UT_Stub_StorageConfig.cache_size = OS_FREERTOS_LFS_FILE_CACHE_SIZE + 1;
    UtAssert_INT32_EQ(OS_FileSysStartVolume_Impl(&token), OS_FS_ERR_DRIVE_NOT_CREATED);
    UtAssert_StrCmp(OS_filesys_table[0].system_mountpt, "", "no system mount point when refused");
}

void Test_OS_FileSysStopVolume_Impl(void)
{
    OS_object_token_t token = UT_FILESYS_TOKEN(0);

    UtAssert_INT32_EQ(OS_FileSysStopVolume_Impl(&token), OS_SUCCESS);
}

void Test_OS_FileSysFormatVolume_Impl(void)
{
    OS_object_token_t token = UT_FILESYS_TOKEN(0);

    /* a no-op: the volume is a directory on the shared device, which is left alone */
    UtAssert_INT32_EQ(OS_FileSysFormatVolume_Impl(&token), OS_SUCCESS);
    UtAssert_STUB_COUNT(lfs_format, 0);
    UtAssert_STUB_COUNT(lfs_mkdir, 0);
}

void Test_OS_FileSysMountVolume_Impl(void)
{
    OS_object_token_t token = UT_FILESYS_TOKEN(0);

    strcpy(OS_filesys_table[0].system_mountpt, "/RAM");

    /* the directory exists -> nothing created, and it was looked up under the lock */
    UT_Stub_StatInfo.type = LFS_TYPE_DIR;
    UtAssert_INT32_EQ(OS_FileSysMountVolume_Impl(&token), OS_SUCCESS);
    UtAssert_STUB_COUNT(lfs_stat, 1);
    UtAssert_STUB_COUNT(lfs_mkdir, 0);
    UtAssert_True(UT_Stub_LastLfs == &OS_FreeRTOS_Lfs, "lfs_stat() called with the volume");
    UtAssert_True(UT_Stub_LastPath == OS_filesys_table[0].system_mountpt, "lfs_stat() called with the mount point");
    UtAssert_STUB_COUNT(xSemaphoreTake, 1);
    UtAssert_STUB_COUNT(xSemaphoreGive, 1);

    /* the directory is missing -> created */
    UT_SetDeferredRetcode(UT_KEY(lfs_stat), 1, LFS_ERR_NOENT);
    UtAssert_INT32_EQ(OS_FileSysMountVolume_Impl(&token), OS_SUCCESS);
    UtAssert_STUB_COUNT(lfs_mkdir, 1);
    UtAssert_True(UT_Stub_LastPath == OS_filesys_table[0].system_mountpt, "lfs_mkdir() called with the mount point");

    /* the directory is missing and cannot be created -> refused */
    UT_SetDeferredRetcode(UT_KEY(lfs_stat), 1, LFS_ERR_NOENT);
    UT_SetDeferredRetcode(UT_KEY(lfs_mkdir), 1, LFS_ERR_NOSPC);
    UtAssert_INT32_EQ(OS_FileSysMountVolume_Impl(&token), OS_FS_ERR_DRIVE_NOT_CREATED);
    UtAssert_STUB_COUNT(lfs_mkdir, 2);

    /* something else is in the way -> refused, nothing created */
    UT_Stub_StatInfo.type = LFS_TYPE_REG;
    UtAssert_INT32_EQ(OS_FileSysMountVolume_Impl(&token), OS_FS_ERR_DRIVE_NOT_CREATED);
    UtAssert_STUB_COUNT(lfs_mkdir, 2);

    /* every lookup released the lock */
    UtAssert_STUB_COUNT(xSemaphoreTake, 4);
    UtAssert_STUB_COUNT(xSemaphoreGive, 4);
}

void Test_OS_FileSysUnmountVolume_Impl(void)
{
    OS_object_token_t token = UT_FILESYS_TOKEN(0);

    /* a no-op: littlefs stays mounted for the other volumes */
    UtAssert_INT32_EQ(OS_FileSysUnmountVolume_Impl(&token), OS_SUCCESS);
    UtAssert_STUB_COUNT(lfs_mkdir, 0);
}

void Test_OS_FileSysCheckVolume_Impl(void)
{
    OS_object_token_t token = UT_FILESYS_TOKEN(0);

    /* no repair -> littlefs has no check */
    UtAssert_INT32_EQ(OS_FileSysCheckVolume_Impl(&token, false), OS_ERR_NOT_IMPLEMENTED);
    UtAssert_STUB_COUNT(lfs_fs_mkconsistent, 0);

    /* repair -> made consistent, under the lock */
    UtAssert_INT32_EQ(OS_FileSysCheckVolume_Impl(&token, true), OS_SUCCESS);
    UtAssert_STUB_COUNT(lfs_fs_mkconsistent, 1);
    UtAssert_True(UT_Stub_LastLfs == &OS_FreeRTOS_Lfs, "lfs_fs_mkconsistent() called with the volume");

    /* lfs_fs_mkconsistent() failure -> OS_ERROR */
    UT_SetDeferredRetcode(UT_KEY(lfs_fs_mkconsistent), 1, LFS_ERR_IO);
    UtAssert_INT32_EQ(OS_FileSysCheckVolume_Impl(&token, true), OS_ERROR);
    UtAssert_STUB_COUNT(lfs_fs_mkconsistent, 2);
}

void Test_OS_FileSysStatVolume_Impl(void)
{
    OS_object_token_t token = UT_FILESYS_TOKEN(0);
    OS_statvfs_t      result;

    UT_Stub_FsBlockSize  = 512;
    UT_Stub_FsBlockCount = 100;
    UT_Stub_FsBlocksUsed = 30;

    /* success -> size and block counts reported, free is what is not used */
    memset(&result, 0, sizeof(result));
    UtAssert_INT32_EQ(OS_FileSysStatVolume_Impl(&token, &result), OS_SUCCESS);
    UtAssert_UINT32_EQ(result.block_size, 512);
    UtAssert_UINT32_EQ(result.total_blocks, 100);
    UtAssert_UINT32_EQ(result.blocks_free, 70);
    UtAssert_True(UT_Stub_LastLfs == &OS_FreeRTOS_Lfs, "stat called with the volume");

    /* lfs_fs_stat() failure -> OS_ERROR, without asking for the size */
    UT_SetDeferredRetcode(UT_KEY(lfs_fs_stat), 1, LFS_ERR_IO);
    UtAssert_INT32_EQ(OS_FileSysStatVolume_Impl(&token, &result), OS_ERROR);
    UtAssert_STUB_COUNT(lfs_fs_size, 1);

    /* lfs_fs_size() failure -> OS_ERROR */
    UT_SetDeferredRetcode(UT_KEY(lfs_fs_size), 1, LFS_ERR_IO);
    UtAssert_INT32_EQ(OS_FileSysStatVolume_Impl(&token, &result), OS_ERROR);
    UtAssert_STUB_COUNT(lfs_fs_size, 2);
}

/* ------------------- End of test cases --------------------------------------*/

void Osapi_Test_Setup(void)
{
    UT_ResetState(0);
    memset(&UT_Stub_StorageConfig, 0, sizeof(UT_Stub_StorageConfig));
    memset(&UT_Stub_StatInfo, 0, sizeof(UT_Stub_StatInfo));
    memset(&OS_filesys_table[0], 0, sizeof(OS_filesys_table[0]));
}

void Osapi_Test_Teardown(void)
{
}

#define ADD_TEST(test) UtTest_Add((Test_##test), Osapi_Test_Setup, Osapi_Test_Teardown, #test)

void UtTest_Setup(void)
{
    ADD_TEST(OS_FreeRTOS_FileSysInit);
    ADD_TEST(OS_FreeRTOS_LfsLock);
    ADD_TEST(OS_FileSysStartVolume_Impl);
    ADD_TEST(OS_FileSysStopVolume_Impl);
    ADD_TEST(OS_FileSysFormatVolume_Impl);
    ADD_TEST(OS_FileSysMountVolume_Impl);
    ADD_TEST(OS_FileSysUnmountVolume_Impl);
    ADD_TEST(OS_FileSysCheckVolume_Impl);
    ADD_TEST(OS_FileSysStatVolume_Impl);
}
