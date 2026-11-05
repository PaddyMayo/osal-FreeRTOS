/*
 * File: coveragetest-dir.c
 *
 * Purpose:
 *   White-box coverage test cases for freertos/src/os-impl-dir.c, run against
 *   the littlefs stand-ins in ../stubs/freertos-dir-stubs.c - no real library
 *   involved. (The black box tests for this port are OSAL's own /src/tests and
 *   /src/unit-tests, which link the real OSAL library.)
 *
 *   Each _Impl function is called directly, and effects are observed via
 *   return codes, the directory handle table, and the stubs' captured call
 *   arguments and counts. Every call must be made under the lock, so each
 *   case also checks the lock was taken and given the same number of times.
 */

#include <string.h>

#include "utassert.h"
#include "uttest.h"
#include "utstubs.h"

#include "osapi-file.h"

#include "os-shared-dir.h"
#include "os-shared-idmap.h"

#include "os-impl-dir.h"

/* Captured stub call arguments and scripted values, declared in freertos-dir-stubs.c */
extern uint32          UT_Stub_LockCount;
extern uint32          UT_Stub_UnlockCount;
extern lfs_t          *UT_Stub_LastLfs;
extern lfs_dir_t      *UT_Stub_LastDir;
extern const char     *UT_Stub_LastPath;
extern struct lfs_info UT_Stub_StatInfo;
extern struct lfs_info UT_Stub_ReadInfo;

#define UT_DIR_TOKEN(idx)                                                                              \
    (OS_object_token_t)                                                                                \
    {                                                                                                  \
        .obj_type = OS_OBJECT_TYPE_OS_DIR, .obj_id = (osal_id_t) { 0x70000 + (idx) }, .obj_idx = (idx) \
    }

#define UT_INDEX_2 OSAL_INDEX_C(2)

static void UT_AssertLocked(uint32 calls)
{
    UtAssert_UINT32_EQ(UT_Stub_LockCount, calls);
    UtAssert_UINT32_EQ(UT_Stub_UnlockCount, calls);
}

/*
**********************************************************************************
**          TEST CASES
**********************************************************************************
*/

void Test_OS_DirCreate_Impl(void)
{
    /* created -> success, and the access mode has nothing to apply to */
    UtAssert_INT32_EQ(OS_DirCreate_Impl("/vol/d", OS_READ_WRITE), OS_SUCCESS);
    UtAssert_STUB_COUNT(lfs_mkdir, 1);
    UtAssert_STUB_COUNT(lfs_stat, 0);
    UtAssert_True(UT_Stub_LastLfs == &OS_FreeRTOS_Lfs, "created on the volume");
    UtAssert_STRINGBUF_EQ(UT_Stub_LastPath, -1, "/vol/d", -1);
    UT_AssertLocked(1);

    /* already there as a directory -> success, as for POSIX */
    UT_SetDeferredRetcode(UT_KEY(lfs_mkdir), 1, LFS_ERR_EXIST);
    UT_Stub_StatInfo.type = LFS_TYPE_DIR;
    UtAssert_INT32_EQ(OS_DirCreate_Impl("/vol/d", OS_READ_WRITE), OS_SUCCESS);
    UtAssert_STUB_COUNT(lfs_stat, 1);
    UT_AssertLocked(2);

    /* already there as a file -> failure */
    UT_SetDeferredRetcode(UT_KEY(lfs_mkdir), 1, LFS_ERR_EXIST);
    UT_Stub_StatInfo.type = LFS_TYPE_REG;
    UtAssert_INT32_EQ(OS_DirCreate_Impl("/vol/f", OS_READ_WRITE), OS_ERROR);
    UT_AssertLocked(3);

    /* already there, but it cannot be told what it is -> failure */
    UT_SetDeferredRetcode(UT_KEY(lfs_mkdir), 1, LFS_ERR_EXIST);
    UT_SetDeferredRetcode(UT_KEY(lfs_stat), 1, LFS_ERR_IO);
    UtAssert_INT32_EQ(OS_DirCreate_Impl("/vol/d", OS_READ_WRITE), OS_ERROR);
    UT_AssertLocked(4);

    /* any other lfs_mkdir() failure -> failure, with no look at what is there */
    UT_SetDeferredRetcode(UT_KEY(lfs_mkdir), 1, LFS_ERR_NOENT);
    UtAssert_INT32_EQ(OS_DirCreate_Impl("/missing/d", OS_READ_WRITE), OS_ERROR);
    UtAssert_STUB_COUNT(lfs_stat, 3);
    UT_AssertLocked(5);
}

