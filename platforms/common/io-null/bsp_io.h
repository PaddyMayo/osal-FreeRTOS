/*
 * File:  bsp_io.h
 *
 * Purpose:
 *   The console functions of the null device that CI builds use, see
 *   bsp_io.c. This is not part of the port: a vendor supplies its own
 *   bsp_io.h and console source in OSAL_FREERTOS_SOURCES, and
 *   generic-freertos/src/generic_freertos_bsp_internal.h describes what
 *   they must provide.
 */

#ifndef BSP_IO_H
#define BSP_IO_H

#include <stddef.h>

void   OS_BSP_Console_DeviceInit(void);
void   OS_BSP_Console_DeviceWrite(const char *Buf, size_t Len);
size_t OS_BSP_Console_DeviceRead(char *Buf, size_t MaxLen);

#endif /* BSP_IO_H */
