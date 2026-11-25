#include <veakrnl.h>

ULONG PtUniqueProcessId = 100;
KSERVICE_TABLE_DESCRIPTOR KsServiceTableDescriptor[SSDT_MAX_ENTRIES];
KSERVICE_TABLE_DESCRIPTOR KsServiceTableDescriptorShadow[SSDT_MAX_ENTRIES];

static inline VOID PiSwapProcess(PKPROCESS TargetProcess) {
    ULONG_PTR DirectoryTableBase = TargetProcess->DirectoryTableBase; 

    __asm__ __volatile__(
        "mov %0, %%cr3"
        : // Tidak ada output
        : "r" (DirectoryTableBase) // Input operand
        : "memory" // Clobber untuk mencegah reordering oleh kompiler
    );
}

VOID
VEAPI
PiInitializeProcess(
    IN PKPROCESS Process,
    IN UCHAR BasePriority,
    IN PULONG_PTR DirectoryTableBase
)
{
    RtlZeroMemory(Process, sizeof(KPROCESS));

    Process->Dispatcher.Type = ProcessObject;
    Process->Dispatcher.Size = sizeof(KPROCESS) / sizeof(ULONG);
    Process->Dispatcher.SignalState = 0;
    InitializeListHead(&Process->Dispatcher.WaitListHead);

    if (DirectoryTableBase)
    {
        Process->DirectoryTableBase = DirectoryTableBase[0];
    }

    Process->State = Initialized;

    if (BasePriority == PROCESS_PRIORITY_NORMAL)
    {
        Process->BasePriority = 8;
    }
    else
    {
        Process->BasePriority = BasePriority;
    }

    Process->StackCount = 0;
    Process->Token = NULL;

    InitializeListHead(&Process->ThreadListHead);
    KsInitializeSpinlock(&Process->ProcessLock);
}

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
)
{
    VEASTATUS Status;
    PEPROCESS Process = NULL;
    PEPROCESS Parent = NULL;
    PVOID SectionObject = NULL;
    HANDLE_TABLE_ENTRY CidEntry;
    HANDLE hProcess = NULL;
    ULONG_PTR DirectoryTableBase[2] = {0, 0};
    BOOLEAN NeedsPeb = FALSE;
    INITIAL_PEB InitialPeb;
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();

    UNREFERENCED_PARAMETER(DebugPort);
    UNREFERENCED_PARAMETER(ExceptionPort);

    // Validate Flags
    if (Flags & ~PROCESS_CREATE_FLAGS_LEGAL_MASK)
    {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // 1. EVALUASI PARENT PROCESS & FALLBACK
    //
    if (ParentProcess)
    {
        Status = ObReferenceObjectByHandle(
            ParentProcess,
            PROCESS_CREATE_PROCESS,
            PtEprocessType,
            PreviousMode,
            (PVOID*)&Parent
        );
        if (!VEA_SUCCESS(Status)) return Status;
    }
    else
    {
        // FIX CRUSIAL: Jika membuat User-Mode Process (SectionHandle ada)
        // tapi ParentProcess NULL, otomatis jadikan PtSystemProcess sebagai Parent!
        if (SectionHandle)
        {
            Parent = PtSystemProcess;
            ObReferenceObject(Parent);
        }
        else
        {
            Parent = NULL; // Murni Boot Process (PtSystemProcess awal)
        }
    }

    //
    // 2. ALOKASI EPROCESS OBJECT VIA OBJECT MANAGER
    //
    Status = ObCreateObject(
        PreviousMode,
        PtEprocessType,
        ObjectAttributes,
        PreviousMode,
        NULL,
        sizeof(EPROCESS),
        0,
        0,
        (PVOID*)&Process
    );

    if (!VEA_SUCCESS(Status)) goto Cleanup;

    RtlZeroMemory(Process, sizeof(EPROCESS));

    // Setup Dasar Struct Process
    Process->ExitStatus = STATUS_PENDING;
    InitializeListHead(&Process->ThreadListHead);
    InitializeListHead(&Process->ActiveProcessLinks);

    MmpInitializeProcessVadTree(Process);

    if (Parent)
    {
        Process->ParentProcess = Parent;
        Process->InheritedFromUniqueProcessId = Parent->UniqueProcessId;

        if (Parent->Token)
        {
            Process->Token = SfDuplicateToken(Parent->Token);
            if (!Process->Token)
            {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto CleanupWithRef;
            }
        }
        else if (SfSystemToken)
        {
            Process->Token = SfDuplicateToken(SfSystemToken);
            if (!Process->Token)
            {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto CleanupWithRef;
            }
        }
    }
    else
    {
        Process->Token = SfSystemToken;
    }

    //
    // 3. ATUR HANDLES SECTION OBJECT (Fisik Executable Image)
    //
    if (SectionHandle)
    {
        Status = ObReferenceObjectByHandle(
            SectionHandle,
            SECTION_MAP_EXECUTE,
            MmSectionObjectType,
            PreviousMode,
            (PVOID*)&SectionObject
        );
        if (!VEA_SUCCESS(Status)) goto CleanupWithRef;

        Process->SectionObject = SectionObject;
    }
    else if (Parent && Parent != PtSystemProcess)
    {
        // Jalur Forking/Cloning: Warisi SectionObject dari Parent jika ada
        if (Parent->SectionObject)
        {
            SectionObject = Parent->SectionObject;
            ObReferenceObject(SectionObject);
            Process->SectionObject = SectionObject;
        }
        else
        {
            Status = STATUS_PROCESS_IS_TERMINATING;
            goto CleanupWithRef;
        }
    }

    //
    // 4. MEMORY ADDRESS SPACE SETUP (CR3 ALOKASI)
    //
    if (Parent)
    {
        // Buat Page Directory (CR3) baru untuk proses anak
        // Menyalin entri PDE kernel-space dari system process
        if (!MmCreateProcessAddressSpace(Process, DirectoryTableBase))
        {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto CleanupWithRef;
        }
    }
    else
    {
        // Mode Boot/Handbuilt Process (Gunakan CR3 Kernel Awal)
        Status = MiInitializeHandBuiltProcess(Process, DirectoryTableBase);
        if (!VEA_SUCCESS(Status)) goto CleanupWithRef;
    }

    // Inisialisasi PCB (Kernel Process Block) & Load CR3 ke Process Control
    PiInitializeProcess(
        &Process->Pcb,
        PROCESS_PRIORITY_NORMAL,
        DirectoryTableBase
    );

    //
    // 5. INSIALISASI HANDLE TABLE & CID (PROCESS ID)
    //
    if (Parent)
    {
        // Warisi atau buat Handle Table terisolasi
        Status = ObInitProcess(
            (Flags & PROCESS_CREATE_FLAGS_INHERIT_HANDLES) ? Parent : NULL,
            Process
        );
        if (!VEA_SUCCESS(Status)) goto CleanupWithRef;
    }
    else
    {
        Process->ObjectTable = UlCreateHandleTable(TEMPORARY_MAX_HANDLE);
    }

    // Terbitkan Unique Process ID dari CID Table VeaOS
    CidEntry.Object = Process;
    CidEntry.GrantedAccess = 0;
    Process->UniqueProcessId = UlCreateHandle(PtpCidTable, &CidEntry);
    if (!Process->UniqueProcessId)
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CleanupWithRef;
    }
    
    if (Process->ObjectTable)
    {
        Process->ObjectTable->UniqueProcessId = Process->UniqueProcessId;
    }

    //
    // 6. INITIALIZE ADDRESS SPACE & DETEKSI SKENARIO PEB (USER MODE vs SYSTEM/CLONE)
    //
    if (SectionHandle)
    {
        // SKENARIO A: Process User-Mode Berbasis File Executable (DOOM, dll)
        Status = MmInitializeProcessAddressSpace(
            Process,
            SectionObject,
            Process->ImageFileName
        );
        if (!VEA_SUCCESS(Status)) goto CleanupWithRef;

        NeedsPeb = TRUE; // Wajib punya PEB untuk Ring 3!
    }
    else if (Parent)
    {
        if (Parent != PtSystemProcess)
        {
            // SKENARIO B: Process Cloning / Forking (Tanpa Executable File Baru)
            // Copy-On-Write (CoW) VAD Tree dari Parent ke Child
            #if 0
            Status = MmCloneProcessAddressSpace(Parent, Process);
            if (!VEA_SUCCESS(Status)) goto CleanupWithRef;
            NeedsPeb = TRUE;
            #else
            // Jika modul cloning belum kamu aktifkan:
            Status = STATUS_NOT_IMPLEMENTED;
            goto CleanupWithRef;
            #endif
        }
        else
        {
            // SKENARIO C: System Helper Process
            RtlCopyRawStringN(Process->ImageFileName, "SystemHelper", 15);
        }
    }
    else
    {
        // SKENARIO D: Boot Process Kernel
        RtlCopyRawStringN(Process->ImageFileName, "SystemProcess", 15);
    }

    // Overwrite ImageFileName jika ada ObjectAttributes spesifik
    if (ObjectAttributes && ObjectAttributes->ObjectName && ObjectAttributes->ObjectName->Buffer)
    {
        RtlCopyRawStringN(Process->ImageFileName, (PCHAR)ObjectAttributes->ObjectName->Buffer, 15);
    }

    //
    // 7. MAP SYSTEM DLL & BUAT PEB (PROCESS ENVIRONMENT BLOCK)
    //
    if (SectionObject)
    {
        // Map C Runtime / System Subsystem DLL (misal veaso.so / ntdll.dll) ke User Space
        PtpMapSystemSo(Process, NULL, FALSE);
    }

    if (Parent && NeedsPeb)
    {
        RtlZeroMemory(&InitialPeb, sizeof(INITIAL_PEB));
        InitialPeb.Mutant = (HANDLE)-1;

        Status = MmCreatePeb(Process, &InitialPeb, (PPEB*)&Process->Peb);
        if (!VEA_SUCCESS(Status)) goto CleanupWithRef;
    }

    //
    // 8. INSERT PROCESS TO ACTIVE LIST & OBJECT MANAGER TABLE
    //
    KsEnterCriticalRegion();
    InsertTailList(&PtProcessList, &Process->ActiveProcessLinks);
    KsLeaveCriticalRegion();

    Status = ObInsertObject(
        Process,
        NULL,
        DesiredAccess,
        0,
        NULL,
        &hProcess
    );

    if (!VEA_SUCCESS(Status)) goto Cleanup;

    if (ProcessHandle)
    {
        *ProcessHandle = hProcess;
    }

    Process->CreateTime.QuadPart = KsQuerySystemTime();

CleanupWithRef:
    //SSSObDereferenceObject(Process);

Cleanup:
    if (Parent) ObDereferenceObject(Parent);
    return Status;
}

