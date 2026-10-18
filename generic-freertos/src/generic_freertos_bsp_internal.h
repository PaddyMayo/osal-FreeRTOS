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
#include <stddef.h>

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
 * Storage device functions
 *
 * Defined by the vendor, in the storage source listed in OSAL_FREERTOS_SOURCES.
 * main() fills the littlefs configuration from them, so one the vendor leaves
 * out is an undefined reference at link time.
 *
 * The callbacks return 0, or a negative LFS_ERR_* value (LFS_ERR_IO for a failed
 * transfer, LFS_ERR_CORRUPT for a block that went bad). The buffers must exist
 * for the life of the program (littlefs does not malloc, LFS_NO_MALLOC), and the
 * cache size must not exceed OS_FREERTOS_LFS_FILE_CACHE_SIZE (see
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

lfs_size_t OS_BSP_Storage_ReadSize(void);
lfs_size_t OS_BSP_Storage_ProgSize(void);
lfs_size_t OS_BSP_Storage_BlockSize(void);
lfs_size_t OS_BSP_Storage_BlockCount(void);
int32_t    OS_BSP_Storage_BlockCycles(void);
lfs_size_t OS_BSP_Storage_CacheSize(void);
lfs_size_t OS_BSP_Storage_LookaheadSize(void);
void      *OS_BSP_Storage_ReadBuffer(void);
void      *OS_BSP_Storage_ProgBuffer(void);
void      *OS_BSP_Storage_LookaheadBuffer(void);

/*
 * Console device functions
 *
 * Defined by the vendor, in the console source listed in OSAL_FREERTOS_SOURCES
 * (bsp_io.c, with bsp_io.h next to it). One the vendor leaves out is an
 * undefined reference at link time.
 *
 * DeviceInit is called once, first thing in main(): the scheduler has not started,
 * interrupts are at their reset state and only one thread exists. It makes
 * DeviceWrite usable (clocks, pins, baud rate). It cannot report a failure, so a
 * device that fails to start must make DeviceWrite do nothing.
 *
 * DeviceWrite sends Len bytes. Buf is read-only, valid only for the call and not
 * NUL-terminated, and Len may be 0 or as large as the console buffer. The bytes
 * are sent as given: lines end in a plain '\n', so a device that needs "\r\n"
 * converts them itself. All of them must be handed to the hardware before it
 * returns, since nothing is left to finish the job later. It must always return,
 * even with nothing listening on the other end: it never waits on flow control,
 * and drops what the device still refuses after a bounded time, so the fatal
 * paths always reach OS_BSP_Halt.
 *
 * DeviceWrite runs from a task holding the BSP lock, and also, without the lock,
 * from the fatal paths in bsp_start.c, which can run from an ISR, a critical
 * section, with interrupts masked, before the scheduler starts, or in the middle
 * of another DeviceWrite on the same CPU. So it must not call FreeRTOS, use the
 * heap, wait for an interrupt or assume a lock, it must leave the hardware usable
 * if a second call interrupts it, and it must fit the stack of an ISR.
 *
 * DeviceRead copies up to MaxLen received bytes and returns how many, 0 when none
 * are waiting. It never blocks and is only called from a task, by one reader at a
 * time. OSAL itself never calls it. The vendor owns the receive buffering and its
 * overrun policy; a device with no input returns 0.
 */
void   OS_BSP_Console_DeviceInit(void);
void   OS_BSP_Console_DeviceWrite(const char *Buf, size_t Len);
size_t OS_BSP_Console_DeviceRead(char *Buf, size_t MaxLen);

#endif /* GENERIC_FREERTOS_BSP_INTERNAL_H */
