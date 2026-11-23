/*
 * File: coveragetest-idmap.c
 *
 * Purpose:
 *   White-box coverage test cases for freertos/src/os-impl-idmap.c, run against
 *   the fake FreeRTOS API in ../fake-inc and stubbed in
 *   ../stubs/freertos-idmap-stubs.c - no real FreeRTOS kernel involved. The
 *   shared-layer calls (OS_ObjectIdGetById() and friends) are OSAL's own
 *   stubs.
 *
 *   Each function is called directly. The table locks and the wait user list
 *   are private, so they are observed through what the port does with them:
 *   which semaphores are taken and given, which tasks are woken, and which
 *   refcounts are given back.
 */

#include <string.h>

#include "utassert.h"
#include "uttest.h"
#include "utstubs.h"

#include "FreeRTOS.h"
#include "task.h"

#include "osapi-idmap.h"
#include "os-shared-idmap.h"

#include "os-freertos.h"

#define UT_TASK_1 ((TaskHandle_t)0x7001)
#define UT_TASK_2 ((TaskHandle_t)0x7002)
#define UT_TASK_3 ((TaskHandle_t)0x7003)

#define UT_TYPE_A OS_OBJECT_TYPE_OS_QUEUE
#define UT_TYPE_B OS_OBJECT_TYPE_OS_TASK

/* Captured stub call arguments, declared in freertos-idmap-stubs.c */
extern TaskHandle_t               UT_Stub_IdmapCurrentTask;
extern TaskHandle_t               UT_Stub_IdmapAborted[];
extern uint32                     UT_Stub_IdmapAbortedCount;
extern TickType_t                 UT_Stub_IdmapLastDelayTicks;
extern OS_FreeRTOS_TaskDiedHook_t UT_Stub_IdmapDiedHook;
extern uint32                     UT_Stub_IdmapLastMs;
extern TickType_t                 UT_Stub_IdmapMsToTicksResult;

/* The records OS_ObjectIdGlobalFromToken() hands out, by the token's index */
#define UT_RECORDS 4
static OS_common_record_t UT_Records[UT_RECORDS];

static void UT_RecordHandler(void *UserObj, UT_EntryKey_t FuncKey, const UT_StubContext_t *Context)
{
    const OS_object_token_t *token  = UT_Hook_GetArgValueByName(Context, "token", const OS_object_token_t *);
    OS_common_record_t      *record = &UT_Records[token->obj_idx];

    (void)UserObj;

    UT_Stub_SetReturnValue(FuncKey, record);
}

static OS_object_token_t UT_MakeToken(osal_objtype_t type, uint32 idx)
{
    OS_object_token_t token;

    memset(&token, 0, sizeof(token));
    token.obj_type = type;
    token.obj_idx  = OSAL_INDEX_C(idx);
    token.obj_id   = (osal_id_t) { idx };

    return token;
}

/* Marks a record as being deleted (reserved) or not */
static void UT_SetRecordDeleted(uint32 idx, bool deleted)
{
    UT_Records[idx].active_id = deleted ? OS_OBJECT_ID_RESERVED : (osal_id_t) { 0x100 + idx };
}

/* Enters a wait user as the given task, on the object at idx of a type */
static void UT_Enter(OS_impl_wait_user_t *user, TaskHandle_t task, osal_objtype_t type, uint32 idx)
{
    OS_object_token_t token = UT_MakeToken(type, idx);

    UT_Stub_IdmapCurrentTask = task;
    memset(user, 0, sizeof(*user));
    UtAssert_INT32_EQ(OS_FreeRTOS_WaitEnter(&token, user), OS_SUCCESS);
}

static bool UT_Aborted(TaskHandle_t task)
{
    uint32 i;

    for (i = 0; i < UT_Stub_IdmapAbortedCount; ++i)
    {
        if (UT_Stub_IdmapAborted[i] == task)
        {
            return true;
        }
    }

    return false;
}

