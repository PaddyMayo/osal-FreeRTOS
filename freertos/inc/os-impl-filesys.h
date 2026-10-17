/*
 * File: os-impl-filesys.h
 *
 * Purpose:
 *   Internal definitions shared by the file and filesys parts of the FreeRTOS
 *   OSAL port: the one littlefs volume, the lock that guards every call into
 *   it, and the sizes the BSP's storage configuration has to respect.
 */

#ifndef OS_IMPL_FILESYS_H
#define OS_IMPL_FILESYS_H

#include "osconfig.h"
#include "common_types.h"

#include "lfs.h"

/* The BSP's storage configuration, OS_BSP_GenericFreeRTOSGlobal.StorageConfig */
#include "generic_freertos_bsp_internal.h"

/*
 * Cache each open file reserves. The cache_size of the BSP's storage
 * configuration must not exceed it, see OS_FileSysStartVolume_Impl().
 */
#ifndef OS_FREERTOS_LFS_FILE_CACHE_SIZE
#define OS_FREERTOS_LFS_FILE_CACHE_SIZE 128
#endif

/* The one volume, mounted and unmounted by the filesys implementation */
extern lfs_t OS_FreeRTOS_Lfs;

/*----------------------------------------------------------------
 *
 *  Purpose: Port-internal helper routine, not part of OSAL API.
 *           Creates the lock that serializes every call into OS_FreeRTOS_Lfs.
 *           Called from OS_API_Impl_Init.
 *
 *  Returns: OS_SUCCESS, or OS_ERROR if the lock could not be created
 *
 *-----------------------------------------------------------------*/
int32 OS_FreeRTOS_FileSysInit(void);

/*----------------------------------------------------------------
 *
 *  Purpose: Port-internal helper routines, not part of OSAL API.
 *           Take and give the lock around every call into OS_FreeRTOS_Lfs,
 *           which is not thread safe. Not recursive, and never held across a
 *           call to the other.
 *
 *-----------------------------------------------------------------*/
void OS_FreeRTOS_LfsLock(void);
void OS_FreeRTOS_LfsUnlock(void);

#endif /* OS_IMPL_FILESYS_H */
