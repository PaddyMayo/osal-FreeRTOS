/*
 * File: coveragetest-files.c
 *
 * Purpose:
 *   White-box coverage test cases for freertos/src/os-impl-files.c, run
 *   against the littlefs stand-ins in ../stubs/freertos-files-stubs.c - no
 *   real library involved. (The black box tests for this port are OSAL's own
 *   /src/tests and /src/unit-tests, which link the real OSAL library.)
 *
 *   Each _Impl function is called directly, and effects are observed via
 *   return codes, the file handle table, and the stubs' captured call
 *   arguments and counts. Every call must be made under the lock, so each
 *   case also checks the lock was taken and given the same number of times.
 */

#include <string.h>

#include "utassert.h"
#include "uttest.h"
#include "utstubs.h"

#include "os-shared-file.h"
#include "os-shared-idmap.h"

#include "os-impl-files.h"

/* Captured stub call arguments and scripted values, declared in freertos-files-stubs.c */
extern uint32                        UT_Stub_LockCount;
extern uint32                        UT_Stub_UnlockCount;
extern lfs_t                        *UT_Stub_LastLfs;
extern lfs_file_t                   *UT_Stub_LastFile;
extern const char                   *UT_Stub_LastPath;
extern const char                   *UT_Stub_LastNewPath;
extern int                           UT_Stub_LastOpenFlags;
extern const struct lfs_file_config *UT_Stub_LastFileConfig;
extern const void                   *UT_Stub_LastBuffer;
extern lfs_size_t                    UT_Stub_LastSize;
extern lfs_soff_t                    UT_Stub_LastSeekOffset;
extern int                           UT_Stub_LastSeekWhence;
extern struct lfs_info               UT_Stub_StatInfo;