void Test_OS_DirOpen_Impl(void)
{
    OS_object_token_t              token = UT_DIR_TOKEN(2);
    OS_impl_dir_internal_record_t *impl  = &OS_impl_dir_table[UT_INDEX_2];

    /* opened into the entry's own lfs_dir_t */
    UtAssert_INT32_EQ(OS_DirOpen_Impl(&token, "/vol/d"), OS_SUCCESS);
    UtAssert_STUB_COUNT(lfs_dir_open, 1);
    UtAssert_True(UT_Stub_LastLfs == &OS_FreeRTOS_Lfs, "opened on the volume");
    UtAssert_True(UT_Stub_LastDir == &impl->dir, "opened into the entry's lfs_dir_t");
    UtAssert_STRINGBUF_EQ(UT_Stub_LastPath, -1, "/vol/d", -1);
    UT_AssertLocked(1);

    /* lfs_dir_open() failure (missing, or not a directory) -> OS_ERROR */
    UT_SetDeferredRetcode(UT_KEY(lfs_dir_open), 1, LFS_ERR_NOTDIR);
    UtAssert_INT32_EQ(OS_DirOpen_Impl(&token, "/vol/f"), OS_ERROR);
    UT_AssertLocked(2);
}

void Test_OS_DirClose_Impl(void)
{
    OS_object_token_t token = UT_DIR_TOKEN(2);

    UtAssert_INT32_EQ(OS_DirClose_Impl(&token), OS_SUCCESS);
    UtAssert_STUB_COUNT(lfs_dir_close, 1);
    UtAssert_True(UT_Stub_LastLfs == &OS_FreeRTOS_Lfs, "closed on the volume");
    UtAssert_True(UT_Stub_LastDir == &OS_impl_dir_table[UT_INDEX_2].dir, "the entry's lfs_dir_t closed");
    UT_AssertLocked(1);

    /* lfs_dir_close() failure -> OS_ERROR */
    UT_SetDeferredRetcode(UT_KEY(lfs_dir_close), 1, LFS_ERR_IO);
    UtAssert_INT32_EQ(OS_DirClose_Impl(&token), OS_ERROR);
    UT_AssertLocked(2);
}

