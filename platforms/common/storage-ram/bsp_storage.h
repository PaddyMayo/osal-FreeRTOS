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

#endif /* BSP_STORAGE_H */