/*
 * What the vTaskDelay() stub does, so a contended lock can be freed from inside
 * the delay, as the task that holds it would, and what a lock looks like to
 * another task while this one sleeps
 */
typedef enum
{
    UT_DELAY_NOTHING,
    UT_DELAY_UNLOCK, /* releases the lock of UT_DelayType, as its holder running during the sleep would */
    UT_DELAY_PROBE   /* tries the lock of UT_DelayType as another task, and notes whether it had to wait */
} UT_DelayMode_t;

static UT_DelayMode_t UT_DelayMode;
static osal_objtype_t UT_DelayType;
static uint32         UT_DelayCalls;
static uint32         UT_ProbeWaited;

static int32 UT_DelayHook(void *UserObj, int32 StubRetcode, uint32 CallCount, const UT_StubContext_t *Context)
{
    UT_DelayMode_t mode = UT_DelayMode;

    (void)UserObj;
    (void)CallCount;
    (void)Context;

    ++UT_DelayCalls;

    if (mode == UT_DELAY_UNLOCK)
    {
        OS_Unlock_Global_Impl(UT_DelayType);
    }
    else if (mode == UT_DELAY_PROBE)
    {
        uint32       before;
        TaskHandle_t task;

        UT_DelayMode = UT_DELAY_NOTHING;
        task         = UT_Stub_IdmapCurrentTask;
        before       = UT_DelayCalls;

        UT_Stub_IdmapCurrentTask = UT_TASK_3;
        UT_DelayMode             = UT_DELAY_UNLOCK;
        OS_Lock_Global_Impl(UT_DelayType);
        UT_ProbeWaited = UT_DelayCalls - before;
        OS_Unlock_Global_Impl(UT_DelayType);

        UT_Stub_IdmapCurrentTask = task;
        UT_DelayMode             = UT_DELAY_PROBE;
    }
    else
    {
        /* nothing to do */
    }

    return StubRetcode;
}

/*
 * What the OS_ObjectIdRelease() stub does besides counting: marks the token released as the real one does, and
 * notes how many critical sections had been entered by then, which is how the test sees what was done before it
 */
static uint32 UT_ReleaseCriticals;

static int32 UT_ReleaseHook(void *UserObj, int32 StubRetcode, uint32 CallCount, const UT_StubContext_t *Context)
{
    OS_object_token_t *token = UT_Hook_GetArgValueByName(Context, "token", OS_object_token_t *);

    (void)UserObj;
    (void)CallCount;

    token->lock_mode    = OS_LOCK_MODE_NONE;
    UT_ReleaseCriticals = UT_GetStubCount(UT_KEY(vTaskEnterCritical));

    return StubRetcode;
}

/*
 * Tells whether the lock of a type is held, by trying it from another task, which waits one tick if it is. The
 * lock is given back to holder afterwards, so looking does not change it.
 */
static bool UT_LockIsHeld(osal_objtype_t type, TaskHandle_t holder)
{
    TaskHandle_t task = UT_Stub_IdmapCurrentTask;
    uint32       before;
    bool         held;

    UT_Stub_IdmapCurrentTask = UT_TASK_3;
    UT_DelayType             = type;
    UT_DelayMode             = UT_DELAY_UNLOCK;
    before                   = UT_DelayCalls;
    OS_Lock_Global_Impl(type);
    OS_Unlock_Global_Impl(type);
    UT_DelayMode = UT_DELAY_NOTHING;
    held         = (UT_DelayCalls != before);

    if (held)
    {
        UT_Stub_IdmapCurrentTask = holder;
        OS_Lock_Global_Impl(type);
    }

    UT_Stub_IdmapCurrentTask = task;

    return held;
}

/*
**********************************************************************************
**          TEST CASES
**********************************************************************************
*/