VEASTATUS
VEAPI
PtCreateSystemProcess(
    OUT PHANDLE ProcessHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjAttr
)
{
    return PtpCreateProcess(
        ProcessHandle,
        DesiredAccess,
        ObjAttr,
        NULL,
        0,
        NULL,
        NULL,
        NULL
    );
}

VOID
VEAPI
PtInsertProcess(
    PEPROCESS Process
)
{
    if(!Process) return;

    InsertTailList(&PtProcessList, &Process->ActiveProcessLinks);
}

VOID
VEAPI
PtAttachProcess(
    IN PKPROCESS Process
)
{
    PKTHREAD CurrentThread = PtGetCurrentThread();
    KIRQL OldIrql;

    if (CurrentThread->ApcState.Process == Process) {
        return;
    }

    if (CurrentThread->ApcStateIndex != OriginalApcEnvironment || 
        KsDpcRoutineRunning()) {
        KsBugCheckEx(
            INVALID_OBJECT_ATTACH,
            (ULONG_PTR)Process,
            (ULONG_PTR)CurrentThread->ApcState.Process,
            CurrentThread->ApcStateIndex,
            KsDpcRoutineRunning()
        );
    }

    InterlockedIncrement((PLONG)&Process->StackCount);
    KsAcquireSpinLock(&CurrentThread->ApcQueueLock, &OldIrql);

    CurrentThread->SavedApcState = CurrentThread->ApcState;

    InitializeListHead(&CurrentThread->ApcState.ApcListHead[0]);
    InitializeListHead(&CurrentThread->ApcState.ApcListHead[1]);
    CurrentThread->ApcState.Process = Process;
    CurrentThread->ApcState.KernelApcInProgress = FALSE;
    CurrentThread->ApcState.KernelApcPending = FALSE;
    CurrentThread->ApcState.UserApcPending = FALSE;

    CurrentThread->ApcStatePointer[OriginalApcEnvironment] = &CurrentThread->SavedApcState;
    CurrentThread->ApcStatePointer[AttachedApcEnvironment] = &CurrentThread->ApcState;
    CurrentThread->ApcStateIndex = AttachedApcEnvironment;

    KsReleaseSpinLock(&CurrentThread->ApcQueueLock, OldIrql);

    PiSwapProcess(Process);
}

