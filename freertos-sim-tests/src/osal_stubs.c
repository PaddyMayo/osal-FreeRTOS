/*
 * File:  osal_stubs.c
 *
 * Purpose:
 *   What the OSAL API needs to link that the port does not implement yet, so a
 *   sim test can call it. It is test scaffolding and not part of the product:
 *   the port only has common, queue, tasks, files and filesys so far.
 *
 *   The global lock is real, a recursive mutex per object type, because OSAL
 *   takes it on every create and delete and the tasks under test run
 *   concurrently. The console is created so OS_API_Init() succeeds, and drops
 *   what is written to it. Everything else fails with OS_ERR_NOT_IMPLEMENTED.
 *   The stubs below it are generated from the OSAL _Impl prototypes.
 */

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "osapi.h"
#include "os-shared-binsem.h"
#include "os-shared-clock.h"
#include "os-shared-condvar.h"
#include "os-shared-console.h"
#include "os-shared-countsem.h"
#include "os-shared-dir.h"
#include "os-shared-file.h"
#include "os-shared-filesys.h"
#include "os-shared-idmap.h"
#include "os-shared-module.h"
#include "os-shared-mutex.h"
#include "os-shared-network.h"
#include "os-shared-printf.h"
#include "os-shared-sockets.h"
#include "os-shared-timebase.h"

#ifndef __linux__
#include <errno.h>
#include <stddef.h>

/*
 * OSAL's snprintf/vsnprintf pull newlib's malloc in, and with it _sbrk, which
 * needs a heap the Cortex-M linker scripts do not give. Any malloc fails.
 */
void *_sbrk(ptrdiff_t increment)
{
    (void)increment;
    errno = ENOMEM;
    return (void *)-1;
}
#endif

#define STUB_LOCK_TYPES (OS_OBJECT_TYPE_USER + 1)

static StaticSemaphore_t StubLockBuffers[STUB_LOCK_TYPES];
static SemaphoreHandle_t StubLocks[STUB_LOCK_TYPES];

static SemaphoreHandle_t Stub_GetLock(osal_objtype_t idtype)
{
    taskENTER_CRITICAL();
    if (StubLocks[idtype % STUB_LOCK_TYPES] == NULL)
    {
        StubLocks[idtype % STUB_LOCK_TYPES] =
            xSemaphoreCreateRecursiveMutexStatic(&StubLockBuffers[idtype % STUB_LOCK_TYPES]);
    }
    taskEXIT_CRITICAL();

    return StubLocks[idtype % STUB_LOCK_TYPES];
}

void OS_Lock_Global_Impl(osal_objtype_t idtype)
{
    (void)xSemaphoreTakeRecursive(Stub_GetLock(idtype), portMAX_DELAY);
}

void OS_Unlock_Global_Impl(osal_objtype_t idtype)
{
    (void)xSemaphoreGiveRecursive(Stub_GetLock(idtype));
}

/*
 * Called with the lock held, and must give it up while it waits: the task that
 * holds the object needs it to finish. Waits attempts^2 x 10 ms, as POSIX does.
 */
void OS_WaitForStateChange_Impl(osal_objtype_t objtype, uint32 attempts)
{
    SemaphoreHandle_t Lock = Stub_GetLock(objtype);

    (void)xSemaphoreGiveRecursive(Lock);
    vTaskDelay(pdMS_TO_TICKS(attempts * attempts * 10U) + 1U);
    (void)xSemaphoreTakeRecursive(Lock, portMAX_DELAY);
}