void Test_OS_FreeRTOS_TableMutex_Init(void)
{
    /* every lock is reset, and the task death hook is registered */
    UtAssert_INT32_EQ(OS_FreeRTOS_TableMutex_Init(), OS_SUCCESS);
    UtAssert_STUB_COUNT(OS_FreeRTOS_OnTaskDied, 1);
    UtAssert_True(UT_Stub_IdmapDiedHook != NULL, "task death hook registered");
    UtAssert_True(!UT_LockIsHeld(UT_TYPE_A, NULL), "a lock starts free");

    /* a refused hook registration fails the init */
    UT_SetDeferredRetcode(UT_KEY(OS_FreeRTOS_OnTaskDied), 1, -1);
    UtAssert_INT32_EQ(OS_FreeRTOS_TableMutex_Init(), OS_ERROR);
    UtAssert_STUB_COUNT(OS_FreeRTOS_OnTaskDied, 2);

    /* a second init forgets who held a lock, and who was waiting */
    UtAssert_INT32_EQ(OS_FreeRTOS_TableMutex_Init(), OS_SUCCESS);
    UT_Stub_IdmapCurrentTask = UT_TASK_1;
    OS_Lock_Global_Impl(UT_TYPE_A);
    UtAssert_True(UT_LockIsHeld(UT_TYPE_A, UT_TASK_1), "the lock is held");
    UtAssert_INT32_EQ(OS_FreeRTOS_TableMutex_Init(), OS_SUCCESS);
    UtAssert_True(!UT_LockIsHeld(UT_TYPE_A, NULL), "init frees a held lock");
}

void Test_OS_Lock_Global_Impl(void)
{
    UtAssert_INT32_EQ(OS_FreeRTOS_TableMutex_Init(), OS_SUCCESS);
    UT_Stub_IdmapCurrentTask = UT_TASK_1;

    /* a free lock -> taken at once, without waiting, and only that type's */
    OS_Lock_Global_Impl(UT_TYPE_A);
    UtAssert_UINT32_EQ(UT_DelayCalls, 0);
    UtAssert_True(UT_LockIsHeld(UT_TYPE_A, UT_TASK_1), "the lock is held");
    UtAssert_True(!UT_LockIsHeld(UT_TYPE_B, NULL), "another type's lock is not");

    /* a lock held by another task -> polled by one tick until it is free, then taken */
    UT_DelayCalls            = 0;
    UT_Stub_IdmapCurrentTask = UT_TASK_2;
    UT_DelayType             = UT_TYPE_A;
    UT_DelayMode             = UT_DELAY_UNLOCK;
    OS_Lock_Global_Impl(UT_TYPE_A);
    UT_DelayMode = UT_DELAY_NOTHING;
    UtAssert_UINT32_EQ(UT_DelayCalls, 1);
    UtAssert_UINT32_EQ(UT_Stub_IdmapLastDelayTicks, 1);
    UtAssert_True(UT_LockIsHeld(UT_TYPE_A, UT_TASK_2), "the lock is held again, by the second task");

    /* unlock frees it: the dead task's lock, which is the second one's now, is not given back by the first's death */
    UT_Stub_IdmapDiedHook(UT_TASK_1);
    UtAssert_True(UT_LockIsHeld(UT_TYPE_A, UT_TASK_2), "the other task's death leaves the lock alone");
    OS_Unlock_Global_Impl(UT_TYPE_A);
    UtAssert_True(!UT_LockIsHeld(UT_TYPE_A, NULL), "unlock frees the lock");

    /* a type with no table -> not taken, not waited for */
    UT_DelayCalls = 0;
    OS_Lock_Global_Impl(OS_OBJECT_TYPE_USER);
    OS_Lock_Global_Impl(OS_OBJECT_TYPE_USER);
    OS_Unlock_Global_Impl(OS_OBJECT_TYPE_USER);
    OS_Lock_Global_Impl(OS_OBJECT_TYPE_USER + 1);
    OS_Unlock_Global_Impl(OS_OBJECT_TYPE_USER + 1);
    UtAssert_UINT32_EQ(UT_DelayCalls, 0);
}