VOID
VEAPI
PtDetachProcess(VOID)
{
    PKTHREAD CurrentThread = PtGetCurrentThread();
    KIRQL OldIrql;

    if (CurrentThread->ApcStateIndex == OriginalApcEnvironment) {
        return; 
    }

    PKPROCESS AttachedProcess = CurrentThread->ApcState.Process;
    PKPROCESS OriginalProcess = CurrentThread->SavedApcState.Process;

    KsAcquireSpinLock(&CurrentThread->ApcQueueLock, &OldIrql);

    CurrentThread->ApcState = CurrentThread->SavedApcState;
    CurrentThread->SavedApcState.Process = NULL;

    CurrentThread->ApcStatePointer[OriginalApcEnvironment] = &CurrentThread->ApcState;
    CurrentThread->ApcStatePointer[AttachedApcEnvironment] = &CurrentThread->SavedApcState;
    CurrentThread->ApcStateIndex = OriginalApcEnvironment;

    KsReleaseSpinLock(&CurrentThread->ApcQueueLock, OldIrql);

    InterlockedDecrement((PLONG)&AttachedProcess->StackCount);

    PiSwapProcess(OriginalProcess);
}

VEASTATUS
VEAPI
PtpTerminateProcess(
    IN PEPROCESS Process,
    IN VEASTATUS ExitStatus
)
{
    PLIST_ENTRY NextEntry;
    PETHREAD Thread;
    PETHREAD CurrentThread = CONTAINING_RECORD(PtGetCurrentThread(), ETHREAD, Tcb);
    PEPROCESS CurrentProcess = CONTAINING_RECORD(PtGetCurrentProcess(), EPROCESS, Pcb);
    BOOLEAN KillCurrentThread = FALSE;
    KIRQL OldIrql;

    if (!Process)
    {
        return STATUS_INVALID_PARAMETER;
    }

    /* Lindungi System Process agar tidak bisa dimatikan */
    if (Process == PtSystemProcess)
    {
        return STATUS_ACCESS_DENIED;
    }

    /* Cek apakah proses ini punya flag BreakOnTermination */
    PtpCatchCriticalBreak(Process, TRUE, ExitStatus);

    /* Kunci ProcessLock saat menelusuri ThreadListHead */
    KsAcquireSpinLock(&Process->Pcb.ProcessLock, &OldIrql);

    /* Set flag ProcessDelete agar PtpCreateThread menolak thread baru (QuickWayHereBro) */
    Process->Settings.ProcessDelete = TRUE;
    if (Process->ExitStatus == STATUS_PENDING)
    {
        Process->ExitStatus = ExitStatus;
    }

    NextEntry = Process->ThreadListHead.Flink;
    while (NextEntry != &Process->ThreadListHead)
    {
        Thread = CONTAINING_RECORD(NextEntry, ETHREAD, ThreadListEntry);
        NextEntry = NextEntry->Flink;

        if (Thread == CurrentThread)
        {
            /* Tunda terminasi diri sendiri sampai semua sibling thread selesai ditembak */
            KillCurrentThread = TRUE;
            continue;
        }

        /* Tahan referensi sebentar sebelum melepas spinlock agar object tidak hilang */
        ObReferenceObject(Thread);
        KsReleaseSpinLock(&Process->Pcb.ProcessLock, OldIrql);

        PtpTerminateThread(Thread, ExitStatus, FALSE);

        ObDereferenceObject(Thread);
        KsAcquireSpinLock(&Process->Pcb.ProcessLock, &OldIrql);

        /* Reset iterasi dari head karena list bisa berubah saat lock dilepas */
        NextEntry = Process->ThreadListHead.Flink;
        while (NextEntry != &Process->ThreadListHead)
        {
            PETHREAD Candidate = CONTAINING_RECORD(NextEntry, ETHREAD, ThreadListEntry);
            if (!Candidate->Settings.DeadThread && Candidate != CurrentThread)
            {
                break;
            }
            NextEntry = NextEntry->Flink;
        }
    }

    KsReleaseSpinLock(&Process->Pcb.ProcessLock, OldIrql);

    if (Process->ActiveThreads == 0)
    {
        KsSignalDispatcherObject(&Process->Pcb.Dispatcher);
    }

    /* Jika proses yang dimatikan adalah proses kita sendiri, bunuh CurrentThread terakhir */
    if (KillCurrentThread || Process == CurrentProcess)
    {
        PtpTerminateThread(CurrentThread, ExitStatus, TRUE);
    }

    return STATUS_SUCCESS;
}

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
)
{
    VEASTATUS Status = STATUS_SUCCESS;
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();
    HANDLE LocalProcessHandle = NULL;
    OBJECT_ATTRIBUTES LocalObjectAttributes;
    POBJECT_ATTRIBUTES CapturedObjectAttributes = NULL;

    //
    // 1. VALIDASI & CAPTURE POINTER USER MODE VIA SEH
    //
    if (PreviousMode == UserMode)
    {
        // Validasi Alamat Output Pointer ProcessHandle (< 0x80000000)
        if (!ProcessHandle || (ULONG_PTR)ProcessHandle >= 0x80000000)
        {
            return STATUS_ACCESS_VIOLATION;
        }

        _VEA_TRY
        {
            // Probe Write Access untuk ProcessHandle
            *(volatile HANDLE*)ProcessHandle = *(volatile HANDLE*)ProcessHandle;

            // Capture & Probe ObjectAttributes jika disuplai oleh User
            if (ObjectAttributes)
            {
                if ((ULONG_PTR)ObjectAttributes >= 0x80000000)
                {
                    return STATUS_ACCESS_VIOLATION;
                }

                // Copy struct ObjectAttributes ke Kernel Stack
                RtlCopyMemory(&LocalObjectAttributes, ObjectAttributes, sizeof(OBJECT_ATTRIBUTES));
                CapturedObjectAttributes = &LocalObjectAttributes;

                // Probe ObjectName Buffer jika ada
                if (LocalObjectAttributes.ObjectName)
                {
                    if ((ULONG_PTR)LocalObjectAttributes.ObjectName >= 0x80000000)
                    {
                        return STATUS_ACCESS_VIOLATION;
                    }
                }
            }
        }
        _VEA_EXCEPT(1)
        {
            return STATUS_ACCESS_VIOLATION;
        }
        _VEA_END
    }
    else
    {
        // Panggilan dari Kernel Mode (Trusted Boundary)
        CapturedObjectAttributes = ObjectAttributes;
    }

    //
    // 2. EKSREKUSI ENGINE UTAMA
    //
    Status = PtpCreateProcess(
        &LocalProcessHandle,
        DesiredAccess,
        CapturedObjectAttributes,
        ParentProcess,
        Flags,
        SectionHandle,
        DebugPort,
        ExceptionPort
    );

    //
    // 3. TULIS KEMBALI RESULT HANDLE KE USER SPACE (LEAK-PROOF WRITEBACK)
    //
    if (VEA_SUCCESS(Status))
    {
        if (PreviousMode == UserMode)
        {
            _VEA_TRY
            {
                *ProcessHandle = LocalProcessHandle;
            }
            _VEA_EXCEPT(1)
            {
                // PROTEKSI KEBOCORAN HANDLE (LEAK PREVENTION):
                // Jika User Space mengalami #PF / crash saat Kernel menulis *ProcessHandle,
                // Handle yang terlanjur terbuat di Object Table harus ditutup kembali.
                VzClose(LocalProcessHandle);
                Status = STATUS_ACCESS_VIOLATION;
            }
            _VEA_END
        }
        else
        {
            *ProcessHandle = LocalProcessHandle;
        }
    }

    return Status;
}

