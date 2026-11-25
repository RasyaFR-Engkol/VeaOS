/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : thread.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : RTL Thread
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <rtl.h>
#include <vz_wrappers.h>

/* Revision History ------------------------------------------------------
 * DATE       : 08-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file thread.c
 * --------------------------------------------------------------------- */

#if defined (BUILDING_VEAKRNL) || defined(BUILD_USER)

VEASTATUS
VEAPI
RtlpCreateUserStack(
    IN HANDLE ProcessHandle,
    IN SIZE_T StackReserve OPTIONAL,
    IN SIZE_T StackCommit OPTIONAL,
    IN ULONG StackZeroBits OPTIONAL,
    OUT PINITIAL_TEB InitialTeb
)
{
    VEASTATUS Status;
    SYSTEM_BASIC_INFORMATION SystemBasicInfo;
    ULONG_PTR Stack = 0;
    BOOLEAN UseGuard = FALSE;
    ULONG OldProtect = 0;
    SIZE_T MinimumStackCommit = 0;
    SIZE_T GuardPageSize = 0;

    Status = VzQuerySystemInformation(
        SystemBasicInformation,
        &SystemBasicInfo,
        sizeof(SYSTEM_BASIC_INFORMATION),
        NULL
    );
    
    if (!VEA_SUCCESS(Status)) return Status;

    if (ProcessHandle == NtCurrentProcess())
    {
        if (StackReserve == 0) StackReserve = 1024 * 1024;
        if (StackCommit == 0)  StackCommit  = SystemBasicInfo.PageSize;
    }
    else
    {
        if (StackReserve == 0) StackReserve = SystemBasicInfo.AllocationGranularity;
        if (StackCommit == 0)  StackCommit  = SystemBasicInfo.PageSize;
    }

    if (StackCommit >= StackReserve)
    {
        StackReserve = ROUND_UP(StackCommit, 1024 * 1024);
    }

    StackCommit  = ROUND_UP(StackCommit, SystemBasicInfo.PageSize);
    StackReserve = ROUND_UP(StackReserve, SystemBasicInfo.AllocationGranularity);

    Stack = 0;
    Status = VzAllocateVirtualMemory(
        ProcessHandle,
        (PVOID*)&Stack,
        StackZeroBits,
        &StackReserve,
        MEM_RESERVE,
        PAGE_READWRITE
    );
    if (!VEA_SUCCESS(Status)) return Status;

    InitialTeb->AllocatedStackBase = (PVOID)Stack;
    InitialTeb->StackBase          = (PVOID)(Stack + StackReserve);
    InitialTeb->PreviousStackBase  = NULL;
    InitialTeb->PreviousStackLimit = NULL;

    Stack += StackReserve - StackCommit;

    if (StackReserve >= StackCommit + SystemBasicInfo.PageSize)
    {
        Stack -= SystemBasicInfo.PageSize;
        StackCommit += SystemBasicInfo.PageSize;
        UseGuard = TRUE;
    }
    else
    {
        UseGuard = FALSE;
    }

    Status = VzAllocateVirtualMemory(
        ProcessHandle,
        (PVOID*)&Stack,
        0,
        &StackCommit,
        MEM_COMMIT,
        PAGE_READWRITE
    );
    if (!VEA_SUCCESS(Status))
    {
        GuardPageSize = 0;
        VzFreeVirtualMemory(ProcessHandle, &InitialTeb->AllocatedStackBase, &GuardPageSize, MEM_RELEASE);
        return Status;
    }

    InitialTeb->StackLimit = (PVOID)Stack;

    if (UseGuard)
    {
        // TODO:
        /* GuardPageSize = SystemBasicInfo.PageSize;
        Status = VzProtectVirtualMemory(
            ProcessHandle,
            (PVOID*)&Stack,
            &GuardPageSize,
            PAGE_GUARD | PAGE_READWRITE,
            &OldProtect
        );
        if (!VEA_SUCCESS(Status)) return Status;

        // Geser Stack Limit asli di atas Guard Page
        InitialTeb->StackLimit = (PVOID)((ULONG_PTR)InitialTeb->StackLimit + GuardPageSize); */
    }

    return STATUS_SUCCESS;
}