void Test_OS_Lock_Global_Impl_NotCreated(void)
{
    /*
     * Before the init has run (OSAL's own init does, ahead of OS_API_Impl_Init()) no lock exists: the calls do
     * nothing, however often they are made. This test runs before any init in this runner.
     */
    OS_Lock_Global_Impl(UT_TYPE_A);
    OS_Lock_Global_Impl(UT_TYPE_A);
    OS_Unlock_Global_Impl(UT_TYPE_A);
    UtAssert_UINT32_EQ(UT_DelayCalls, 0);
    UtAssert_STUB_COUNT(vTaskEnterCritical, 0);
}

void Test_OS_WaitForStateChange_Impl(void)
{
    OS_impl_wait_user_t reserved_a;
    OS_impl_wait_user_t live_a;
    OS_impl_wait_user_t reserved_b;
    OS_impl_wait_user_t released_a;

    UtAssert_INT32_EQ(OS_FreeRTOS_TableMutex_Init(), OS_SUCCESS);

    /* two users of type A (one on a deleted record) and a user of type B on a deleted record */
    UT_SetRecordDeleted(0, true);
    UT_SetRecordDeleted(1, false);
    UT_SetRecordDeleted(2, true);
    UT_Enter(&reserved_a, UT_TASK_1, UT_TYPE_A, 0);
    UT_Enter(&live_a, UT_TASK_2, UT_TYPE_A, 1);
    UT_Enter(&reserved_b, UT_TASK_3, UT_TYPE_B, 2);

    UT_Stub_IdmapCurrentTask = UT_TASK_1;
    OS_Lock_Global_Impl(UT_TYPE_A);

    /* the lock is given up while it waits (another task finds it free then), and taken again */
    UT_DelayType   = UT_TYPE_A;
    UT_DelayMode   = UT_DELAY_PROBE;
    UT_DelayCalls  = 0;
    UT_ProbeWaited = 99;

    /* only the users of this type on a deleted record are woken, it waits 10ms */
    OS_WaitForStateChange_Impl(UT_TYPE_A, 1);
    UtAssert_True(UT_Stub_IdmapLastDelayTicks == UT_Stub_IdmapMsToTicksResult, "the delay uses the converted time");
    UT_DelayMode = UT_DELAY_NOTHING;
    UtAssert_UINT32_EQ(UT_ProbeWaited, 0);
    UtAssert_True(UT_LockIsHeld(UT_TYPE_A, UT_TASK_1), "the lock is taken again");
    UtAssert_UINT32_EQ(UT_Stub_IdmapAbortedCount, 1);
    UtAssert_True(UT_Aborted(UT_TASK_1), "the user of the deleted object is woken");
    UtAssert_True(!UT_Aborted(UT_TASK_2), "a user of an object that is not being deleted is left alone");
    UtAssert_True(!UT_Aborted(UT_TASK_3), "a user of another object type is left alone");
    UtAssert_UINT32_EQ(UT_Stub_IdmapLastMs, 10);

    /* the wait grows with the attempts, squared, up to the tenth */
    OS_WaitForStateChange_Impl(UT_TYPE_A, 3);
    UtAssert_UINT32_EQ(UT_Stub_IdmapLastMs, 90);
    OS_WaitForStateChange_Impl(UT_TYPE_A, 10);
    UtAssert_UINT32_EQ(UT_Stub_IdmapLastMs, 1000);

    /* then it polls once a second */
    OS_WaitForStateChange_Impl(UT_TYPE_A, 11);
    UtAssert_UINT32_EQ(UT_Stub_IdmapLastMs, 1000);
    OS_WaitForStateChange_Impl(UT_TYPE_A, 50);
    UtAssert_UINT32_EQ(UT_Stub_IdmapLastMs, 1000);

    /* the other type's turn: now its user is the one woken */
    UT_Stub_IdmapAbortedCount = 0;
    OS_WaitForStateChange_Impl(UT_TYPE_B, 1);
    UtAssert_UINT32_EQ(UT_Stub_IdmapAbortedCount, 1);
    UtAssert_True(UT_Aborted(UT_TASK_3), "the user of type B is woken");

    /* a user whose refcount was already given back (it is leaving) is not woken, whatever its record reads */
    UT_Enter(&released_a, UT_TASK_2, UT_TYPE_B, 2);
    OS_ObjectIdRelease(&released_a.token);
    UT_Stub_IdmapAbortedCount = 0;
    OS_WaitForStateChange_Impl(UT_TYPE_B, 1);
    UtAssert_UINT32_EQ(UT_Stub_IdmapAbortedCount, 1);
    UtAssert_True(UT_Aborted(UT_TASK_3), "the user that still holds a refcount is woken");
    UtAssert_True(!UT_Aborted(UT_TASK_2), "the user that gave its refcount back is not");

    OS_Unlock_Global_Impl(UT_TYPE_A);
    (void)OS_FreeRTOS_WaitLeave(&released_a);
    (void)OS_FreeRTOS_WaitLeave(&reserved_a);
    (void)OS_FreeRTOS_WaitLeave(&live_a);
    (void)OS_FreeRTOS_WaitLeave(&reserved_b);
}

