/*
 * File: coveragetest-bsp-console.c
 *
 * Purpose:
 *   White-box coverage test cases for generic-freertos/src/bsp_console.c, run
 *   against the stand-in for the vendor's OS_BSP_Uart_Configure and UART in
 *   ../stubs/freertos-bsp-console-stubs.c.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "utassert.h"
#include "uttest.h"

#include "osapi-common.h"
#include "bsp-impl.h"

#include "generic_freertos_bsp_internal.h"
#include "bsp_io.h"

/* The stand-in vendor and UART, declared in freertos-bsp-console-stubs.c */
extern struct OS_BSP_UartConfig UT_Stub_UartConfig;
extern uint32_t                 UT_Stub_ConfigureCount;
extern uint8_t                  UT_Stub_TxBytes[];
extern uint32_t                 UT_Stub_TxCount;
extern uint32_t                 UT_Stub_TxReadyCalls;
extern uint32_t                 UT_Stub_AcceptBytes;
extern uint32_t                 UT_Stub_NotReadyPolls;
extern uint32_t                 UT_Stub_PollsThisByte;
bool                            UT_Stub_TxReady(void);
void                            UT_Stub_TxByte(uint8_t Byte);

/* What the UART has been given so far must read exactly Expected */
static void AssertSent(const char *Expected)
{
    UtAssert_STRINGBUF_EQ((const char *)UT_Stub_TxBytes, UT_Stub_TxCount, Expected, strlen(Expected));
}

/* Rates for which the wait limit is 20 polls, so a UART that is never ready is given up on quickly */
static void UseShortWait(void)
{
    UT_Stub_UartConfig.BaudRate = 1000;
    UT_Stub_UartConfig.ClockHz  = 1000;
}

void Test_DeviceInit(void)
{
    UtAssert_INT32_EQ(OS_BSP_Console_DeviceInit(), 0);
    UtAssert_UINT32_EQ(UT_Stub_ConfigureCount, 1);

    /* The accessors it was given are the ones the console uses */
    OS_BSP_ConsoleOutput_Impl("ok", 2);
    AssertSent("ok");
}

void Test_DeviceInitRejects(void)
{
    /* An accessor missing leaves nothing to send through: output is dropped */
    UT_Stub_UartConfig.TxReady = NULL;
    UtAssert_INT32_EQ(OS_BSP_Console_DeviceInit(), OS_BSP_CONSOLE_ERR_ACCESSOR);
    OS_BSP_ConsoleOutput_Impl("dropped", 7);
    UtAssert_UINT32_EQ(UT_Stub_TxCount, 0);

    UT_Stub_UartConfig.TxReady = UT_Stub_TxReady;
    UT_Stub_UartConfig.TxByte  = NULL;
    UtAssert_INT32_EQ(OS_BSP_Console_DeviceInit(), OS_BSP_CONSOLE_ERR_ACCESSOR);
    OS_BSP_ConsoleOutput_Impl("dropped", 7);
    UtAssert_UINT32_EQ(UT_Stub_TxCount, 0);
    UT_Stub_UartConfig.TxByte = UT_Stub_TxByte;

    /* A bad rate is reported through a console that still works, so main() can say why it stops */
    UT_Stub_UartConfig.BaudRate = 0;
    UtAssert_INT32_EQ(OS_BSP_Console_DeviceInit(), OS_BSP_CONSOLE_ERR_RATE);
    OS_BSP_ConsoleOutput_Impl("1", 1);
    AssertSent("1");

    UT_Stub_UartConfig.BaudRate = 115200;
    UT_Stub_UartConfig.ClockHz  = 0;
    UtAssert_INT32_EQ(OS_BSP_Console_DeviceInit(), OS_BSP_CONSOLE_ERR_RATE);

    UT_Stub_UartConfig.ClockHz = 9600;
    UtAssert_INT32_EQ(OS_BSP_Console_DeviceInit(), OS_BSP_CONSOLE_ERR_RANGE);
    OS_BSP_ConsoleOutput_Impl("2", 1);
    AssertSent("12");
}

