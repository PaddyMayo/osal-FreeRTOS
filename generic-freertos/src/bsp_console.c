/*
 * File:  bsp_console.c
 *
 * Purpose:
 *   OSAL BSP debug console implementation, shared across every FreeRTOS
 *   BSP. The bytes go out through the platform's console backend, the
 *   generic-freertos/console/<CONSOLE>.h that osal_freertos_platform()
 *   selects via OSAL_FREERTOS_PLATFORM_CONSOLE_BACKEND. A backend provides:
 *
 *     size_t OS_FreeRTOS_ConsoleWrite(const char *Str, size_t Len)
 *         writes up to Len bytes, returning how many were written (0 = give up)
 *     void OS_FreeRTOS_PlatformHalt(void)
 *         stops the platform for good, e.g. abort() or disable IRQs and spin
 */

#include <string.h>

#include "FreeRTOS.h"

#include "bsp-impl.h"

#include OSAL_FREERTOS_PLATFORM_CONSOLE_BACKEND

/*----------------------------------------------------------------
   OS_BSP_ConsoleOutput_Impl
   See full description in header
 ------------------------------------------------------------------*/
void OS_BSP_ConsoleOutput_Impl(const char *Str, size_t DataLen)
{
    while (DataLen > 0)
    {
        size_t WriteLen;

        WriteLen = OS_FreeRTOS_ConsoleWrite(Str, DataLen);
        if (WriteLen == 0)
        {
            /* no recourse if this fails, just stop. */
            break;
        }
        Str     += WriteLen;
        DataLen -= WriteLen;
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

/*----------------------------------------------------------------
   OS_FreeRTOS_AssertFailed

   Called by configASSERT (see FreeRTOSConfig.h). Formats without stdio,
   and without the console lock, since it may run from an ISR or a
   critical section.
 ------------------------------------------------------------------*/
void OS_FreeRTOS_AssertFailed(const char *File, int Line)
{
    static const char Prefix[] = "configASSERT failed: ";
    char              Digits[sizeof(unsigned int) * 3U];
    size_t            Pos   = sizeof(Digits);
    unsigned int      Value = (unsigned int)Line;

    do
    {
        --Pos;
        Digits[Pos]  = (char)('0' + (Value % 10U));
        Value       /= 10U;
    } while (Value > 0U);

    OS_BSP_ConsoleOutput_Impl(Prefix, sizeof(Prefix) - 1U);
    OS_BSP_ConsoleOutput_Impl(File, strlen(File));
    OS_BSP_ConsoleOutput_Impl(":", 1U);
    OS_BSP_ConsoleOutput_Impl(&Digits[Pos], sizeof(Digits) - Pos);
    OS_BSP_ConsoleOutput_Impl("\n", 1U);

    OS_FreeRTOS_PlatformHalt();
}
