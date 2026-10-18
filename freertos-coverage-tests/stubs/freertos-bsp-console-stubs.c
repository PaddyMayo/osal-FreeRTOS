/*
 * File: freertos-bsp-console-stubs.c
 *
 * Purpose:
 *   Stand-in for the vendor's console device that generic-freertos/src/bsp_console.c
 *   writes to, so the test can see what the BSP hands it.
 */

#include <stddef.h>
#include <stdint.h>

/* What the last OS_BSP_Console_DeviceWrite() call was given, and how many calls there were */
const char *UT_Stub_LastWriteBuf;
size_t      UT_Stub_LastWriteLen;
uint32_t    UT_Stub_WriteCount;

void OS_BSP_Console_DeviceWrite(const char *Buf, size_t Len)
{
    UT_Stub_LastWriteBuf = Buf;
    UT_Stub_LastWriteLen = Len;
    UT_Stub_WriteCount++;
}
