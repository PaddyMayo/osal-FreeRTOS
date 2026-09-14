/*
 * File:  bsp_console.c
 *
 * Purpose:
 *   OSAL BSP debug console implementation for the FreeRTOS POSIX simulator.
 */

#include <unistd.h>

#include "bsp-impl.h"

/*----------------------------------------------------------------
   OS_BSP_ConsoleOutput_Impl
   See full description in header
 ------------------------------------------------------------------*/
void OS_BSP_ConsoleOutput_Impl(const char *Str, size_t DataLen)
{
    while (DataLen > 0)
    {
        ssize_t WriteLen;

        /* writes the raw data directly to STDOUT_FILENO (unbuffered) */
        WriteLen = write(STDOUT_FILENO, Str, DataLen);
        if (WriteLen <= 0)
        {
            /* no recourse if this fails, just stop. */
            break;
        }
        Str     += WriteLen;
        DataLen -= (size_t)WriteLen;
    }
}

/*----------------------------------------------------------------
   OS_BSP_ConsoleSetMode_Impl
   See full description in header

   This BSP does not support console color/attribute control.
 ------------------------------------------------------------------*/
void OS_BSP_ConsoleSetMode_Impl(uint32 ModeBits)
{
}
