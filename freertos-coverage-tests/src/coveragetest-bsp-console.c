/*
 * File: coveragetest-bsp-console.c
 *
 * Purpose:
 *   White-box coverage test cases for generic-freertos/src/bsp_console.c, run
 *   against the stand-in for the vendor's console device in
 *   ../stubs/freertos-bsp-console-stubs.c.
 */

#include <stddef.h>
#include <stdint.h>

#include "utassert.h"
#include "uttest.h"

#include "osapi-common.h"
#include "bsp-impl.h"

/* What the stand-in console device was given, declared in freertos-bsp-console-stubs.c */
extern const char *UT_Stub_LastWriteBuf;
extern size_t      UT_Stub_LastWriteLen;
extern uint32_t    UT_Stub_WriteCount;

void Test_ConsoleOutput(void)
{
    static const char Text[] = "hello\n";

    OS_BSP_ConsoleOutput_Impl(Text, sizeof(Text) - 1);

    UtAssert_UINT32_EQ(UT_Stub_WriteCount, 1);
    UtAssert_True(UT_Stub_LastWriteBuf == Text, "the console device gets the caller's buffer");
    UtAssert_True(UT_Stub_LastWriteLen == sizeof(Text) - 1, "the console device gets the caller's length");

    /* The BSP does not look at the data: an empty slice goes through too */
    OS_BSP_ConsoleOutput_Impl(Text, 0);

    UtAssert_UINT32_EQ(UT_Stub_WriteCount, 2);
    UtAssert_True(UT_Stub_LastWriteLen == 0, "an empty write reaches the console device");
}

void Test_ConsoleSetMode(void)
{
    OS_BSP_ConsoleSetMode_Impl(0);

    UtAssert_NA("the console has no modes yet");
}

/*
 * Setup function prior to every test
 */
void Osapi_Test_Setup(void)
{
    UT_Stub_LastWriteBuf = NULL;
    UT_Stub_LastWriteLen = 0;
    UT_Stub_WriteCount   = 0;
}

void Osapi_Test_Teardown(void)
{
}

#define ADD_TEST(test) UtTest_Add((Test_##test), Osapi_Test_Setup, Osapi_Test_Teardown, #test)

void UtTest_Setup(void)
{
    ADD_TEST(ConsoleOutput);
    ADD_TEST(ConsoleSetMode);
}