VOID
VEAPI
RtlInitializeContext(
    IN HANDLE ProcessHandle,
    OUT PKTRAP_FRAME ThreadContext,
    IN PVOID ThreadStartParam OPTIONAL,
    IN PTHREAD_START_ROUTINE ThreadStartAddress,
    IN PVOID StackBase
)
{
    ThreadContext->Ebp = 0;
    ThreadContext->Eax = 0;
    ThreadContext->Ebx = 1;
    ThreadContext->Ecx = 2;
    ThreadContext->Edx = 3;
    ThreadContext->Esi = 4;
    ThreadContext->Edi = 5;

    ThreadContext->SegGs = 0;
    ThreadContext->SegFs = KGDT32_R3_TEB;
    ThreadContext->SegEs = KGDT32_R3_DATA;
    ThreadContext->SegDs = KGDT32_R3_DATA;
    ThreadContext->HardwareSegSs = KGDT32_R3_DATA;
    ThreadContext->SegCs = KGDT32_R3_CODE;

    ThreadContext->EFlags = EFLAGS_INTERRUPT_MASK;

    ThreadContext->Eip = (ULONG)ThreadStartAddress;
    ThreadContext->HardwareEsp = (ULONG)StackBase;

    ThreadContext->HardwareEsp -= sizeof(PVOID);
    VzWriteVirtualMemory(
        ProcessHandle,
        (PVOID)ThreadContext->HardwareEsp,
        (PVOID)&ThreadStartParam,
        sizeof(PVOID),
        NULL
    );

    ThreadContext->HardwareEsp -= sizeof(PVOID);
}

VOID
VEAPI
RtlpFreeUserStack(
    IN HANDLE ProcessHandle,
    IN PINITIAL_TEB InitialTeb
)
{
    SIZE_T Dummy = 0;

    VzFreeVirtualMemory(
        ProcessHandle,
        &InitialTeb->AllocatedStackBase,
        &Dummy,
        MEM_RELEASE
    );

    RtlZeroMemory(InitialTeb, sizeof(*InitialTeb));
}

VEASTATUS
VEAPI
RtlCreateUserThread(
    IN HANDLE ProcessHandle,
    IN PVOID SecurityDesciptor,
    IN BOOLEAN CreateSuspended,
    IN ULONG StackZeroBits OPTIONAL,
    IN SIZE_T StackReserve OPTIONAL,
    IN SIZE_T StackCommit OPTIONAL,
    IN PTHREAD_START_ROUTINE StartAddress,
    IN PVOID Parameter OPTIONAL,
    OUT HANDLE *ThreadHandle OPTIONAL,
    OUT PCLIENT_ID ClientId OPTIONAL
)
{
    VEASTATUS Status;
    HANDLE Handle;
    CLIENT_ID ThreadCid;
    INITIAL_TEB InitialTeb;
    OBJECT_ATTRIBUTES ObjAttr;
    KTRAP_FRAME ContextAsTf;

    Status = RtlpCreateUserStack(
        ProcessHandle,
        StackReserve,
        StackCommit,
        StackZeroBits,
        &InitialTeb
    );
    if (!VEA_SUCCESS(Status)) return Status;

    RtlInitializeContext(
        ProcessHandle,
        &ContextAsTf,
        Parameter,
        StartAddress,
        InitialTeb.StackBase
    );

    InitializeObjectAttributes(
        &ObjAttr,
        NULL,
        0,
        NULL,
        SecurityDesciptor
    );

    Status = VzCreateThread(
        &Handle,
        THREAD_ALL_ACCESS,
        &ObjAttr,
        ProcessHandle,
        &ThreadCid,
        &ContextAsTf,
        &InitialTeb,
        CreateSuspended
    );

    if (!VEA_SUCCESS(Status))
    {
        RtlpFreeUserStack(ProcessHandle, &InitialTeb);
    }
    else
    {
        if (ThreadHandle)
            *ThreadHandle = Handle;
        else
            VzClose(Handle);
        if (ClientId) *ClientId = ThreadCid;
    }

    return Status;
}

#endif
