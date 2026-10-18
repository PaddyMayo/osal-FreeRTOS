/*
 * File:  bsp_io.c
 *
 * Purpose:
 *   A console UART that accepts every byte and discards it, for the CI builds
 *   of the platforms. A vendor replaces this with its own device.
 */

#include "bsp_io.h"

static bool Uart_TxReady(void)
{
    return true;
}

static void Uart_TxByte(uint8_t Byte)
{
    (void)Byte;
}

struct OS_BSP_UartConfig OS_BSP_Uart_Configure(void)
{
    struct OS_BSP_UartConfig Config = {
        .BaudRate = 115200U,
        .ClockHz  = 16000000U,
        .TxReady  = Uart_TxReady,
        .TxByte   = Uart_TxByte,
    };

    return Config;
}
