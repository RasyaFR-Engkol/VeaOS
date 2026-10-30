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

VEASTATUS
VEAPI
PtCreateSystemProcess(
    OUT PHANDLE ProcessHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjAttr
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

VOID
VEAPI
PtYieldThread(VOID);

BOOLEAN
VEAPI
PtWakeThread(
    PKTHREAD Thread
);

BOOLEAN
VEAPI
PtSetThreadPriority(
    IN PKTHREAD Thread,
    IN CHAR Priority
);

VOID
VEAPI
PtDetachProcess(VOID);

VOID
VEAPI
PtAttachProcess(
    IN PKPROCESS Process
);

BOOLEAN
VEAPI
PtWakeThreadEx(
    PKTHREAD Thread,
    VEASTATUS WakeStatus
);

ULONG_PTR
VEAFAST
PtpSaveAndDisableInterrupts(VOID)
;

VOID
VEAFAST
PtpRestoreInterrupts(ULONG_PTR Flags);

/* EOF */