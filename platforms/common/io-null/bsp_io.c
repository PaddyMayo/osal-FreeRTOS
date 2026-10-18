/*
 * File:  bsp_io.c
 *
 * Purpose:
 *   A console that discards its output and has no input, for the CI builds of
 *   the platforms. A vendor replaces this with its own device.
 */

#include "bsp_io.h"

void OS_BSP_Console_DeviceInit(void)
{
}

void OS_BSP_Console_DeviceWrite(const char *Buf, size_t Len)
{
    (void)Buf;
    (void)Len;
}

size_t OS_BSP_Console_DeviceRead(char *Buf, size_t MaxLen)
{
    (void)Buf;
    (void)MaxLen;
    return 0;
}
