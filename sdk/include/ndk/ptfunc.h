#pragma once

#include "hct.h"
#include "internal/veakrnl.h"
#include "procbind.h"
#include "pttypes.h"
#include "mmtype.h"

VEASTATUS
VEAPI
PtCreateSystemThread(
    OUT PHANDLE ThreadHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL,
    IN HANDLE ProcessHandle OPTIONAL,
    OUT PCLIENT_ID ClientId OPTIONAL,
    IN PKSTART_ROUTINE StartRoutine,
    IN PVOID StartContext OPTIONAL
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
    PKSTART_ROUTINE StartAddress,
    PVOID Argument,
    POOL_TYPE PoolType
);

VOID
VEAPI
PtInitializeRunQueues(VOID);

FORCEINLINE
PKTHREAD 
PtGetCurrentThread(VOID)
{
    return (PKTHREAD)KeGetCurrentPrcb()->CurrentThread;
}

FORCEINLINE
PKPROCESS 
PtGetCurrentProcess(VOID)
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

VEASTATUS
VEAPI
PtLocateSystemSo(VOID);

VEASTATUS
VEAPI
PtpCreateThread(
    OUT PHANDLE ThreadHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjAttr OPTIONAL,
    IN HANDLE ProcessHandle,
    IN PEPROCESS TargetProcess,
    OUT PCLIENT_ID ClientId,
    IN PKTRAP_FRAME ThreadContext, // IN NT, CONTEXT not KTRAP_FRAME
    IN PINITIAL_TEB InitialTeb,
    IN BOOLEAN CreateSuspended,
    IN PKSTART_ROUTINE StartRoutine OPTIONAL,
    IN PVOID StartContext OPTIONAL
);

VOID
VEAPI
PiInitializeThread(
    IN PKTHREAD Thread,
    IN PVOID KernelStack,
    IN PVOID InitialStack,
    IN PVOID StackLimit,
    IN CHAR Priority
);

VEASTATUS
VEAPI
PtpMapSystemSo(
    IN PEPROCESS Process,
    IN PVOID *DllBase,
    IN BOOLEAN UseLargePages
);

VEASTATUS
VEAPI
PtpCreateProcess(
    OUT PHANDLE ProcessHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL,
    IN HANDLE ParentProcess OPTIONAL,
    IN ULONG Flags,
    IN HANDLE SectionHandle OPTIONAL,
    IN HANDLE DebugPort OPTIONAL,
    IN HANDLE ExceptionPort OPTIONAL
);

VEASTATUS
VEAPI
VeaCreateProcess(
    OUT PHANDLE ProcessHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL,
    IN HANDLE ParentProcess OPTIONAL,
    IN ULONG Flags,
    IN HANDLE SectionHandle OPTIONAL,
    IN HANDLE DebugPort OPTIONAL,
    IN HANDLE ExceptionPort OPTIONAL
);

VEASTATUS
VEAPI
VeaQueryInformationProcess(
    IN HANDLE ProcessHandle,
    IN PROCESSINFOCLASS ProcessInformationClass,
    OUT PVOID ProcessInformation,
    IN ULONG ProcessInformationLength,
    OUT PULONG ReturnLength OPTIONAL
);

VEASTATUS
VEAPI
VeaCreateThread(
    OUT PHANDLE ThreadHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL,
    IN HANDLE ProcessHandle,
    OUT PCLIENT_ID ClientId OPTIONAL,
    IN PKTRAP_FRAME ThreadContext, // Direct KTRAP_FRAME
    IN PINITIAL_TEB InitialTeb,
    IN BOOLEAN CreateSuspended
);

ULONG
VEAPI
PtResumeThread(
    IN PKTHREAD Thread,
    OUT PULONG PreviousSuspendCount OPTIONAL
);

VEASTATUS
VEAPI
VeaResumeThread(
    IN HANDLE ThreadHandle,
    OUT PULONG PreviousSuspendCount OPTIONAL
);

VEASTATUS
VEAPI
PtpLookupSystemSoEntryPoint(
    IN PCSTR SymbolName,
    OUT PVOID *EntryPoint
);

VEASTATUS
VEAPI
VeaYieldExecution(VOID);

LONG_PTR
VEAFAST
PtSwapThread(
    IN PKTHREAD Thread,
    IN PKPRCB Prcb
);

VOID
VEAPI
PtEstablishVeapiCallouts(
    IN PVEAPI_CALLOUTS_FPNS CalloutData
);

VEASTATUS
VEAPI
PtConvertToGuiThread(VOID);

PVOID
VEAPI
PtGetCurrentThreadVeapiThread(VOID);

VOID
VEAPI
PtpCatchCriticalBreak(
    IN PVOID Object,
    IN BOOLEAN IsProcess,
    IN VEASTATUS ExitStatus
);

VEASTATUS
VEAPI
PtpTerminateThread(
    IN PETHREAD Thread,
    IN VEASTATUS ExitStatus,
    IN BOOLEAN Direct
);

VEASTATUS
VEAPI
VeaTerminateProcess(
    IN HANDLE ProcessHandle OPTIONAL,
    IN VEASTATUS ExitStatus
);

VEASTATUS
VEAPI
VeaTerminateThread(
    IN HANDLE ThreadHandle,
    IN VEASTATUS ExitStatus
);

/* EOF */