/*
 * File:  bsp_console.c
 *
 * Purpose:
 *   OSAL BSP debug console implementation, shared across every FreeRTOS
 *   BSP. The output goes to the vendor's console device, see
 *   generic_freertos_bsp_internal.h.
 *
 *   TODO: OS_BSP_ConsoleSetMode_Impl is a template.
 */

#include <stdint.h>

#include "bsp-impl.h"
#include "bsp_io.h"

/*----------------------------------------------------------------
   OS_BSP_ConsoleOutput_Impl
   See full description in header
 ------------------------------------------------------------------*/
void OS_BSP_ConsoleOutput_Impl(const char *Str, size_t DataLen)
{
    OS_BSP_Console_DeviceWrite(Str, DataLen);
}

/*----------------------------------------------------------------
   OS_BSP_ConsoleSetMode_Impl
   See full description in header
 ------------------------------------------------------------------*/
void OS_BSP_ConsoleSetMode_Impl(uint32 ModeBits)
{
    /* TODO: console color/attribute control, if the console supports it */
}