void Test_OS_FreeRTOS_WaitEnter(void)
{
    OS_impl_wait_user_t user;
    OS_impl_wait_user_t failed;
    OS_object_token_t   token = UT_MakeToken(UT_TYPE_A, 1);

    UtAssert_INT32_EQ(OS_FreeRTOS_TableMutex_Init(), OS_SUCCESS);
    UT_SetRecordDeleted(1, true);
    memset(&user, 0, sizeof(user));
    memset(&failed, 0, sizeof(failed));

    /* a live object -> a refcount lock on it is taken, and the caller is listed under its task */
    UT_Stub_IdmapCurrentTask = UT_TASK_2;
    UtAssert_INT32_EQ(OS_FreeRTOS_WaitEnter(&token, &user), OS_SUCCESS);
    UtAssert_STUB_COUNT(OS_FreeRTOS_Enter, 1);
    UtAssert_STUB_COUNT(OS_ObjectIdGetById, 1);
    UtAssert_True(user.task == UT_TASK_2, "the user is listed under the calling task");
    UtAssert_UINT32_EQ(user.token.lock_mode, OS_LOCK_MODE_REFCOUNT);
    UtAssert_UINT32_EQ(user.token.obj_type, UT_TYPE_A);
    UtAssert_UINT32_EQ(user.token.obj_idx, 1);

    /* it is in the list: a wait for a state change on its (deleted) object wakes it */
    OS_WaitForStateChange_Impl(UT_TYPE_A, 1);
    UtAssert_True(UT_Aborted(UT_TASK_2), "the listed user is woken");

    /* an object that is gone -> the lookup's status, and nothing listed (the wait is still a wait entering the port) */
    UT_SetDefaultReturnValue(UT_KEY(OS_ObjectIdGetById), OS_ERR_INVALID_ID);
    UT_Stub_IdmapCurrentTask  = UT_TASK_3;
    UT_Stub_IdmapAbortedCount = 0;
    UtAssert_INT32_EQ(OS_FreeRTOS_WaitEnter(&token, &failed), OS_ERR_INVALID_ID);
    UtAssert_STUB_COUNT(OS_FreeRTOS_Enter, 2);
    UtAssert_NULL(failed.task);
    OS_WaitForStateChange_Impl(UT_TYPE_A, 1);
    UtAssert_UINT32_EQ(UT_Stub_IdmapAbortedCount, 1);
    UtAssert_True(!UT_Aborted(UT_TASK_3), "a refused user is not listed");

    UT_ClearDefaultReturnValue(UT_KEY(OS_ObjectIdGetById));
    (void)OS_FreeRTOS_WaitLeave(&user);
}

