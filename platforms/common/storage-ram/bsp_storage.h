/*
 * File:  bsp_storage.h
 *
 * Purpose:
 *   The storage functions of the RAM-backed device that CI builds use, see
 *   bsp_storage.c. This is not part of the port: a vendor supplies its own
 *   bsp_storage.h and storage source in OSAL_FREERTOS_SOURCES, and
 *   generic-freertos/src/bsp_start.c describes what they must provide.
 */

#ifndef BSP_STORAGE_H
#define BSP_STORAGE_H

#include <stdint.h>

#include "lfs.h"

int OS_BSP_Storage_Read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size);
int OS_BSP_Storage_Prog(const struct lfs_config *c,
                        lfs_block_t              block,
                        lfs_off_t                off,
                        const void              *buffer,
                        lfs_size_t               size);
int OS_BSP_Storage_Erase(const struct lfs_config *c, lfs_block_t block);
int OS_BSP_Storage_Sync(const struct lfs_config *c);

#define OS_BSP_STORAGE_READ_SIZE      16U
#define OS_BSP_STORAGE_PROG_SIZE      16U
#define OS_BSP_STORAGE_BLOCK_SIZE     256U
#define OS_BSP_STORAGE_BLOCK_COUNT    12U
/* RAM does not wear out, so blocks are never moved */
#define OS_BSP_STORAGE_BLOCK_CYCLES   (-1)
#define OS_BSP_STORAGE_CACHE_SIZE     128U
#define OS_BSP_STORAGE_LOOKAHEAD_SIZE 16U

extern uint8_t OS_BSP_StorageReadBuffer[OS_BSP_STORAGE_CACHE_SIZE];
extern uint8_t OS_BSP_StorageProgBuffer[OS_BSP_STORAGE_CACHE_SIZE];
extern uint8_t OS_BSP_StorageLookaheadBuffer[OS_BSP_STORAGE_LOOKAHEAD_SIZE];

#define OS_BSP_STORAGE_READ_BUFFER      OS_BSP_StorageReadBuffer
#define OS_BSP_STORAGE_PROG_BUFFER      OS_BSP_StorageProgBuffer
#define OS_BSP_STORAGE_LOOKAHEAD_BUFFER OS_BSP_StorageLookaheadBuffer

#endif /* BSP_STORAGE_H */