void Test_OS_DirRead_Impl(void)
{
    OS_object_token_t token = UT_DIR_TOKEN(2);
    os_dirent_t       dirent;
    char              long_name[LFS_NAME_MAX + 1];

    /* an entry -> its name, read from the entry's own lfs_dir_t */
    UT_SetDefaultReturnValue(UT_KEY(lfs_dir_read), 1);
    strcpy(UT_Stub_ReadInfo.name, "file");
    memset(&dirent, 0xA5, sizeof(dirent));
    UtAssert_INT32_EQ(OS_DirRead_Impl(&token, &dirent), OS_SUCCESS);
    UtAssert_True(UT_Stub_LastDir == &OS_impl_dir_table[UT_INDEX_2].dir, "read from the entry's lfs_dir_t");
    UtAssert_STRINGBUF_EQ(dirent.FileName, sizeof(dirent.FileName), "file", -1);
    UT_AssertLocked(1);

    /* a name that just fits is kept whole */
    memset(UT_Stub_ReadInfo.name, 'n', sizeof(dirent.FileName) - 1);
    UT_Stub_ReadInfo.name[sizeof(dirent.FileName) - 1] = 0;
    UtAssert_INT32_EQ(OS_DirRead_Impl(&token, &dirent), OS_SUCCESS);
    UtAssert_UINT32_EQ(strlen(dirent.FileName), sizeof(dirent.FileName) - 1);
    UtAssert_True(memcmp(dirent.FileName, UT_Stub_ReadInfo.name, sizeof(dirent.FileName)) == 0, "name kept whole");

    /* a name that is too long is cut to the buffer, and still terminated */
    memset(long_name, 'n', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = 0;
    strcpy(UT_Stub_ReadInfo.name, long_name);
    memset(&dirent, 0xA5, sizeof(dirent));
    UtAssert_INT32_EQ(OS_DirRead_Impl(&token, &dirent), OS_SUCCESS);
    UtAssert_UINT32_EQ(strlen(dirent.FileName), sizeof(dirent.FileName) - 1);
    UtAssert_UINT32_EQ(dirent.FileName[sizeof(dirent.FileName) - 1], 0);
    UT_AssertLocked(3);

    /* the end of the directory -> OS_ERROR, and the caller's entry is left alone */
    UT_SetDefaultReturnValue(UT_KEY(lfs_dir_read), 0);
    memset(&dirent, 0xA5, sizeof(dirent));
    UtAssert_INT32_EQ(OS_DirRead_Impl(&token, &dirent), OS_ERROR);
    UtAssert_UINT32_EQ((uint8)dirent.FileName[0], 0xA5);

    /* lfs_dir_read() failure -> OS_ERROR */
    UT_SetDefaultReturnValue(UT_KEY(lfs_dir_read), LFS_ERR_IO);
    UtAssert_INT32_EQ(OS_DirRead_Impl(&token, &dirent), OS_ERROR);
    UtAssert_UINT32_EQ((uint8)dirent.FileName[0], 0xA5);
    UT_AssertLocked(5);
}

void Test_OS_DirRewind_Impl(void)
{
    OS_object_token_t token = UT_DIR_TOKEN(2);

    UtAssert_INT32_EQ(OS_DirRewind_Impl(&token), OS_SUCCESS);
    UtAssert_STUB_COUNT(lfs_dir_rewind, 1);
    UtAssert_True(UT_Stub_LastLfs == &OS_FreeRTOS_Lfs, "rewound on the volume");
    UtAssert_True(UT_Stub_LastDir == &OS_impl_dir_table[UT_INDEX_2].dir, "the entry's lfs_dir_t rewound");
    UT_AssertLocked(1);

    /* lfs_dir_rewind() failure -> OS_ERROR */
    UT_SetDeferredRetcode(UT_KEY(lfs_dir_rewind), 1, LFS_ERR_IO);
    UtAssert_INT32_EQ(OS_DirRewind_Impl(&token), OS_ERROR);
    UT_AssertLocked(2);
}

void Test_OS_DirRemove_Impl(void)
{
    /* an empty directory -> removed, after being checked to be one */
    UT_Stub_StatInfo.type = LFS_TYPE_DIR;
    UtAssert_INT32_EQ(OS_DirRemove_Impl("/vol/d"), OS_SUCCESS);
    UtAssert_STUB_COUNT(lfs_stat, 1);
    UtAssert_STUB_COUNT(lfs_remove, 1);
    UtAssert_True(UT_Stub_LastLfs == &OS_FreeRTOS_Lfs, "removed from the volume");
    UtAssert_STRINGBUF_EQ(UT_Stub_LastPath, -1, "/vol/d", -1);
    UT_AssertLocked(1);

    /* lfs_remove() failure (not empty) -> OS_ERROR */
    UT_SetDeferredRetcode(UT_KEY(lfs_remove), 1, LFS_ERR_NOTEMPTY);
    UtAssert_INT32_EQ(OS_DirRemove_Impl("/vol/d"), OS_ERROR);
    UtAssert_STUB_COUNT(lfs_remove, 2);
    UT_AssertLocked(2);

    /* a file is left alone, though lfs_remove() would take it */
    UT_Stub_StatInfo.type = LFS_TYPE_REG;
    UtAssert_INT32_EQ(OS_DirRemove_Impl("/vol/f"), OS_ERROR);
    UtAssert_STUB_COUNT(lfs_remove, 2);
    UT_AssertLocked(3);

    /* nothing there -> OS_ERROR, with no removal attempted */
    UT_SetDeferredRetcode(UT_KEY(lfs_stat), 1, LFS_ERR_NOENT);
    UtAssert_INT32_EQ(OS_DirRemove_Impl("/vol/missing"), OS_ERROR);
    UtAssert_STUB_COUNT(lfs_remove, 2);
    UT_AssertLocked(4);
}

/* ------------------- End of test cases --------------------------------------*/

void Osapi_Test_Setup(void)
{
    UT_ResetState(0);
    memset(OS_impl_dir_table, 0, sizeof(OS_impl_dir_table));
    memset(&UT_Stub_StatInfo, 0, sizeof(UT_Stub_StatInfo));
    memset(&UT_Stub_ReadInfo, 0, sizeof(UT_Stub_ReadInfo));
    UT_Stub_LockCount   = 0;
    UT_Stub_UnlockCount = 0;
}

void Osapi_Test_Teardown(void)
{
}

#define ADD_TEST(test) UtTest_Add((Test_##test), Osapi_Test_Setup, Osapi_Test_Teardown, #test)

void UtTest_Setup(void)
{
    ADD_TEST(OS_DirCreate_Impl);
    ADD_TEST(OS_DirOpen_Impl);
    ADD_TEST(OS_DirClose_Impl);
    ADD_TEST(OS_DirRead_Impl);
    ADD_TEST(OS_DirRewind_Impl);
    ADD_TEST(OS_DirRemove_Impl);
}