void Test_OS_FreeRTOS_WaitLeave(void)
{
    OS_impl_wait_user_t first;
    OS_impl_wait_user_t second;
    OS_impl_wait_user_t third;
    uint32              criticals;

    UtAssert_INT32_EQ(OS_FreeRTOS_TableMutex_Init(), OS_SUCCESS);

    /* the list is third -> second -> first, all on deleted records, each woken by a wait */
    UT_SetRecordDeleted(0, true);
    UT_SetRecordDeleted(1, true);
    UT_SetRecordDeleted(2, false);
    UT_Enter(&first, UT_TASK_1, UT_TYPE_A, 0);
    UT_Enter(&second, UT_TASK_2, UT_TYPE_A, 1);
    UT_Enter(&third, UT_TASK_3, UT_TYPE_A, 2);

    /* the middle one -> the refcount is given back, and the object was being deleted */
    criticals = UT_GetStubCount(UT_KEY(vTaskEnterCritical));
    UtAssert_True(OS_FreeRTOS_WaitLeave(&second), "leaving a deleted object reports it");
    UtAssert_STUB_COUNT(OS_ObjectIdRelease, 1);

    /* the refcount went back before the node was unlisted, so a task deleted in between leaves nothing to leak */
    UtAssert_UINT32_EQ(UT_ReleaseCriticals, criticals);
    UtAssert_UINT32_EQ(UT_GetStubCount(UT_KEY(vTaskEnterCritical)), criticals + 1);
    UtAssert_UINT32_EQ(second.token.lock_mode, OS_LOCK_MODE_NONE);
    UT_Stub_IdmapAbortedCount = 0;
    OS_WaitForStateChange_Impl(UT_TYPE_A, 1);
    UtAssert_UINT32_EQ(UT_Stub_IdmapAbortedCount, 1);
    UtAssert_True(UT_Aborted(UT_TASK_1), "the first user is still listed");

    /* the newest one, at the head; its object was not being deleted */
    UtAssert_True(!OS_FreeRTOS_WaitLeave(&third), "leaving a live object reports nothing");
    UtAssert_STUB_COUNT(OS_ObjectIdRelease, 2);

    /* the oldest, the last one */
    UtAssert_True(OS_FreeRTOS_WaitLeave(&first), "leaving the last user");
    UtAssert_STUB_COUNT(OS_ObjectIdRelease, 3);
    UT_Stub_IdmapAbortedCount = 0;
    OS_WaitForStateChange_Impl(UT_TYPE_A, 1);
    UtAssert_UINT32_EQ(UT_Stub_IdmapAbortedCount, 0);
}

