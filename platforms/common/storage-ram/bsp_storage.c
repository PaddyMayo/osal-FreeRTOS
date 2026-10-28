/*
 * File:  bsp_storage.c
 *
 * Purpose:
 *   A storage device that lives in RAM, for the CI builds of the platforms.
 *   It loses its contents at reset, so the file system is formatted on every
 *   boot. A vendor replaces this with its own device.
 */

#include <string.h>

#include "bsp_storage.h"

static uint8_t Storage_Blocks[OS_BSP_STORAGE_BLOCK_COUNT][OS_BSP_STORAGE_BLOCK_SIZE];

_Alignas(4) uint8_t OS_BSP_StorageReadBuffer[OS_BSP_STORAGE_CACHE_SIZE];
_Alignas(4) uint8_t OS_BSP_StorageProgBuffer[OS_BSP_STORAGE_CACHE_SIZE];
_Alignas(4) uint8_t OS_BSP_StorageLookaheadBuffer[OS_BSP_STORAGE_LOOKAHEAD_SIZE];

int OS_BSP_Storage_Read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size)
{
    (void)c;
    memcpy(buffer, &Storage_Blocks[block][off], size);
    return 0;
}

int OS_BSP_Storage_Prog(const struct lfs_config *c,
                        lfs_block_t              block,
                        lfs_off_t                off,
                        const void              *buffer,
                        lfs_size_t               size)
{
    (void)c;
    memcpy(&Storage_Blocks[block][off], buffer, size);
    return 0;
}

int OS_BSP_Storage_Erase(const struct lfs_config *c, lfs_block_t block)
{
    (void)c;
    memset(Storage_Blocks[block], 0xFF, OS_BSP_STORAGE_BLOCK_SIZE);
    return 0;
}

int OS_BSP_Storage_Sync(const struct lfs_config *c)
{
    (void)c;
    return 0;
}
