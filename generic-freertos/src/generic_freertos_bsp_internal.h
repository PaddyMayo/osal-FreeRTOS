/*
 * File:  generic_freertos_bsp_internal.h
 *
 * Purpose:
 *   Internal state of the generic FreeRTOS BSP, shared by bsp_start.c, which
 *   defines it, and bsp_console.c.
 */

#ifndef GENERIC_FREERTOS_BSP_INTERNAL_H
#define GENERIC_FREERTOS_BSP_INTERNAL_H

#include <stdbool.h>

#include "lfs.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include "osapi-common.h"
#include "bsp-impl.h"

/*
 * BSP types
 */
typedef struct
{
    /*
     * Console output may be locked/unlocked from any FreeRTOS task, so a
     * statically-allocated mutex is used (avoids relying on the heap, and is
     * always valid even if referenced before the scheduler is started).
     */
    StaticSemaphore_t ConsoleMutexBuffer;
    SemaphoreHandle_t ConsoleMutex;

    /*
     * Set by OS_BSP_StartupTask, which only runs once the scheduler is up. If
     * vTaskStartScheduler() returns with this still false, the scheduler never
     * started.
     */
    volatile bool SchedulerRan;

    /* Set by the first fatal error, see OS_BSP_BeginHalt */
    volatile bool Halting;

    /*
     * The littlefs configuration of the storage device, a static object in
     * main() that lives as long as the program. Set by main() before it fills
     * the members.
     */
    struct lfs_config *StorageConfig;
} OS_BSP_GenericFreeRTOSGlobalData_t;

/*
 * Global Data object
 */
extern OS_BSP_GenericFreeRTOSGlobalData_t OS_BSP_GenericFreeRTOSGlobal;

/*
 * Storage device
 *
 * Defined by the vendor: the callbacks in the storage source listed in
 * OSAL_FREERTOS_SOURCES, the rest as macros in the bsp_storage.h next to it.
 * main() fills the littlefs configuration from them.
 *
 * The callbacks are functions, so one the vendor leaves out is an undefined
 * reference at link time. They return 0, or a negative LFS_ERR_* value
 * (LFS_ERR_IO for a failed transfer, LFS_ERR_CORRUPT for a block that went bad).
 *
 * The rest are macros, so bsp_start.c checks them at build time: one the vendor
 * leaves out stops the build with an #error, and a value that breaks a rule
 * littlefs asserts when it mounts stops it with a _Static_assert. The sizes, the
 * count and the cycles are integer constant expressions.
 *
 *   OS_BSP_STORAGE_READ_SIZE         bytes per read, not 0, divides the block and cache sizes
 *   OS_BSP_STORAGE_PROG_SIZE         bytes per program, not 0, divides the block and cache sizes
 *   OS_BSP_STORAGE_BLOCK_SIZE        bytes per erasable block, not 0, a multiple of the cache size
 *   OS_BSP_STORAGE_BLOCK_COUNT       blocks on the device, not 0
 *   OS_BSP_STORAGE_BLOCK_CYCLES      erases before a block is moved, -1 for never
 *   OS_BSP_STORAGE_CACHE_SIZE        bytes per cache, not 0, a multiple of the read and prog sizes
 *   OS_BSP_STORAGE_LOOKAHEAD_SIZE    bytes of lookahead window, not 0, a multiple of 8
 *   OS_BSP_STORAGE_READ_BUFFER       the read cache, CACHE_SIZE bytes
 *   OS_BSP_STORAGE_PROG_BUFFER       the program cache, CACHE_SIZE bytes
 *   OS_BSP_STORAGE_LOOKAHEAD_BUFFER  the lookahead window, LOOKAHEAD_SIZE bytes
 *
 * Each buffer is the name of an array of static storage, since it must exist for
 * the life of the program (littlefs does not malloc, LFS_NO_MALLOC). The build
 * checks that it is an array, not a pointer, of at least the size given above.
 * The cache size must not exceed OS_FREERTOS_LFS_FILE_CACHE_SIZE (see
 * freertos/inc/os-impl-filesys.h).
 */
int OS_BSP_Storage_Read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size);
int OS_BSP_Storage_Prog(const struct lfs_config *c,
                        lfs_block_t              block,
                        lfs_off_t                off,
                        const void              *buffer,
                        lfs_size_t               size);
int OS_BSP_Storage_Erase(const struct lfs_config *c, lfs_block_t block);
int OS_BSP_Storage_Sync(const struct lfs_config *c);

/*
 * Console UART
 *
 * The vendor supplies one function, OS_BSP_Uart_Configure, declared in its bsp_io.h
 * and defined in the console source listed in OSAL_FREERTOS_SOURCES (bsp_io.c, with
 * bsp_io.h next to it). It brings the UART up however the hardware needs (pins,
 * clock gating, registers) and returns a struct with at least these members. The
 * BSP never names the struct's type, so the vendor chooses it.
 *
 *   uint32_t BaudRate           bits per second the device runs at
 *   uint32_t ClockHz            clock feeding the UART, used to bound the wait
 *   bool   (*TxReady)(void)     true if TxByte would be taken now; never blocks
 *   void   (*TxByte)(uint8_t)   hands one byte to the UART and returns at once
 *
 * A member that is missing or has another type fails the build, see bsp_console.c.
 * There is no status member: a vendor that cannot bring the UART up returns a struct
 * of zeros, which OS_BSP_Console_DeviceInit rejects.
 *
 * OS_BSP_Uart_Configure runs once, first thing in main(): the scheduler has not
 * started, interrupts are at their reset state and only one thread exists. It must
 * not use FreeRTOS or the heap.
 *
 * TxReady and TxByte are a few register accesses. They run from a task holding the
 * BSP lock, and also, without it, from the fatal paths in bsp_start.c, which can run
 * from an ISR, a critical section or with interrupts masked, or in the middle of the
 * same functions on the same CPU. So they must not call FreeRTOS, use the heap or
 * block, and must fit the stack of an ISR. A fatal path that interrupts the normal
 * one between TxReady and TxByte can garble bytes, but never hangs.
 *
 * The loop over the bytes, the "\r\n" conversion and the bounded wait for TxReady
 * are the BSP's, in bsp_console.c.
 */

/*
 * What OS_BSP_Console_DeviceInit returns, 0 when the console is usable. The codes
 * are positive because OS_BSP_ReportNumber prints a negative number as 0.
 */
#define OS_BSP_CONSOLE_ERR_ACCESSOR 1 /* TxReady or TxByte is NULL */
#define OS_BSP_CONSOLE_ERR_RATE     2 /* BaudRate or ClockHz is 0 */
#define OS_BSP_CONSOLE_ERR_RANGE    3 /* BaudRate is above ClockHz */

/*
 * Calls OS_BSP_Uart_Configure and checks what it returned. With an accessor missing
 * the console drops everything written to it. With a bad rate it stays usable, so
 * main() can report the failure through it.
 */
int OS_BSP_Console_DeviceInit(void);

#endif /* GENERIC_FREERTOS_BSP_INTERNAL_H */