#define UT_FILE_TOKEN(idx)                                                                                \
    (OS_object_token_t)                                                                                   \
    {                                                                                                     \
        .obj_type = OS_OBJECT_TYPE_OS_STREAM, .obj_id = (osal_id_t) { 0x60000 + (idx) }, .obj_idx = (idx) \
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

void Test_OS_FreeRTOS_FileInit(void)
{
    memset(&OS_impl_filehandle_table[1], 0xA5, sizeof(OS_impl_filehandle_table[1]));

    UtAssert_INT32_EQ(OS_FreeRTOS_FileInit(), OS_SUCCESS);
    UtAssert_MemCmpValue(OS_impl_filehandle_table, 0, sizeof(OS_impl_filehandle_table), "handle table cleared");
}

void Test_OS_FileOpen_Impl(void)
{
    OS_object_token_t               token = UT_FILE_TOKEN(2);
    OS_impl_file_internal_record_t *impl  = &OS_impl_filehandle_table[UT_INDEX_2];

    /* read only, no flags -> opened read only, with the entry's own file, cache and config */
    UtAssert_INT32_EQ(OS_FileOpen_Impl(&token, "/a", OS_FILE_FLAG_NONE, OS_READ_ONLY), OS_SUCCESS);
    UtAssert_STUB_COUNT(lfs_file_opencfg, 1);
    UtAssert_True(UT_Stub_LastLfs == &OS_FreeRTOS_Lfs, "opened on the volume");
    UtAssert_True(UT_Stub_LastFile == &impl->file, "opened into the entry's lfs_file_t");
    UtAssert_True(UT_Stub_LastFileConfig == &impl->config, "opened with the entry's config");
    UtAssert_True(impl->config.buffer == impl->cache, "config points at the entry's cache");
    UtAssert_STRINGBUF_EQ(UT_Stub_LastPath, -1, "/a", -1);
    UtAssert_UINT32_EQ(UT_Stub_LastOpenFlags, LFS_O_RDONLY);
    UT_AssertLocked(1);

    /* write only, create -> created if missing */
    UtAssert_INT32_EQ(OS_FileOpen_Impl(&token, "/a", OS_FILE_FLAG_CREATE, OS_WRITE_ONLY), OS_SUCCESS);
    UtAssert_UINT32_EQ(UT_Stub_LastOpenFlags, LFS_O_WRONLY | LFS_O_CREAT);

    /* read write, truncate -> emptied */
    UtAssert_INT32_EQ(OS_FileOpen_Impl(&token, "/a", OS_FILE_FLAG_TRUNCATE, OS_READ_WRITE), OS_SUCCESS);
    UtAssert_UINT32_EQ(UT_Stub_LastOpenFlags, LFS_O_RDWR | LFS_O_TRUNC);

    /* both flags */
    UtAssert_INT32_EQ(OS_FileOpen_Impl(&token, "/a", OS_FILE_FLAG_CREATE | OS_FILE_FLAG_TRUNCATE, OS_READ_WRITE),
                      OS_SUCCESS);
    UtAssert_UINT32_EQ(UT_Stub_LastOpenFlags, LFS_O_RDWR | LFS_O_CREAT | LFS_O_TRUNC);
    UT_AssertLocked(4);

    /* a config left over from an earlier open is cleared before the next one */
    impl->config.attr_count = 7;
    UtAssert_INT32_EQ(OS_FileOpen_Impl(&token, "/a", OS_FILE_FLAG_NONE, OS_READ_ONLY), OS_SUCCESS);
    UtAssert_UINT32_EQ(impl->config.attr_count, 0);

    /* lfs_file_opencfg() failure -> OS_ERROR */
    UT_SetDeferredRetcode(UT_KEY(lfs_file_opencfg), 1, LFS_ERR_NOENT);
    UtAssert_INT32_EQ(OS_FileOpen_Impl(&token, "/a", OS_FILE_FLAG_NONE, OS_READ_ONLY), OS_ERROR);
    UT_AssertLocked(6);
}

void Test_OS_FileNotImplemented_Impl(void)
{
    OS_object_token_t token = UT_FILE_TOKEN(2);

    UtAssert_INT32_EQ(OS_FileAllocate_Impl(&token, 0, 10), OS_ERR_NOT_IMPLEMENTED);
    UtAssert_INT32_EQ(OS_ShellOutputToFile_Impl(&token, "ls"), OS_ERR_NOT_IMPLEMENTED);
    UtAssert_INT32_EQ(OS_FileChmod_Impl("/a", OS_READ_ONLY), OS_ERR_NOT_IMPLEMENTED);
    UT_AssertLocked(0);
}

void Test_OS_FileTruncate_Impl(void)
{
    OS_object_token_t token = UT_FILE_TOKEN(2);

    UtAssert_INT32_EQ(OS_FileTruncate_Impl(&token, 100), OS_SUCCESS);
    UtAssert_STUB_COUNT(lfs_file_truncate, 1);
    UtAssert_True(UT_Stub_LastFile == &OS_impl_filehandle_table[UT_INDEX_2].file, "the entry's file truncated");
    UtAssert_UINT32_EQ(UT_Stub_LastSize, 100);
    UT_AssertLocked(1);

    UT_SetDeferredRetcode(UT_KEY(lfs_file_truncate), 1, LFS_ERR_BADF);
    UtAssert_INT32_EQ(OS_FileTruncate_Impl(&token, 100), OS_ERROR);
    UT_AssertLocked(2);
}

void Test_OS_GenericSeek_Impl(void)
{
    OS_object_token_t token = UT_FILE_TOKEN(2);

    /* each whence is passed through, and the new position returned */
    UT_SetDefaultReturnValue(UT_KEY(lfs_file_seek), 40);
    UtAssert_INT32_EQ(OS_GenericSeek_Impl(&token, 10, OS_SEEK_SET), 40);
    UtAssert_INT32_EQ(UT_Stub_LastSeekWhence, LFS_SEEK_SET);
    UtAssert_INT32_EQ(UT_Stub_LastSeekOffset, 10);
    UtAssert_True(UT_Stub_LastFile == &OS_impl_filehandle_table[UT_INDEX_2].file, "the entry's file sought");

    UtAssert_INT32_EQ(OS_GenericSeek_Impl(&token, -5, OS_SEEK_CUR), 40);
    UtAssert_INT32_EQ(UT_Stub_LastSeekWhence, LFS_SEEK_CUR);
    UtAssert_INT32_EQ(UT_Stub_LastSeekOffset, -5);

    UtAssert_INT32_EQ(OS_GenericSeek_Impl(&token, 0, OS_SEEK_END), 40);
    UtAssert_INT32_EQ(UT_Stub_LastSeekWhence, LFS_SEEK_END);
    UT_AssertLocked(3);

    /* anything past OS_SEEK_END is not a whence -> rejected before touching littlefs */
    UtAssert_INT32_EQ(OS_GenericSeek_Impl(&token, 0, OS_SEEK_END + 1), OS_ERROR);
    UtAssert_STUB_COUNT(lfs_file_seek, 3);
    UT_AssertLocked(3);

    /* lfs_file_seek() failure -> OS_ERROR */
    UT_SetDefaultReturnValue(UT_KEY(lfs_file_seek), LFS_ERR_INVAL);
    UtAssert_INT32_EQ(OS_GenericSeek_Impl(&token, 0, OS_SEEK_SET), OS_ERROR);
    UT_AssertLocked(4);
}

void Test_OS_GenericRead_Impl(void)
{
    OS_object_token_t token = UT_FILE_TOKEN(2);
    char              buffer[8];
    OS_time_t         timeout = { 0 };

    /* the count read is returned, and the timeout is of no use to a file */
    UT_SetDefaultReturnValue(UT_KEY(lfs_file_read), 5);
    UtAssert_INT32_EQ(OS_GenericRead_Impl(&token, buffer, sizeof(buffer), timeout), 5);
    UtAssert_True(UT_Stub_LastFile == &OS_impl_filehandle_table[UT_INDEX_2].file, "the entry's file read");
    UtAssert_True(UT_Stub_LastBuffer == buffer, "read into the caller's buffer");
    UtAssert_UINT32_EQ(UT_Stub_LastSize, sizeof(buffer));
    UT_AssertLocked(1);

    /* end of file -> zero bytes, not an error */
    UT_SetDefaultReturnValue(UT_KEY(lfs_file_read), 0);
    UtAssert_INT32_EQ(OS_GenericRead_Impl(&token, buffer, sizeof(buffer), timeout), 0);

    /* lfs_file_read() failure -> OS_ERROR */
    UT_SetDefaultReturnValue(UT_KEY(lfs_file_read), LFS_ERR_BADF);
    UtAssert_INT32_EQ(OS_GenericRead_Impl(&token, buffer, sizeof(buffer), timeout), OS_ERROR);
    UT_AssertLocked(3);
}

void Test_OS_GenericWrite_Impl(void)
{
    OS_object_token_t token    = UT_FILE_TOKEN(2);
    const char        buffer[] = "data";
    OS_time_t         timeout  = { 0 };

    /* the count written is returned, and the timeout is of no use to a file */
    UT_SetDefaultReturnValue(UT_KEY(lfs_file_write), 4);
    UtAssert_INT32_EQ(OS_GenericWrite_Impl(&token, buffer, 4, timeout), 4);
    UtAssert_True(UT_Stub_LastFile == &OS_impl_filehandle_table[UT_INDEX_2].file, "the entry's file written");
    UtAssert_True(UT_Stub_LastBuffer == buffer, "written from the caller's buffer");
    UtAssert_UINT32_EQ(UT_Stub_LastSize, 4);
    UT_AssertLocked(1);

    /* lfs_file_write() failure (a full volume, say) -> OS_ERROR */
    UT_SetDefaultReturnValue(UT_KEY(lfs_file_write), LFS_ERR_NOSPC);
    UtAssert_INT32_EQ(OS_GenericWrite_Impl(&token, buffer, 4, timeout), OS_ERROR);
    UT_AssertLocked(2);
}

void Test_OS_GenericClose_Impl(void)
{
    OS_object_token_t token = UT_FILE_TOKEN(2);

    UtAssert_INT32_EQ(OS_GenericClose_Impl(&token), OS_SUCCESS);
    UtAssert_STUB_COUNT(lfs_file_close, 1);
    UtAssert_True(UT_Stub_LastFile == &OS_impl_filehandle_table[UT_INDEX_2].file, "the entry's file closed");
    UT_AssertLocked(1);

    UT_SetDeferredRetcode(UT_KEY(lfs_file_close), 1, LFS_ERR_IO);
    UtAssert_INT32_EQ(OS_GenericClose_Impl(&token), OS_ERROR);
    UT_AssertLocked(2);
}

void Test_OS_FileStat_Impl(void)
{
    os_fstat_t filestat;

    memset(&filestat, 0xA5, sizeof(filestat));

    /* a regular file -> its size, readable and writable */
    UT_Stub_StatInfo.type = LFS_TYPE_REG;
    UT_Stub_StatInfo.size = 1234;
    UtAssert_INT32_EQ(OS_FileStat_Impl("/a", &filestat), OS_SUCCESS);
    UtAssert_STRINGBUF_EQ(UT_Stub_LastPath, -1, "/a", -1);
    UtAssert_UINT32_EQ(filestat.FileSize, 1234);
    UtAssert_UINT32_EQ(filestat.FileModeBits, OS_FILESTAT_MODE_READ | OS_FILESTAT_MODE_WRITE);
    UtAssert_UINT32_EQ(OS_TimeGetTotalSeconds(filestat.FileTime), 0);
    UT_AssertLocked(1);

    /* a directory -> flagged as one */
    UT_Stub_StatInfo.type = LFS_TYPE_DIR;
    UtAssert_INT32_EQ(OS_FileStat_Impl("/d", &filestat), OS_SUCCESS);
    UtAssert_UINT32_EQ(filestat.FileModeBits, OS_FILESTAT_MODE_READ | OS_FILESTAT_MODE_WRITE | OS_FILESTAT_MODE_DIR);

    /* lfs_stat() failure -> OS_ERROR, and the caller's structure is left alone */
    memset(&filestat, 0xA5, sizeof(filestat));
    UT_SetDeferredRetcode(UT_KEY(lfs_stat), 1, LFS_ERR_NOENT);
    UtAssert_INT32_EQ(OS_FileStat_Impl("/missing", &filestat), OS_ERROR);
    UtAssert_UINT32_EQ(filestat.FileSize, 0xA5A5A5A5);
    UT_AssertLocked(3);
}

void Test_OS_FileRemove_Impl(void)
{
    UtAssert_INT32_EQ(OS_FileRemove_Impl("/a"), OS_SUCCESS);
    UtAssert_STUB_COUNT(lfs_remove, 1);
    UtAssert_STRINGBUF_EQ(UT_Stub_LastPath, -1, "/a", -1);
    UT_AssertLocked(1);

    UT_SetDeferredRetcode(UT_KEY(lfs_remove), 1, LFS_ERR_NOENT);
    UtAssert_INT32_EQ(OS_FileRemove_Impl("/a"), OS_ERROR);
    UT_AssertLocked(2);
}

void Test_OS_FileRename_Impl(void)
{
    UtAssert_INT32_EQ(OS_FileRename_Impl("/a", "/b"), OS_SUCCESS);
    UtAssert_STUB_COUNT(lfs_rename, 1);
    UtAssert_STRINGBUF_EQ(UT_Stub_LastPath, -1, "/a", -1);
    UtAssert_STRINGBUF_EQ(UT_Stub_LastNewPath, -1, "/b", -1);
    UT_AssertLocked(1);

    UT_SetDeferredRetcode(UT_KEY(lfs_rename), 1, LFS_ERR_EXIST);
    UtAssert_INT32_EQ(OS_FileRename_Impl("/a", "/b"), OS_ERROR);
    UT_AssertLocked(2);
}

/* ------------------- End of test cases --------------------------------------*/

void Osapi_Test_Setup(void)
{
    UT_ResetState(0);
    memset(OS_impl_filehandle_table, 0, sizeof(OS_impl_filehandle_table));
    memset(&UT_Stub_StatInfo, 0, sizeof(UT_Stub_StatInfo));
    UT_Stub_LockCount   = 0;
    UT_Stub_UnlockCount = 0;
}

void Osapi_Test_Teardown(void)
{
}

#define ADD_TEST(test) UtTest_Add((Test_##test), Osapi_Test_Setup, Osapi_Test_Teardown, #test)

void UtTest_Setup(void)
{
    ADD_TEST(OS_FreeRTOS_FileInit);
    ADD_TEST(OS_FileOpen_Impl);
    ADD_TEST(OS_FileNotImplemented_Impl);
    ADD_TEST(OS_FileTruncate_Impl);
    ADD_TEST(OS_GenericSeek_Impl);
    ADD_TEST(OS_GenericRead_Impl);
    ADD_TEST(OS_GenericWrite_Impl);
    ADD_TEST(OS_GenericClose_Impl);
    ADD_TEST(OS_FileStat_Impl);
    ADD_TEST(OS_FileRemove_Impl);
    ADD_TEST(OS_FileRename_Impl);
}
