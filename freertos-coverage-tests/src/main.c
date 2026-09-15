/*
 * File: main.c
 *
 * Purpose:
 *   Standalone entry point for this coverage-test executable - deliberately
 *   plain (no OSAL BSP, no FreeRTOS scheduler): just runs the registered
 *   UtTest_Setup()/UtTest_Run() test sequence and exits.
 */

#include "uttest.h"
#include "utassert.h"

int main(int argc, char *argv[])
{
    const UtAssert_TestCounter_t *counters;

    (void)argc;
    (void)argv;

    /* Normally called by OS_Application_Startup() (ut_assert's utbsp.c) -
     * initializes ut_assert's internal test database before anything can
     * register a test case. */
    UtTest_EarlyInit();

    UtTest_Setup();
    UtTest_Run();

    counters = UtAssert_GetCounters();

    if (counters->CaseCount[UTASSERT_CASETYPE_FAILURE] > 0 || counters->CaseCount[UTASSERT_CASETYPE_TSF] > 0
        || counters->CaseCount[UTASSERT_CASETYPE_TTF] > 0)
    {
        return 1;
    }

    return 0;
}