VEASTATUS
VEAPI
VeaQueryInformationProcess(
    IN HANDLE ProcessHandle,
    IN PROCESSINFOCLASS ProcessInformationClass,
    OUT PVOID ProcessInformation,
    IN ULONG ProcessInformationLength,
    OUT PULONG ReturnLength OPTIONAL
)
{
    VEASTATUS Status = STATUS_SUCCESS;
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();
    PEPROCESS Process = NULL;
    ULONG RequiredLength = 0;

    if (!ProcessInformation && ProcessInformationLength > 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (PreviousMode == UserMode)
    {
        _VEA_TRY
        {
            if (ProcessInformationLength > 0)
            {
                if ((ULONG_PTR)ProcessInformation >= 0x80000000 ||
                    ((ULONG_PTR)ProcessInformation + ProcessInformationLength) >= 0x80000000)
                {
                    return STATUS_ACCESS_VIOLATION;
                }

                // Probe write buffer
                RtlZeroMemory(ProcessInformation, ProcessInformationLength);
            }

            if (ReturnLength)
            {
                if ((ULONG_PTR)ReturnLength >= 0x80000000)
                {
                    return STATUS_ACCESS_VIOLATION;
                }
                *ReturnLength = *ReturnLength;
            }
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            return STATUS_ACCESS_VIOLATION;
        }
        _VEA_END
    }

    if (ProcessHandle == NtCurrentProcess() || ProcessHandle == (HANDLE)-1)
    {
        Process = (PEPROCESS)PtGetCurrentProcess();
        ObReferenceObject(Process);
    }
    else
    {
        Status = ObReferenceObjectByHandle(
            ProcessHandle,
            PROCESS_QUERY_INFORMATION,
            PtEprocessType,
            PreviousMode,
            (PVOID*)&Process
        );

        if (!VEA_SUCCESS(Status))
        {
            return Status;
        }
    }

    switch (ProcessInformationClass)
    {
        case ProcessBasicInformation:
        {
            PROCESS_BASIC_INFORMATION BasicInfo;
            RequiredLength = sizeof(PROCESS_BASIC_INFORMATION);

            if (ProcessInformationLength < RequiredLength)
            {
                Status = STATUS_INFO_LENGTH_MISMATCH;
                break;
            }

            RtlZeroMemory(&BasicInfo, sizeof(PROCESS_BASIC_INFORMATION));
            BasicInfo.ExitStatus                   = Process->ExitStatus;
            BasicInfo.PebBaseAddress               = Process->Peb;
            BasicInfo.AffinityMask                 = 0x00000001; // Default 1 Core Mask
            BasicInfo.BasePriority                 = (UCHAR)Process->Pcb.BasePriority;
            BasicInfo.UniqueProcessId              = (ULONG_PTR)Process->UniqueProcessId;
            BasicInfo.InheritedFromUniqueProcessId = (ULONG_PTR)Process->InheritedFromUniqueProcessId;

            if (PreviousMode == UserMode)
            {
                _VEA_TRY
                {
                    RtlCopyMemory(ProcessInformation, &BasicInfo, sizeof(PROCESS_BASIC_INFORMATION));
                }
                _VEA_EXCEPT(1)
                {
                    Status = STATUS_ACCESS_VIOLATION;
                }
                _VEA_END
            }
            else
            {
                RtlCopyMemory(ProcessInformation, &BasicInfo, sizeof(PROCESS_BASIC_INFORMATION));
            }
            break;
        }

        case ProcessSessionId:
        {
            ULONG SessionId = 0;
            RequiredLength = sizeof(ULONG);

            if (ProcessInformationLength < RequiredLength)
            {
                Status = STATUS_INFO_LENGTH_MISMATCH;
                break;
            }

            if (Process->Peb)
            {
                SessionId = Process->Peb->SessionId;
            }

            if (PreviousMode == UserMode)
            {
                _VEA_TRY
                {
                    *(PULONG)ProcessInformation = SessionId;
                }
                _VEA_EXCEPT(1)
                {
                    Status = STATUS_ACCESS_VIOLATION;
                }
                _VEA_END
            }
            else
            {
                *(PULONG)ProcessInformation = SessionId;
            }
            break;
        }

        case ProcessImageFileName:
        {
            // Salin string nama ImageFileName milik EPROCESS (16 bytes)
            RequiredLength = sizeof(Process->ImageFileName);

            if (ProcessInformationLength < RequiredLength)
            {
                Status = STATUS_INFO_LENGTH_MISMATCH;
                break;
            }

            if (PreviousMode == UserMode)
            {
                _VEA_TRY
                {
                    RtlCopyMemory(ProcessInformation, Process->ImageFileName, RequiredLength);
                }
                _VEA_EXCEPT(1)
                {
                    Status = STATUS_ACCESS_VIOLATION;
                }
                _VEA_END
            }
            else
            {
                RtlCopyMemory(ProcessInformation, Process->ImageFileName, RequiredLength);
            }
            break;
        }

        case ProcessWow64Information:
        {
            ULONG_PTR Wow64Info = 0; // 32-bit VeaOS (Non-WOW64)
            RequiredLength = sizeof(ULONG_PTR);

            if (ProcessInformationLength < RequiredLength)
            {
                Status = STATUS_INFO_LENGTH_MISMATCH;
                break;
            }

            if (PreviousMode == UserMode)
            {
                _VEA_TRY
                {
                    *(PULONG_PTR)ProcessInformation = Wow64Info;
                }
                _VEA_EXCEPT(1)
                {
                    Status = STATUS_ACCESS_VIOLATION;
                }
                _VEA_END
            }
            else
            {
                *(PULONG_PTR)ProcessInformation = Wow64Info;
            }
            break;
        }

        default:
            Status = STATUS_INVALID_INFO_CLASS;
            break;
    }

    if (ReturnLength)
    {
        if (PreviousMode == UserMode)
        {
            _VEA_TRY
            {
                *ReturnLength = RequiredLength;
            }
            _VEA_EXCEPT(1)
            {
                Status = STATUS_ACCESS_VIOLATION;
            }
            _VEA_END
        }
        else
        {
            *ReturnLength = RequiredLength;
        }
    }

    ObDereferenceObject(Process);
    return Status;
}

VEASTATUS
VEAPI
VeaTerminateProcess(
    IN HANDLE ProcessHandle OPTIONAL,
    IN VEASTATUS ExitStatus
)
{
    VEASTATUS Status;
    PEPROCESS Process;
    PEPROCESS CurrentProcess = CONTAINING_RECORD(PtGetCurrentProcess(), EPROCESS, Pcb);
    PETHREAD CurrentThread = CONTAINING_RECORD(PtGetCurrentThread(), ETHREAD, Tcb);
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();

    /*
     * KASUS KHUSUS REACTOS / WINDOWS:
     * Jika ProcessHandle == NULL, matikan semua thread lain di CurrentProcess
     * KECUALI CurrentThread (dipakai oleh ExitProcess di Ring 3).
     */
    if (ProcessHandle == NULL)
    {
        PLIST_ENTRY NextEntry;
        PETHREAD Thread;
        KIRQL OldIrql;

        KsAcquireSpinLock(&CurrentProcess->Pcb.ProcessLock, &OldIrql);
        CurrentProcess->ExitStatus = ExitStatus;

        NextEntry = CurrentProcess->ThreadListHead.Flink;
        while (NextEntry != &CurrentProcess->ThreadListHead)
        {
            Thread = CONTAINING_RECORD(NextEntry, ETHREAD, ThreadListEntry);
            NextEntry = NextEntry->Flink;

            if (Thread != CurrentThread && !Thread->Settings.DeadThread)
            {
                ObReferenceObject(Thread);
                KsReleaseSpinLock(&CurrentProcess->Pcb.ProcessLock, OldIrql);

                PtpTerminateThread(Thread, ExitStatus, FALSE);

                ObDereferenceObject(Thread);
                KsAcquireSpinLock(&CurrentProcess->Pcb.ProcessLock, &OldIrql);
                NextEntry = CurrentProcess->ThreadListHead.Flink;
            }
        }

        KsReleaseSpinLock(&CurrentProcess->Pcb.ProcessLock, OldIrql);
        return STATUS_SUCCESS;
    }

    /* Handle pseudo-handle NtCurrentProcess() / (HANDLE)-1 */
    if (ProcessHandle == NtCurrentProcess())
    {
        Process = CurrentProcess;
        ObReferenceObject(Process);
    }
    else
    {
        Status = ObReferenceObjectByHandle(
            ProcessHandle,
            PROCESS_TERMINATE,
            PtEprocessType,
            PreviousMode,
            (PVOID*)&Process
        );
        if (!VEA_SUCCESS(Status))
        {
            return Status;
        }
    }

    /* Jika mematikan proses sendiri, lepas ref object dulu karena tidak akan return */
    if (Process == CurrentProcess)
    {
        ObDereferenceObject(Process);
        return PtpTerminateProcess(Process, ExitStatus);
    }

    Status = PtpTerminateProcess(Process, ExitStatus);
    ObDereferenceObject(Process);
    return Status;
}

/* EOF */