void Test_DeviceInitClampsTheWait(void)
{
    /* 20 polls per cycle of a 1 GHz clock at 1 baud does not fit in 32 bits */
    UT_Stub_UartConfig.BaudRate = 1;
    UT_Stub_UartConfig.ClockHz  = 1000000000;
    UtAssert_INT32_EQ(OS_BSP_Console_DeviceInit(), 0);

    /* A UART that takes a while to get ready is still waited for */
    UT_Stub_NotReadyPolls = 5;
    OS_BSP_ConsoleOutput_Impl("x", 1);
    AssertSent("x");
    UtAssert_UINT32_EQ(UT_Stub_TxReadyCalls, 6);
}

void Test_ConsoleOutput(void)
{
    UtAssert_INT32_EQ(OS_BSP_Console_DeviceInit(), 0);

    OS_BSP_ConsoleOutput_Impl("hello", 5);
    AssertSent("hello");

    /* Lines end in a plain '\n', the UART gets "\r\n" */
    OS_BSP_ConsoleOutput_Impl("a\nb", 3);
    AssertSent("helloa\r\nb");

    /* Only Len bytes are looked at, and none is fine */
    OS_BSP_ConsoleOutput_Impl("xyz", 1);
    OS_BSP_ConsoleOutput_Impl("zzz", 0);
    AssertSent("helloa\r\nbx");
}

void Test_ConsoleOutputGivesUp(void)
{
    UseShortWait();
    UtAssert_INT32_EQ(OS_BSP_Console_DeviceInit(), 0);

    /* The UART takes 1 byte and then never gets ready: the second is waited for, then dropped */
    UT_Stub_AcceptBytes = 1;
    OS_BSP_ConsoleOutput_Impl("abc", 3);
    AssertSent("a");
    UtAssert_UINT32_EQ(UT_Stub_TxReadyCalls, 1 + 21);

    /* Giving up is for good, and costs no more waiting */
    UT_Stub_AcceptBytes = UINT32_MAX;
    OS_BSP_ConsoleOutput_Impl("def", 3);
    AssertSent("a");
    UtAssert_UINT32_EQ(UT_Stub_TxReadyCalls, 1 + 21);
}

void Test_ConsoleOutputGivesUpInALineEnd(void)
{
    UseShortWait();
    UtAssert_INT32_EQ(OS_BSP_Console_DeviceInit(), 0);

    /* The '\r' goes out, the '\n' behind it does not */
    UT_Stub_AcceptBytes = 1;
    OS_BSP_ConsoleOutput_Impl("\n", 1);
    AssertSent("\r");

    /* The '\r' itself does not */
    UtAssert_INT32_EQ(OS_BSP_Console_DeviceInit(), 0);
    UT_Stub_TxCount     = 0;
    UT_Stub_AcceptBytes = 0;
    OS_BSP_ConsoleOutput_Impl("\n", 1);
    AssertSent("");
}

void Test_ConsoleSetMode(void)
{
    OS_BSP_ConsoleSetMode_Impl(0);

    UtAssert_NA("the console has no modes yet");
}

/*
 * Setup function prior to every test: a good vendor and a UART that takes everything at once
 */
void Osapi_Test_Setup(void)
{
    memset(&UT_Stub_UartConfig, 0, sizeof(UT_Stub_UartConfig));
    UT_Stub_UartConfig.BaudRate = 115200;
    UT_Stub_UartConfig.ClockHz  = 16000000;
    UT_Stub_UartConfig.TxReady  = UT_Stub_TxReady;
    UT_Stub_UartConfig.TxByte   = UT_Stub_TxByte;
    UT_Stub_ConfigureCount      = 0;

    UT_Stub_TxCount       = 0;
    UT_Stub_TxReadyCalls  = 0;
    UT_Stub_AcceptBytes   = UINT32_MAX;
    UT_Stub_NotReadyPolls = 0;
    UT_Stub_PollsThisByte = 0;
}

void Osapi_Test_Teardown(void)
{
}

#define ADD_TEST(test) UtTest_Add((Test_##test), Osapi_Test_Setup, Osapi_Test_Teardown, #test)

void UtTest_Setup(void)
{
    ADD_TEST(DeviceInit);
    ADD_TEST(DeviceInitRejects);
    ADD_TEST(DeviceInitClampsTheWait);
    ADD_TEST(ConsoleOutput);
    ADD_TEST(ConsoleOutputGivesUp);
    ADD_TEST(ConsoleOutputGivesUpInALineEnd);
    ADD_TEST(ConsoleSetMode);
}
