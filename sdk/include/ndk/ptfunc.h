#pragma once

#include "hct.h"
#include "procbind.h"
#include "pttypes.h"
#include "mmtype.h"

PETHREAD
VEAPI
PtCreateSystemThread(
    PEPROCESS ParentProcess,
    PVOID StartAddress,
    PVOID Argument,
    POOL_TYPE PoolType
);

PEPROCESS
VEAPI
PtCreateSystemProcess(
    VOID
);

VOID
VEAPI
PtInitializeHandBuildThread(
    IN PKTHREAD InitialThread, 
    IN PKPROCESS Process,
    IN PVOID StackBase, 
    IN ULONG StackSize, 
    IN PVOID StartAddress
);

VOID
VEAPI
PtIdleLoop(VOID);

PETHREAD
VEAPI
PtCreateSystemThreadForMainProcess(
    PVOID StartAddress,
    PVOID Argument,
    POOL_TYPE PoolType
);

VOID
VEAPI
PtInitializeRunQueues(VOID);

static inline PKTHREAD PtGetCurrentThread(VOID)
{
    return (PKTHREAD)KeGetCurrentPrcb()->CurrentThread;
}

static inline PKPROCESS PtGetCurrentProcess(VOID)
{
    PKTHREAD CurrentThread = PtGetCurrentThread();
    return CurrentThread ? CurrentThread->ApcState.Process : NULL;
}

static inline VOID PtSetCurrentThread(PVOID Thread)
{
    KeGetCurrentPrcb()->CurrentThread = (struct _KTHREAD *)Thread;
}

BOOLEAN
VEAPI
PtInitSystem(VOID);

VOID
VEAPI
PtEnqueueThreadCfs(
    PKRUNQUEUE RunQueue, 
    PKTHREAD Thread
);

VOID
VEAPI
PtSchedulerTick(ULONGLONG DeltaTimeNs);