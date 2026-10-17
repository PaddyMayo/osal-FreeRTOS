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

#define STORAGE_READ_SIZE      16U
#define STORAGE_PROG_SIZE      16U
#define STORAGE_BLOCK_SIZE     256U
#define STORAGE_BLOCK_COUNT    12U
#define STORAGE_CACHE_SIZE     128U
#define STORAGE_LOOKAHEAD_SIZE 16U

static uint8_t Storage_Blocks[STORAGE_BLOCK_COUNT][STORAGE_BLOCK_SIZE];

static _Alignas(4) uint8_t Storage_ReadBuffer[STORAGE_CACHE_SIZE];
static _Alignas(4) uint8_t Storage_ProgBuffer[STORAGE_CACHE_SIZE];
static _Alignas(4) uint8_t Storage_LookaheadBuffer[STORAGE_LOOKAHEAD_SIZE];

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
    memset(Storage_Blocks[block], 0xFF, STORAGE_BLOCK_SIZE);
    return 0;
}

int OS_BSP_Storage_Sync(const struct lfs_config *c)
{
    (void)c;
    return 0;
}

lfs_size_t OS_BSP_Storage_ReadSize(void)
{
    return STORAGE_READ_SIZE;
}

lfs_size_t OS_BSP_Storage_ProgSize(void)
{
    return STORAGE_PROG_SIZE;
}

lfs_size_t OS_BSP_Storage_BlockSize(void)
{
    return STORAGE_BLOCK_SIZE;
}

lfs_size_t OS_BSP_Storage_BlockCount(void)
{
    return STORAGE_BLOCK_COUNT;
}

/* RAM does not wear out, so blocks are never moved */
int32_t OS_BSP_Storage_BlockCycles(void)
{
    return -1;
}

lfs_size_t OS_BSP_Storage_CacheSize(void)
{
    return STORAGE_CACHE_SIZE;
}

lfs_size_t OS_BSP_Storage_LookaheadSize(void)
{
    return STORAGE_LOOKAHEAD_SIZE;
}

void *OS_BSP_Storage_ReadBuffer(void)
{
    return Storage_ReadBuffer;
}

void *OS_BSP_Storage_ProgBuffer(void)
{
    return Storage_ProgBuffer;
}

void *OS_BSP_Storage_LookaheadBuffer(void)
{
    return Storage_LookaheadBuffer;
}
