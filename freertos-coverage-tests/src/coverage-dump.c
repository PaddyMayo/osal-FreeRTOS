/*
 * File: coverage-dump.c
 *
 * Purpose:
 *   Writes out the gcov counters of an instrumented test executable.
 *
 *   Every platform builds the tests with the freestanding coverage flags
 *   (-fprofile-arcs -ftest-coverage -fprofile-info-section=gcov_info) instead of
 *   --coverage. The compiler then emits no start-up and exit hooks: the bare-metal
 *   libgcov has no __gcov_exit, so --coverage cannot link for Arm. Instead it
 *   leaves a pointer to each instrumented file's gcov_info in the section
 *   "gcov_info", and the test calls CoverageDump() to walk them.
 *
 *   GNU ld brackets a section whose name is a C identifier with
 *   __start_<name> and __stop_<name>. A linker script that has its own layout
 *   must define the same two symbols around the section.
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gcov.h>

#include "coverage-dump.h"

extern const struct gcov_info *const __start_gcov_info[] __attribute__((weak));
extern const struct gcov_info *const __stop_gcov_info[] __attribute__((weak));

#if defined(__unix__)

/* The data of one gcov_info is one gcda file, named by the filename callback */
static FILE *CoverageFile;

static void CoverageFilename(const char *Name, void *Arg)
{
    (void)Arg;

    if (CoverageFile != NULL)
    {
        (void)fclose(CoverageFile);
        CoverageFile = NULL;
    }

    if (Name != NULL)
    {
        CoverageFile = fopen(Name, "wb");
    }
}

static void CoverageWrite(const void *Buffer, unsigned Size, void *Arg)
{
    (void)Arg;

    if (CoverageFile != NULL)
    {
        (void)fwrite(Buffer, 1U, Size, CoverageFile);
    }
}

static void CoverageClose(void)
{
    if (CoverageFile != NULL)
    {
        (void)fclose(CoverageFile);
        CoverageFile = NULL;
    }
}

#else

/*
 * A bare-metal board has no files. The gcda streams go into this buffer instead, and
 * run_board_test.py reads it through the debugger when the run ends and writes the
 * files on the host. A record is the file name and a 0 byte, the length of the data
 * as four bytes (least significant first), then the data. The three variables are
 * global, not static, so that the debugger can find them by name.
 */
#define COVERAGE_BUFFER_SIZE 65536U

__attribute__((used)) unsigned char CoverageBuffer[COVERAGE_BUFFER_SIZE];
__attribute__((used)) unsigned      CoverageBufferUsed;
/* Bytes that did not fit: the debugger fails the run when this is not 0 */
__attribute__((used)) unsigned      CoverageBufferLost;

/* Where the length of the record being written sits, or 0 when there is none */
static unsigned CoverageLengthAt;

static void CoveragePut(const void *Data, unsigned Size)
{
    if (Size > COVERAGE_BUFFER_SIZE - CoverageBufferUsed)
    {
        CoverageBufferLost += Size;
        return;
    }

    (void)memcpy(&CoverageBuffer[CoverageBufferUsed], Data, Size);
    CoverageBufferUsed += Size;
}

/* The length of a record is only known once all of its data is in */
static void CoverageEndRecord(void)
{
    if (CoverageLengthAt != 0U)
    {
        unsigned Length = CoverageBufferUsed - (CoverageLengthAt + 4U);
        unsigned Index;

        for (Index = 0U; Index < 4U; Index++)
        {
            CoverageBuffer[CoverageLengthAt + Index] = (unsigned char)(Length >> (8U * Index));
        }

        CoverageLengthAt = 0U;
    }
}

static void CoverageFilename(const char *Name, void *Arg)
{
    static const unsigned char NoLength[4] = { 0U, 0U, 0U, 0U };

    (void)Arg;

    CoverageEndRecord();

    if (Name != NULL)
    {
        CoveragePut(Name, (unsigned)strlen(Name));
    }
    CoveragePut(NoLength, 1U);

    if (CoverageBufferLost == 0U && CoverageBufferUsed + 4U <= COVERAGE_BUFFER_SIZE)
    {
        CoverageLengthAt = CoverageBufferUsed;
    }
    CoveragePut(NoLength, 4U);
}

static void CoverageWrite(const void *Buffer, unsigned Size, void *Arg)
{
    (void)Arg;

    CoveragePut(Buffer, Size);
}

static void CoverageClose(void)
{
    CoverageEndRecord();
}

#endif

static void *CoverageAllocate(unsigned Size, void *Arg)
{
    (void)Arg;

    return malloc(Size);
}

void CoverageDump(void)
{
    const struct gcov_info *const *Info;

    /* Weak: null when the linker script does not define the section's bounds */
    // cppcheck-suppress knownConditionTrueFalse
    if (__start_gcov_info == NULL || __stop_gcov_info == NULL)
    {
        return;
    }

    for (Info = __start_gcov_info; Info != __stop_gcov_info; Info++)
    {
        __gcov_info_to_gcda(*Info, CoverageFilename, CoverageWrite, CoverageAllocate, NULL);
    }

    CoverageClose();
}