void Test_TaskDied(void)
{
    OS_impl_wait_user_t t1_a;
    OS_impl_wait_user_t t2_a;
    OS_impl_wait_user_t t1_b;

    UtAssert_INT32_EQ(OS_FreeRTOS_TableMutex_Init(), OS_SUCCESS);
    UT_SetRecordDeleted(0, true);
    UT_SetRecordDeleted(1, true);
    UT_SetRecordDeleted(2, true);

    /* a task that holds nothing -> nothing changes */
    UT_Stub_IdmapDiedHook(UT_TASK_3);
    UtAssert_STUB_COUNT(OS_ObjectIdRelease, 0);
    UtAssert_True(!UT_LockIsHeld(UT_TYPE_A, NULL), "a free lock stays free");

    /* a lock held by someone else stays held */
    UT_Stub_IdmapCurrentTask = UT_TASK_2;
    OS_Lock_Global_Impl(UT_TYPE_A);
    UT_Stub_IdmapDiedHook(UT_TASK_1);
    UtAssert_True(UT_LockIsHeld(UT_TYPE_A, UT_TASK_2), "another task's lock stays held");

    /* locks held by the dead task are released, and only those */
    UT_Stub_IdmapCurrentTask = UT_TASK_1;
    OS_Lock_Global_Impl(UT_TYPE_B);
    OS_Lock_Global_Impl(OS_OBJECT_TYPE_OS_MUTEX);
    UT_Stub_IdmapDiedHook(UT_TASK_1);
    UtAssert_True(!UT_LockIsHeld(UT_TYPE_B, NULL), "the dead task's lock is released");
    UtAssert_True(!UT_LockIsHeld(OS_OBJECT_TYPE_OS_MUTEX, NULL), "all of the dead task's locks are released");
    UtAssert_True(UT_LockIsHeld(UT_TYPE_A, UT_TASK_2), "the other task still holds its lock");
    OS_Unlock_Global_Impl(UT_TYPE_A);

    /* one node of the dead task -> its refcount is given back, once, and it is no longer listed */
    UT_Enter(&t1_a, UT_TASK_1, UT_TYPE_A, 0);
    UT_Stub_IdmapDiedHook(UT_TASK_1);
    UtAssert_STUB_COUNT(OS_ObjectIdRelease, 1);
    UT_Stub_IdmapAbortedCount = 0;
    OS_WaitForStateChange_Impl(UT_TYPE_A, 1);
    UtAssert_UINT32_EQ(UT_Stub_IdmapAbortedCount, 0);
    UT_Stub_IdmapDiedHook(UT_TASK_1);
    UtAssert_STUB_COUNT(OS_ObjectIdRelease, 1);

    /* the list is t1_b -> t2_a -> t1_a: the dead task's nodes are given back, another task's is not */
    UT_Enter(&t1_a, UT_TASK_1, UT_TYPE_A, 0);
    UT_Enter(&t2_a, UT_TASK_2, UT_TYPE_A, 1);
    UT_Enter(&t1_b, UT_TASK_1, UT_TYPE_B, 2);
    UT_Stub_IdmapDiedHook(UT_TASK_1);
    UtAssert_STUB_COUNT(OS_ObjectIdRelease, 3);

    UT_Stub_IdmapAbortedCount = 0;
    OS_WaitForStateChange_Impl(UT_TYPE_A, 1);
    OS_WaitForStateChange_Impl(UT_TYPE_B, 1);
    UtAssert_UINT32_EQ(UT_Stub_IdmapAbortedCount, 1);
    UtAssert_True(UT_Aborted(UT_TASK_2), "the other task's user is still listed");

    (void)OS_FreeRTOS_WaitLeave(&t2_a);
}

/*
 * Setup function prior to every test
 */
void Osapi_Test_Setup(void)
{
    UT_ResetState(0);
    memset(UT_Records, 0, sizeof(UT_Records));
    UT_SetHandlerFunction(UT_KEY(OS_ObjectIdGlobalFromToken), UT_RecordHandler, NULL);

    UT_SetHookFunction(UT_KEY(vTaskDelay), UT_DelayHook, NULL);
    UT_SetHookFunction(UT_KEY(OS_ObjectIdRelease), UT_ReleaseHook, NULL);
    UT_DelayMode        = UT_DELAY_NOTHING;
    UT_DelayCalls       = 0;
    UT_ProbeWaited      = 0;
    UT_ReleaseCriticals = 0;

    UT_Stub_IdmapCurrentTask    = (TaskHandle_t)0x7000;
    UT_Stub_IdmapAbortedCount   = 0;
    UT_Stub_IdmapLastDelayTicks = 0;
    UT_Stub_IdmapDiedHook       = NULL;
    UT_Stub_IdmapLastMs         = 0;
}

void Osapi_Test_Teardown(void)
{
}

#define ADD_TEST(test) UtTest_Add((Test_##test), Osapi_Test_Setup, Osapi_Test_Teardown, #test)

void UtTest_Setup(void)
{
    ADD_TEST(OS_Lock_Global_Impl_NotCreated);
    ADD_TEST(OS_FreeRTOS_TableMutex_Init);
    ADD_TEST(OS_Lock_Global_Impl);
    ADD_TEST(OS_WaitForStateChange_Impl);
    ADD_TEST(OS_FreeRTOS_WaitEnter);
    ADD_TEST(OS_FreeRTOS_WaitLeave);
    ADD_TEST(TaskDied);
}
