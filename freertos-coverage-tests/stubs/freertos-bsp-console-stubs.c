/*
 * File: freertos-bsp-console-stubs.c
 *
 * Purpose:
 *   Stand-in for the vendor's OS_BSP_Uart_Configure that
 *   generic-freertos/src/bsp_console.c calls, so the test chooses what the
 *   vendor "returns", and a UART whose readiness and received bytes it can
 *   watch and steer.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bsp_io.h"

/* What the stubbed OS_BSP_Uart_Configure returns, and how often it was called */
struct OS_BSP_UartConfig UT_Stub_UartConfig;
uint32_t                 UT_Stub_ConfigureCount;

struct OS_BSP_UartConfig OS_BSP_Uart_Configure(void)
{
    UT_Stub_ConfigureCount++;

    return UT_Stub_UartConfig;
}

/*
 * The stand-in UART behind the accessors. It takes UT_Stub_AcceptBytes bytes in all and then never
 * gets ready again, and is not ready for the first UT_Stub_NotReadyPolls polls of each byte.
 */
uint8_t  UT_Stub_TxBytes[64];
uint32_t UT_Stub_TxCount;
uint32_t UT_Stub_TxReadyCalls;
uint32_t UT_Stub_AcceptBytes;
uint32_t UT_Stub_NotReadyPolls;
uint32_t UT_Stub_PollsThisByte;

bool UT_Stub_TxReady(void)
{
    UT_Stub_TxReadyCalls++;

    if (UT_Stub_TxCount >= UT_Stub_AcceptBytes)
    {
        return false;
    }

    if (UT_Stub_PollsThisByte < UT_Stub_NotReadyPolls)
    {
        UT_Stub_PollsThisByte++;
        return false;
    }

    return true;
}

void UT_Stub_TxByte(uint8_t Byte)
{
    UT_Stub_TxBytes[UT_Stub_TxCount] = Byte;
    UT_Stub_TxCount++;
    UT_Stub_PollsThisByte = 0;
}
