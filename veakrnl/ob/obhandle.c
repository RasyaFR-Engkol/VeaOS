/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : obhandle.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Handle
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 19-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file obhandle.c
 * --------------------------------------------------------------------- */

PHANDLE_TABLE ObpKernelHandleTable;

PHANDLE_TABLE
VEAPI
ObpGetHandleTable(IN OUT PHANDLE Handle, IN KPROCESSOR_MODE Mode)
{
    PEPROCESS Process;

    if (ObpIsKernelHandle(*Handle, Mode))
    {
        *Handle = ObKernelHandleToHandle(*Handle);   /* buang flag sebelum HANDLE_TO_INDEX */
        return ObpKernelHandleTable;
    }

    Process = (PEPROCESS)PtGetCurrentProcess();
    return Process ? (PHANDLE_TABLE)Process->ObjectTable : NULL;
}

BOOLEAN
VEAPI
ObpInitKernelHandleTable(VOID)
{
    ObpKernelHandleTable = UlCreateHandleTable(OB_KERNEL_HANDLE_TABLE_SIZE);
    if (!ObpKernelHandleTable)
    {
        DPRINT("OB: Failed to create Kernel Handle Table!\n\r");
        return FALSE;
    }
    return TRUE;
}

VEASTATUS
VEAPI
ObpCreateHandle(
    IN  PVOID           Object,
    IN  ULONG           ObjAttributes,
    IN  KPROCESSOR_MODE AccessMode,
    IN  ACCESS_MASK     GrantedAccess,
    OUT PHANDLE         ReturnedHandle
)
{
    PEPROCESS          Process = (PEPROCESS)PtGetCurrentProcess();
    PHANDLE_TABLE      Table;
    BOOLEAN            IsKernel = FALSE;
    HANDLE             NewHandle;
    HANDLE_TABLE_ENTRY Entry;

    *ReturnedHandle = NULL;

    if ((ObjAttributes & OBJ_KERNEL_HANDLE) || !Process)
    {
        /* User mode tidak boleh minta handle kernel */
        if (AccessMode == UserMode)
            return STATUS_INVALID_PARAMETER;
        IsKernel = TRUE;
    }

    Table = IsKernel ? ObpKernelHandleTable : (PHANDLE_TABLE)Process->ObjectTable;
    if (!Table)
        return STATUS_INVALID_PARAMETER;

    /* Handle punya reference sendiri */
    ObReferenceObject(Object);

    /* Siapkan struktur Entry sebelum di-commit ke Handle Table */
    RtlZeroMemory(&Entry, sizeof(HANDLE_TABLE_ENTRY));
    Entry.Object = Object;
    Entry.GrantedAccess = GrantedAccess;

    NewHandle = UlCreateHandle(Table, &Entry);
    if (!NewHandle)
    {
        ObDereferenceObject(Object);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    if (IsKernel)
        NewHandle = ObMarkHandleAsKernelHandle(NewHandle);

    *ReturnedHandle = NewHandle;
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
ObInitProcess(
    IN PEPROCESS Parent OPTIONAL,
    IN PEPROCESS Process
)
{
    PHANDLE_TABLE ObjectTable;

    if (!Process) 
        return STATUS_INVALID_PARAMETER;

    if (Parent)
    {
        ObjectTable = UlDupHandleTable(Process, Parent->ObjectTable);
    }
    else
    {
        /* Buat Handle Table kosong untuk boot/system process */
        ObjectTable = UlCreateHandleTable(TEMPORARY_MAX_HANDLE);
    }

    if (!ObjectTable)
    {
        Process->ObjectTable = NULL;
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Process->ObjectTable = ObjectTable;
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
ObDuplicateObject(
    IN PEPROCESS SourceProcess,
    IN HANDLE SourceHandle,
    IN PEPROCESS TargetProcess OPTIONAL,
    OUT PHANDLE TargetHandle OPTIONAL,
    IN ACCESS_MASK DesiredAccess,
    IN ULONG HandleAttributes,
    IN ULONG Options,
    IN KPROCESSOR_MODE AccessMode
)
{
    PHANDLE_TABLE SourceTable = NULL;
    PHANDLE_TABLE TargetTable = NULL;
    PHANDLE_TABLE_ENTRY SourceEntry = NULL;
    HANDLE_TABLE_ENTRY NewEntry;
    PVOID Object = NULL;
    ACCESS_MASK TargetAccess = 0;
    HANDLE NewHandle = NULL;
    ULONG Index = 0;

    if (TargetHandle) *TargetHandle = NULL;

    if (ObpIsKernelHandle(SourceHandle, AccessMode))
    {
        SourceTable = ObpKernelHandleTable;
        SourceHandle = ObKernelHandleToHandle(SourceHandle);
    }
    else
    {
        SourceTable = SourceProcess ? (PHANDLE_TABLE)SourceProcess->ObjectTable : NULL;
    }

    if (!SourceTable)
    {
        return STATUS_INVALID_HANDLE;
    }

    Index = HANDLE_TO_INDEX(SourceHandle);
    if (Index == 0 || Index >= SourceTable->MaxEntries)
    {
        return STATUS_INVALID_HANDLE;
    }

    SourceEntry = &SourceTable->Entries[Index];
    if (SourceEntry->Object == NULL)
    {
        return STATUS_INVALID_HANDLE;
    }

    Object = SourceEntry->Object;

    if (Options & DUPLICATE_SAME_ACCESS)
    {
        TargetAccess = SourceEntry->GrantedAccess;
    }
    else
    {
        TargetAccess = DesiredAccess;
    }

    if (TargetProcess && TargetHandle)
    {
        TargetTable = (PHANDLE_TABLE)TargetProcess->ObjectTable;
        if (!TargetTable)
        {
            return STATUS_INVALID_PARAMETER;
        }

        // Tambah Reference Count pada Objek yang Diduplikasi
        ObReferenceObject(Object);

        RtlZeroMemory(&NewEntry, sizeof(HANDLE_TABLE_ENTRY));
        NewEntry.Object = Object;
        NewEntry.GrantedAccess = TargetAccess;

        NewHandle = UlCreateHandle(TargetTable, &NewEntry);
        if (!NewHandle)
        {
            ObDereferenceObject(Object);
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        if (HandleAttributes & OBJ_KERNEL_HANDLE)
        {
            NewHandle = ObMarkHandleAsKernelHandle(NewHandle);
        }

        *TargetHandle = NewHandle;
    }

    if (Options & DUPLICATE_CLOSE_SOURCE)
    {
        UlDestroyHandle(SourceTable, SourceHandle);
    }

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
VeaDuplicateObject(
    IN HANDLE SourceProcessHandle,
    IN HANDLE SourceHandle,
    IN HANDLE TargetProcessHandle OPTIONAL,
    OUT PHANDLE TargetHandle OPTIONAL,
    IN ACCESS_MASK DesiredAccess,
    IN ULONG HandleAttributes,
    IN ULONG Options
)
{
    VEASTATUS Status = STATUS_SUCCESS;
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();
    PEPROCESS SourceProcess = NULL;
    PEPROCESS TargetProcess = NULL;
    HANDLE LocalTargetHandle = NULL;

    //
    // PROBE & ZERO OUT TARGET HANDLE POINTER IN USER SPACE VIA SEH
    //

    if (TargetHandle && PreviousMode == UserMode)
    {
        if ((ULONG_PTR)TargetHandle >= 0x80000000)
        {
            return STATUS_ACCESS_VIOLATION;
        }

        _VEA_TRY
        {
            *(volatile HANDLE*)TargetHandle = NULL;
        }
        _VEA_EXCEPT(1)
        {
            return STATUS_ACCESS_VIOLATION;
        }
        _VEA_END
    }

    //
    // REFERENCE SOURCE PROCESS OBJECT
    //

    if (SourceProcessHandle == NtCurrentProcess() || SourceProcessHandle == (HANDLE)-1)
    {
        SourceProcess = (PEPROCESS)PtGetCurrentProcess();
        ObReferenceObject(SourceProcess);
    }
    else
    {
        Status = ObReferenceObjectByHandle(
            SourceProcessHandle,
            PROCESS_DUP_HANDLE,
            PtEprocessType,
            PreviousMode,
            (PVOID*)&SourceProcess
        );

        if (!VEA_SUCCESS(Status))
        {
            return Status;
        }
    }

    //
    // REFERENCE TARGET PROCESS OBJECT
    //
    if (TargetProcessHandle)
    {
        if (TargetProcessHandle == NtCurrentProcess() || TargetProcessHandle == (HANDLE)-1)
        {
            TargetProcess = (PEPROCESS)PtGetCurrentProcess();
            ObReferenceObject(TargetProcess);
        }
        else
        {
            Status = ObReferenceObjectByHandle(
                TargetProcessHandle,
                PROCESS_DUP_HANDLE,
                PtEprocessType,
                PreviousMode,
                (PVOID*)&TargetProcess
            );

            if (!VEA_SUCCESS(Status))
            {
                ObDereferenceObject(SourceProcess);
                return Status;
            }
        }
    }

    //
    // RUN DUP ENGINE INTERNAL
    //
    Status = ObDuplicateObject(
        SourceProcess,
        SourceHandle,
        TargetProcess,
        TargetHandle ? &LocalTargetHandle : NULL,
        DesiredAccess,
        HandleAttributes,
        Options,
        PreviousMode
    );

    //
    // WRITEBACK RESULT TARGET HANDLE 
    //
    if (VEA_SUCCESS(Status) && TargetHandle)
    {
        if (PreviousMode == UserMode)
        {
            _VEA_TRY
            {
                *TargetHandle = LocalTargetHandle;
            }
            _VEA_EXCEPT(1)
            {
                // Jika User Space crash saat penulisan result, tutup kembali handle yang baru terbuat
                if (TargetProcess && LocalTargetHandle)
                {
                    PHANDLE_TABLE TargetTable = (PHANDLE_TABLE)TargetProcess->ObjectTable;
                    if (TargetTable)
                    {
                        UlDestroyHandle(TargetTable, LocalTargetHandle);
                    }
                }
                Status = STATUS_ACCESS_VIOLATION;
            }
            _VEA_END
        }
        else
        {
            *TargetHandle = LocalTargetHandle;
        }
    }

    //
    // DEREFERENCE PROCESS OBJECTS
    //
    if (TargetProcess)
    {
        ObDereferenceObject(TargetProcess);
    }

    ObDereferenceObject(SourceProcess);

    return Status;
}

/* EOF */