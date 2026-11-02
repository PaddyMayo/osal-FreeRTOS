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

/* A bare-metal board has no files: its output channel is not wired up yet, so the counters are not written */
static void CoverageFilename(const char *Name, void *Arg)
{
    (void)Name;
    (void)Arg;
}

static void CoverageWrite(const void *Buffer, unsigned Size, void *Arg)
{
    (void)Buffer;
    (void)Size;
    (void)Arg;
}

static void CoverageClose(void)
{
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
