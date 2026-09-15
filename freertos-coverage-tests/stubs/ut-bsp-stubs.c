/*
 * File: ut-bsp-stubs.c
 *
 * Purpose:
 *   Minimal UT_BSP_* implementation (see ut_assert/inc/utbsp.h) for this
 *   standalone coverage-test executable. The real utbsp.c (part of the
 *   ut_assert library) implements these via OS_BSP_*_Impl, which pulls in
 *   the configured OSAL BSP - and in this project, that BSP links the real
 *   FreeRTOS kernel, whose main() runs an actual scheduler rather than
 *   returning. This test wants none of that: it links ut_assert's other
 *   sources directly (see ../CMakeLists.txt) and provides its own plain,
 *   stdio-based console output and its own main() (../src/main.c) instead.
 */

#include <stdio.h>

#include "utbsp.h"
#include "uttest.h"

void UT_BSP_Setup(void)
{
    UT_BSP_DoText(UTASSERT_CASETYPE_BEGIN, "UNIT TEST");
}

void UT_BSP_StartTestSegment(uint32 SegmentNumber, const char *SegmentName)
{
    printf("%02u %s\n", (unsigned int)SegmentNumber, SegmentName);
}

void UT_BSP_DoText(uint8 MessageType, const char *OutputMessage)
{
    printf("[%s] %s\n", UtAssert_GetCaseTypeAbbrev(MessageType), OutputMessage);
}

void UT_BSP_EndTest(const UtAssert_TestCounter_t *TestCounters)
{
    printf("COMPLETE: %u test segment(s) executed\n", (unsigned int)TestCounters->TestSegmentCount);
}

void UT_BSP_Lock(void)
{
}

void UT_BSP_Unlock(void)
{
}