int32 OS_BinSemCreate_Impl(const OS_object_token_t *token, uint32 sem_initial_value, uint32 options)
{
    (void)token;
    (void)sem_initial_value;
    (void)options;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_BinSemDelete_Impl(const OS_object_token_t *token)
{
    (void)token;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_BinSemFlush_Impl(const OS_object_token_t *token)
{
    (void)token;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_BinSemGetInfo_Impl(const OS_object_token_t *token, OS_bin_sem_prop_t *bin_prop)
{
    (void)token;
    (void)bin_prop;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_BinSemGive_Impl(const OS_object_token_t *token)
{
    (void)token;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_BinSemTake_Impl(const OS_object_token_t *token)
{
    (void)token;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_BinSemTimedWait_Impl(const OS_object_token_t *token, uint32 msecs)
{
    (void)token;
    (void)msecs;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_CondVarBroadcast_Impl(const OS_object_token_t *token)
{
    (void)token;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_CondVarCreate_Impl(const OS_object_token_t *token, uint32 options)
{
    (void)token;
    (void)options;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_CondVarDelete_Impl(const OS_object_token_t *token)
{
    (void)token;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_CondVarGetInfo_Impl(const OS_object_token_t *token, OS_condvar_prop_t *condvar_prop)
{
    (void)token;
    (void)condvar_prop;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_CondVarLock_Impl(const OS_object_token_t *token)
{
    (void)token;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_CondVarSignal_Impl(const OS_object_token_t *token)
{
    (void)token;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_CondVarTimedWait_Impl(const OS_object_token_t *token, const OS_time_t *abs_wakeup_time)
{
    (void)token;
    (void)abs_wakeup_time;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_CondVarUnlock_Impl(const OS_object_token_t *token)
{
    (void)token;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_CondVarWait_Impl(const OS_object_token_t *token)
{
    (void)token;
    return OS_ERR_NOT_IMPLEMENTED;
}

/* OS_API_Init() creates the console and fails without one: it exists, its output goes nowhere */
int32 OS_ConsoleCreate_Impl(const OS_object_token_t *token)
{
    (void)token;
    return OS_SUCCESS;
}

void OS_ConsoleOutput_Impl(const OS_object_token_t *token)
{
    (void)token;
}

void OS_ConsoleWakeup_Impl(const OS_object_token_t *token)
{
    (void)token;
}

int32 OS_CountSemCreate_Impl(const OS_object_token_t *token, uint32 sem_initial_value, uint32 options)
{
    (void)token;
    (void)sem_initial_value;
    (void)options;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_CountSemDelete_Impl(const OS_object_token_t *token)
{
    (void)token;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_CountSemGetInfo_Impl(const OS_object_token_t *token, OS_count_sem_prop_t *count_prop)
{
    (void)token;
    (void)count_prop;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_CountSemGive_Impl(const OS_object_token_t *token)
{
    (void)token;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_CountSemTake_Impl(const OS_object_token_t *token)
{
    (void)token;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_CountSemTimedWait_Impl(const OS_object_token_t *token, uint32 msecs)
{
    (void)token;
    (void)msecs;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_DirClose_Impl(const OS_object_token_t *token)
{
    (void)token;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_DirCreate_Impl(const char *local_path, uint32 access)
{
    (void)local_path;
    (void)access;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_DirOpen_Impl(const OS_object_token_t *token, const char *local_path)
{
    (void)token;
    (void)local_path;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_DirRead_Impl(const OS_object_token_t *token, os_dirent_t *dirent)
{
    (void)token;
    (void)dirent;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_DirRemove_Impl(const char *local_path)
{
    (void)local_path;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_DirRewind_Impl(const OS_object_token_t *token)
{
    (void)token;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_GetLocalTime_Impl(OS_time_t *time_struct)
{
    (void)time_struct;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_GetMonotonicTime_Impl(OS_time_t *time_struct)
{
    (void)time_struct;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_ModuleGetInfo_Impl(const OS_object_token_t *token, OS_module_prop_t *module_prop)
{
    (void)token;
    (void)module_prop;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_ModuleLoad_Impl(const OS_object_token_t *token, const char *translated_path)
{
    (void)token;
    (void)translated_path;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_ModuleSymbolLookup_Impl(const OS_object_token_t *token, cpuaddr *SymbolAddress, const char *SymbolName)
{
    (void)token;
    (void)SymbolAddress;
    (void)SymbolName;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_ModuleUnload_Impl(const OS_object_token_t *token)
{
    (void)token;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_MutSemCreate_Impl(const OS_object_token_t *token, uint32 options)
{
    (void)token;
    (void)options;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_MutSemDelete_Impl(const OS_object_token_t *token)
{
    (void)token;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_MutSemGetInfo_Impl(const OS_object_token_t *token, OS_mut_sem_prop_t *mut_prop)
{
    (void)token;
    (void)mut_prop;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_MutSemGive_Impl(const OS_object_token_t *token)
{
    (void)token;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_MutSemTake_Impl(const OS_object_token_t *token)
{
    (void)token;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_NetworkGetHostName_Impl(char *host_name, size_t name_len)
{
    (void)host_name;
    (void)name_len;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_NetworkGetID_Impl(int32 *IdBuf)
{
    (void)IdBuf;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_SetLocalTime_Impl(const OS_time_t *time_struct)
{
    (void)time_struct;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_SocketAccept_Impl(const OS_object_token_t *sock_token,
                           const OS_object_token_t *conn_token,
                           OS_SockAddr_t           *Addr,
                           OS_time_t                abs_timeout)
{
    (void)sock_token;
    (void)conn_token;
    (void)Addr;
    (void)abs_timeout;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_SocketAddrFromString_Impl(OS_SockAddr_t *Addr, const char *string)
{
    (void)Addr;
    (void)string;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_SocketAddrGetPort_Impl(uint16 *PortNum, const OS_SockAddr_t *Addr)
{
    (void)PortNum;
    (void)Addr;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_SocketAddrInit_Impl(OS_SockAddr_t *Addr, OS_SocketDomain_t Domain)
{
    (void)Addr;
    (void)Domain;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_SocketAddrSetPort_Impl(OS_SockAddr_t *Addr, uint16 PortNum)
{
    (void)Addr;
    (void)PortNum;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_SocketAddrToString_Impl(char *buffer, size_t buflen, const OS_SockAddr_t *Addr)
{
    (void)buffer;
    (void)buflen;
    (void)Addr;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_SocketBindAddress_Impl(const OS_object_token_t *token, const OS_SockAddr_t *Addr)
{
    (void)token;
    (void)Addr;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_SocketConnect_Impl(const OS_object_token_t *token, const OS_SockAddr_t *Addr, OS_time_t abs_timeout)
{
    (void)token;
    (void)Addr;
    (void)abs_timeout;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_SocketGetInfo_Impl(const OS_object_token_t *token, OS_socket_prop_t *sock_prop)
{
    (void)token;
    (void)sock_prop;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_SocketGetOption_Impl(const OS_object_token_t *token, OS_socket_option_t opt_id, OS_socket_optval_t *optval)
{
    (void)token;
    (void)opt_id;
    (void)optval;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_SocketListen_Impl(const OS_object_token_t *token)
{
    (void)token;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_SocketOpen_Impl(const OS_object_token_t *token)
{
    (void)token;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_SocketRecvFrom_Impl(const OS_object_token_t *token,
                             void                    *buffer,
                             size_t                   buflen,
                             OS_SockAddr_t           *RemoteAddr,
                             OS_time_t                abs_timeout)
{
    (void)token;
    (void)buffer;
    (void)buflen;
    (void)RemoteAddr;
    (void)abs_timeout;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_SocketSendTo_Impl(const OS_object_token_t *token,
                           const void              *buffer,
                           size_t                   buflen,
                           const OS_SockAddr_t     *RemoteAddr)
{
    (void)token;
    (void)buffer;
    (void)buflen;
    (void)RemoteAddr;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_SocketSetOption_Impl(const OS_object_token_t  *token,
                              OS_socket_option_t        opt_id,
                              const OS_socket_optval_t *optval)
{
    (void)token;
    (void)opt_id;
    (void)optval;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_SocketShutdown_Impl(const OS_object_token_t *token, OS_SocketShutdownMode_t Mode)
{
    (void)token;
    (void)Mode;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_SymbolLookup_Impl(cpuaddr *SymbolAddress, const char *SymbolName)
{
    (void)SymbolAddress;
    (void)SymbolName;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_SymbolTableDump_Impl(const char *filename, size_t size_limit)
{
    (void)filename;
    (void)size_limit;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_TimeBaseCreate_Impl(const OS_object_token_t *token)
{
    (void)token;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_TimeBaseDelete_Impl(const OS_object_token_t *token)
{
    (void)token;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_TimeBaseGetInfo_Impl(const OS_object_token_t *token, OS_timebase_prop_t *timer_prop)
{
    (void)token;
    (void)timer_prop;
    return OS_ERR_NOT_IMPLEMENTED;
}

void OS_TimeBaseLock_Impl(const OS_object_token_t *token)
{
    (void)token;
}

int32 OS_TimeBaseSet_Impl(const OS_object_token_t *token, uint32 start_time, uint32 interval_time)
{
    (void)token;
    (void)start_time;
    (void)interval_time;
    return OS_ERR_NOT_IMPLEMENTED;
}

void OS_TimeBaseUnlock_Impl(const OS_object_token_t *token)
{
    (void)token;
}
