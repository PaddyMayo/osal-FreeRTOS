/*
 * File:  bsp_console.c
 *
 * Purpose:
 *   OSAL BSP debug console implementation, shared across every FreeRTOS
 *   BSP.
 *
 *   TODO: no console backend yet, OS_BSP_ConsoleOutput_Impl and
 *   OS_BSP_ConsoleSetMode_Impl are templates.
 */

#include <stdint.h>

#include "bsp-impl.h"

/*----------------------------------------------------------------
   OS_BSP_ConsoleOutput_Impl
   See full description in header
 ------------------------------------------------------------------*/
void OS_BSP_ConsoleOutput_Impl(const char *Str, size_t DataLen)
{
    /* TODO: write DataLen bytes of Str to the console */
}

/*----------------------------------------------------------------
   OS_BSP_ConsoleSetMode_Impl
   See full description in header
 ------------------------------------------------------------------*/
void OS_BSP_ConsoleSetMode_Impl(uint32 ModeBits)
{
    /* TODO: console color/attribute control, if the console supports it */
}
