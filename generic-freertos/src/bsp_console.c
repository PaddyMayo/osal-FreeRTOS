/*
 * File:  bsp_console.c
 *
 * Purpose:
 *   OSAL BSP debug console implementation, shared across every FreeRTOS
 *   BSP. The bytes go out through the UART that the vendor's
 *   OS_BSP_Uart_Configure sets up, see generic_freertos_bsp_internal.h.
 *
 *   TODO: OS_BSP_ConsoleSetMode_Impl is a template.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bsp-impl.h"
#include "generic_freertos_bsp_internal.h"
#include "bsp_io.h"

/*
 * The BSP never names the type that OS_BSP_Uart_Configure returns, so each member it
 * needs is read off the call, which fails the build when it is missing, and its type
 * is stated here. The call sits in a _Generic controlling expression, which is not
 * evaluated, so nothing runs.
 */
#define OS_BSP_UART_REQUIRE_MEMBER(Member, Type)                                  \
    _Static_assert(_Generic(OS_BSP_Uart_Configure().Member, Type: 1, default: 0), \
                   "the struct from OS_BSP_Uart_Configure must have " #Member " as " #Type)

OS_BSP_UART_REQUIRE_MEMBER(BaudRate, uint32_t);
OS_BSP_UART_REQUIRE_MEMBER(ClockHz, uint32_t);
OS_BSP_UART_REQUIRE_MEMBER(TxReady, bool (*)(void));
OS_BSP_UART_REQUIRE_MEMBER(TxByte, void (*)(uint8_t));

/*
 * A frame is 10 bits, and a poll takes at least one cycle, so waiting for as many
 * polls as there are cycles in 10 bit times covers the slowest byte. It is doubled
 * for margin.
 */
#define OS_BSP_CONSOLE_POLLS_PER_BIT_CYCLE 20ULL

/* The wait until the rates are known to be good, enough for the slowest UART on a fast clock */
#define OS_BSP_CONSOLE_FALLBACK_POLLS 100000U

static struct
{
    bool Usable;
    bool (*TxReady)(void);
    void (*TxByte)(uint8_t Byte);
    uint32_t PollLimit;
} OS_BSP_Console;

/*----------------------------------------------------------------
   OS_BSP_Console_DeviceInit
   See full description in generic_freertos_bsp_internal.h
 ------------------------------------------------------------------*/
int OS_BSP_Console_DeviceInit(void)
{
    /* The vendor's struct stays in this function */
    __typeof__(OS_BSP_Uart_Configure()) Config = OS_BSP_Uart_Configure();
    uint64_t                            Limit;

    OS_BSP_Console.Usable = false;

    /* Without both there is nothing to send through, so output is dropped */
    if (Config.TxReady == NULL || Config.TxByte == NULL)
    {
        return OS_BSP_CONSOLE_ERR_ACCESSOR;
    }

    /*
     * From here the UART may well work, so a bad rate does not silence the console:
     * main() reports the failure through it, with a fixed wait until the rates are
     * known to be good.
     */
    OS_BSP_Console.TxReady   = Config.TxReady;
    OS_BSP_Console.TxByte    = Config.TxByte;
    OS_BSP_Console.PollLimit = OS_BSP_CONSOLE_FALLBACK_POLLS;
    OS_BSP_Console.Usable    = true;

    if (Config.BaudRate == 0 || Config.ClockHz == 0)
    {
        return OS_BSP_CONSOLE_ERR_RATE;
    }

    if (Config.BaudRate > Config.ClockHz)
    {
        return OS_BSP_CONSOLE_ERR_RANGE;
    }

    Limit = (OS_BSP_CONSOLE_POLLS_PER_BIT_CYCLE * Config.ClockHz) / Config.BaudRate;
    if (Limit > UINT32_MAX)
    {
        Limit = UINT32_MAX;
    }

    OS_BSP_Console.PollLimit = (uint32_t)Limit;

    return 0;
}

/*----------------------------------------------------------------
   OS_BSP_Console_SendByte

   Waits for the UART for a bounded number of polls. A UART that never gets ready
   is given up on for good, so a broken one costs one wait, not one per byte, and
   the fatal paths still reach OS_BSP_Halt.
 ------------------------------------------------------------------*/
static bool OS_BSP_Console_SendByte(uint8_t Byte)
{
    uint32_t Polls = 0;

    while (!OS_BSP_Console.TxReady())
    {
        Polls++;
        if (Polls > OS_BSP_Console.PollLimit)
        {
            OS_BSP_Console.Usable = false;
            return false;
        }
    }

    OS_BSP_Console.TxByte(Byte);

    return true;
}

/*----------------------------------------------------------------
   OS_BSP_ConsoleOutput_Impl
   See full description in header
 ------------------------------------------------------------------*/
void OS_BSP_ConsoleOutput_Impl(const char *Str, size_t DataLen)
{
    size_t Index;

    if (!OS_BSP_Console.Usable)
    {
        return;
    }

    for (Index = 0; Index < DataLen; Index++)
    {
        /* Lines end in a plain '\n', a terminal wants "\r\n" */
        if (Str[Index] == '\n' && !OS_BSP_Console_SendByte('\r'))
        {
            return;
        }

        if (!OS_BSP_Console_SendByte((uint8_t)Str[Index]))
        {
            return;
        }
    }
}

/*----------------------------------------------------------------
   OS_BSP_ConsoleSetMode_Impl
   See full description in header
 ------------------------------------------------------------------*/
void OS_BSP_ConsoleSetMode_Impl(uint32 ModeBits)
{
    /* TODO: console color/attribute control, if the console supports it */
}
