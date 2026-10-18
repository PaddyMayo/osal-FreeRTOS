/*
 * File:  bsp_io.h
 *
 * Purpose:
 *   The console UART of the null device that CI builds use, see bsp_io.c. This
 *   is not part of the port: a vendor supplies its own bsp_io.h and console
 *   source in OSAL_FREERTOS_SOURCES, and generic_freertos_bsp_internal.h
 *   describes the struct that OS_BSP_Uart_Configure must return.
 */

#ifndef BSP_IO_H
#define BSP_IO_H

#include <stdbool.h>
#include <stdint.h>

// The BSP reads these through the type that OS_BSP_Uart_Configure returns, which cppcheck does not follow
struct OS_BSP_UartConfig
{
    // cppcheck-suppress unusedStructMember
    uint32_t BaudRate;
    // cppcheck-suppress unusedStructMember
    uint32_t ClockHz;
    // cppcheck-suppress unusedStructMember
    bool (*TxReady)(void);
    // cppcheck-suppress unusedStructMember
    void (*TxByte)(uint8_t Byte);
};

struct OS_BSP_UartConfig OS_BSP_Uart_Configure(void);

#endif /* BSP_IO_H */
