/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : basesrv.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : CSR Base Server
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <csr.h>

/* Revision History ------------------------------------------------------
 * DATE       : 21-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file basesrv.c
 * --------------------------------------------------------------------- */

/* PRIVATE ****************************************************************/

#define CSR_PAYLOAD_MIN_SIZE(Type) \
    (sizeof(ULONG) + sizeof(VEASTATUS) + sizeof(Type))

static VEASTATUS
BasepInsertThread(
    IN PCSR_PROCESS Process,
    IN CLIENT_ID ClientId,
    IN HANDLE ThreadHandle
)
{
    PCSR_THREAD Thread;
    PVOID Heap = RtlGetProcessHeap();

    if (!Process)
    {
        return STATUS_INVALID_PARAMETER;
    }

    Thread = (PCSR_THREAD)RtlAllocateHeap(Heap, HEAP_ZERO_MEMORY, sizeof(CSR_THREAD));
    if (!Thread)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Thread->Process      = Process;
    Thread->ClientId     = ClientId;
    Thread->ThreadHandle = ThreadHandle;

    InsertTailList(&Process->ThreadList, &Thread->Link);
    return STATUS_SUCCESS;
}

static VEASTATUS
VEAPI
BaseSrvCreateProcess(
    IN PCSR_PROCESS Process,
    IN OUT PCSR_KPC_MESSAGE Request,
    OUT PBOOLEAN Reply
)
{
    PBASE_CREATE_PROCESS_MSG Msg = (PBASE_CREATE_PROCESS_MSG)Request->Data;
    PCSR_PROCESS NewProc;
    HANDLE ParentProcHandle = NULL;
    HANDLE DupProcHandle = NULL;
    HANDLE DupThreadHandle = NULL;
    BOOLEAN OpenedParent = FALSE;
    VEASTATUS Status;

    *Reply = TRUE;

    if (Request->Header.DataLength < CSR_PAYLOAD_MIN_SIZE(BASE_CREATE_PROCESS_MSG))
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (!Process)
    {
        return STATUS_INVALID_PARAMETER;
    }

    ParentProcHandle = Process->ProcessHandle;
    if (!ParentProcHandle)
    {
        OBJECT_ATTRIBUTES ObjAttr;
        CLIENT_ID ParentCid;

        InitializeObjectAttributes(&ObjAttr, NULL, 0, NULL, NULL);
        ParentCid.UniqueProcess = Process->ProcessId;
        ParentCid.UniqueThread  = NULL;

        Status = VzOpenProcess(
            &ParentProcHandle,
            PROCESS_DUP_HANDLE,
            &ObjAttr,
            &ParentCid
        );
        if (!VEA_SUCCESS(Status))
        {
            KdPrintf("[BASESRV] VzOpenProcess parent PID=0x%X gagal: 0x%08X\n\r",
                     Process->ProcessId, Status);
            return Status;
        }
        OpenedParent = TRUE;
    }

    if (Msg->ProcessHandle)
    {
        Status = VzDuplicateObject(
            ParentProcHandle,
            Msg->ProcessHandle,
            NtCurrentProcess(),
            &DupProcHandle,
            0,
            0,
            DUPLICATE_SAME_ACCESS
        );
        if (!VEA_SUCCESS(Status))
        {
            KdPrintf("[BASESRV] Duplicate ProcessHandle gagal: 0x%08X\n\r", Status);
            if (OpenedParent) VzClose(ParentProcHandle);
            return Status;
        }
    }

    if (Msg->ThreadHandle)
    {
        Status = VzDuplicateObject(
            ParentProcHandle,
            Msg->ThreadHandle,
            NtCurrentProcess(),
            &DupThreadHandle,
            0,
            0,
            DUPLICATE_SAME_ACCESS
        );
        if (!VEA_SUCCESS(Status))
        {
            KdPrintf("[BASESRV] Duplicate ThreadHandle gagal: 0x%08X\n\r", Status);
            if (DupProcHandle) VzClose(DupProcHandle);
            if (OpenedParent)  VzClose(ParentProcHandle);
            return Status;
        }
    }

    if (OpenedParent)
    {
        VzClose(ParentProcHandle);
    }

    NewProc = CsrLocateProcessById(Msg->ClientId.UniqueProcess);
    if (!NewProc)
    {
        NewProc = CsrpAllocateProcess(Msg->ClientId.UniqueProcess, NULL);
        if (!NewProc)
        {
            if (DupThreadHandle) VzClose(DupThreadHandle);
            if (DupProcHandle)   VzClose(DupProcHandle);
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    NewProc->ParentProcessId = Process->ProcessId;
    NewProc->ProcessHandle   = DupProcHandle;
    NewProc->CreationFlags   = Msg->CreationFlags;

    Status = BasepInsertThread(NewProc, Msg->ClientId, DupThreadHandle);
    if (!VEA_SUCCESS(Status))
    {
        if (DupThreadHandle) VzClose(DupThreadHandle);
        return Status;
    }

    KdPrintf("[BASESRV] CreateProcess: Parent=0x%X -> Child=0x%X (ProcH=%p, ThreadH=%p, Flags=0x%X)\n\r",
             NewProc->ParentProcessId,
             Msg->ClientId.UniqueProcess,
             DupProcHandle,
             DupThreadHandle,
             Msg->CreationFlags);

    return STATUS_SUCCESS;
}

static VEASTATUS
VEAPI
BaseSrvCreateThread(
    IN PCSR_PROCESS Process,
    IN OUT PCSR_KPC_MESSAGE Request,
    OUT PBOOLEAN Reply
)
{
    PBASE_CREATE_THREAD_MSG Msg = (PBASE_CREATE_THREAD_MSG)Request->Data;
    PCSR_PROCESS TargetProc;
    PCSR_PROCESS CallerProc;
    HANDLE CallerProcHandle = NULL;
    HANDLE DupThreadHandle = NULL;
    BOOLEAN OpenedCaller = FALSE;
    VEASTATUS Status;

    *Reply = TRUE;

    if (Request->Header.DataLength < CSR_PAYLOAD_MIN_SIZE(BASE_CREATE_THREAD_MSG))
    {
        return STATUS_INVALID_PARAMETER;
    }

    TargetProc = CsrLocateProcessById(Msg->ClientId.UniqueProcess);
    if (!TargetProc)
    {
        TargetProc = Process;
    }
    if (!TargetProc)
    {
        return STATUS_NOT_FOUND;
    }

    CallerProc = Process ? Process : TargetProc;

    if (Msg->ThreadHandle)
    {
        CallerProcHandle = CallerProc->ProcessHandle;
        if (!CallerProcHandle)
        {
            OBJECT_ATTRIBUTES ObjAttr;
            CLIENT_ID CallerCid;

            InitializeObjectAttributes(&ObjAttr, NULL, 0, NULL, NULL);
            CallerCid.UniqueProcess = CallerProc->ProcessId;
            CallerCid.UniqueThread  = NULL;

            Status = VzOpenProcess(
                &CallerProcHandle,
                PROCESS_DUP_HANDLE,
                &ObjAttr,
                &CallerCid
            );
            if (!VEA_SUCCESS(Status))
            {
                KdPrintf("[BASESRV] VzOpenProcess caller PID=0x%X gagal: 0x%08X\n\r",
                         CallerProc->ProcessId, Status);
                return Status;
            }
            OpenedCaller = TRUE;
        }

        Status = VzDuplicateObject(
            CallerProcHandle,
            Msg->ThreadHandle,
            NtCurrentProcess(),
            &DupThreadHandle,
            0,
            0,
            DUPLICATE_SAME_ACCESS
        );

        if (OpenedCaller)
        {
            VzClose(CallerProcHandle);
        }

        if (!VEA_SUCCESS(Status))
        {
            KdPrintf("[BASESRV] Duplicate ThreadHandle gagal: 0x%08X\n\r", Status);
            return Status;
        }
    }

    Status = BasepInsertThread(TargetProc, Msg->ClientId, DupThreadHandle);
    if (!VEA_SUCCESS(Status))
    {
        if (DupThreadHandle)
        {
            VzClose(DupThreadHandle);
        }
        return Status;
    }

    KdPrintf("[BASESRV] CreateThread: PID=0x%X, TID=0x%X (ThreadH=%p)\n\r",
             TargetProc->ProcessId, Msg->ClientId.UniqueThread, DupThreadHandle);

    return STATUS_SUCCESS;
}

static VEASTATUS
VEAPI
BaseSrvExitProcess(
    IN PCSR_PROCESS Process,
    IN OUT PCSR_KPC_MESSAGE Request,
    OUT PBOOLEAN Reply
)
{
    PBASE_EXIT_PROCESS_MSG Msg = (PBASE_EXIT_PROCESS_MSG)Request->Data;

    *Reply = TRUE;

    if (!Process)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (Request->Header.DataLength >= CSR_PAYLOAD_MIN_SIZE(BASE_EXIT_PROCESS_MSG))
    {
        Process->ExitCode = Msg->ExitCode;
    }

    Process->Flags |= CSR_PROCESS_TERMINATED;

    KdPrintf("[BASESRV] ExitProcess: PID=0x%X keluar normal (ExitCode=0x%X)\n\r",
             Process->ProcessId, Process->ExitCode);

    return STATUS_SUCCESS;
}

static VOID
VEAPI
BaseSrvDisconnect(
    IN PCSR_PROCESS Process
)
{
    PVOID Heap = RtlGetProcessHeap();

    if (!Process)
    {
        return;
    }

    if (!(Process->Flags & CSR_PROCESS_TERMINATED))
    {
        KdPrintf("[BASESRV] Peringatan: PID=0x%X putus tanpa ExitProcess (crash/killed)\n\r",
                 Process->ProcessId);
    }

    while (!IsListEmpty(&Process->ThreadList))
    {
        PLIST_ENTRY Entry = RemoveHeadList(&Process->ThreadList);
        PCSR_THREAD Thread = CONTAINING_RECORD(Entry, CSR_THREAD, Link);

        if (Thread->ThreadHandle)
        {
            VzClose(Thread->ThreadHandle);
            Thread->ThreadHandle = NULL;
        }

        RtlFreeHeap(Heap, 0, Thread);
    }
}

static CONST PCSR_API_ROUTINE BaseSrvApiTable[BasepMaxApiNumber] = {
    BaseSrvCreateProcess,
    BaseSrvCreateThread,
    BaseSrvExitProcess
};

VEASTATUS
VEAPI
BaseServerDllInitialization(VOID)
{
    CSR_SERVER_DLL ServerDll = {
        .ServerId          = CSR_SERVER_BASE,
        .Name              = "basesrv",
        .ApiTable          = BaseSrvApiTable,
        .ApiCount          = BasepMaxApiNumber,
        .DisconnectRoutine = BaseSrvDisconnect
    };

    return CsrRegisterServerDll(&ServerDll);
}

/* EOF */