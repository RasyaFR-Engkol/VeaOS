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

ULONG
VEAPI
PtCounterUniqueProcessId(VOID)
{
    PtUniqueProcessId += 4; // Harus ditambahkan ke variabel aslinya!
    return PtUniqueProcessId;
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
    PEPROCESS Parent, Process;
    VEASTATUS Status;
    PULONG_PTR VirtualAddress;
    PULONG SystemPageDir;
    ULONG PdeFlags;

    UNREFERENCED_PARAMETER(SectionHandle);
    UNREFERENCED_PARAMETER(DebugPort);
    UNREFERENCED_PARAMETER(ExceptionPort);
    UNREFERENCED_PARAMETER(Flags);

    if (ParentProcess)
    {
        Status = ObReferenceObjectByHandle(
            ParentProcess,
            0x1000, /* TODO: ACCESS_MASK lebih jelas nanti*/
            PtEprocessType,
            KernelMode, /* TODO: Get more Good Access*/
            (PVOID*)&Parent
        );

        if (!VEA_SUCCESS(Status))
        {
            return Status;
        }
    }
    else
    {
        Parent = NULL;
    }

    Status = ObCreateObject(
        KernelMode, /* TODO: Get more Good Access */
        PtEprocessType,
        ObjectAttributes,
        KernelMode,
        NULL,
        sizeof(EPROCESS),
        0,
        0,
        (PVOID*)&Process
    );
    if (!VEA_SUCCESS(Status)) return Status;

    RtlZeroMemory(Process, sizeof(EPROCESS));

    if (Parent)
        Process->ParentProcess = Parent;

    Process->Cid.UniqueProcess = PtCounterUniqueProcessId();

    if (ObjectAttributes && ObjectAttributes->ObjectName && ObjectAttributes->ObjectName->Buffer)
    {
        RtlCopyRawStringN(Process->ImageFileName, (PCHAR)ObjectAttributes->ObjectName->Buffer, 15);
    }
    else if (Parent)
    {
        RtlCopyRawStringN(Process->ImageFileName, Parent->ImageFileName, 15);
    }
    else
    {
        RtlCopyRawStringN(Process->ImageFileName, "SystemProcess", 15);
    }

    Process->ObjectTable = UlCreateHandleTable(TEMPORARY_MAX_HANDLE);

    Status = MmInitializeProcessDirectoryBase(Process);
    if (!VEA_SUCCESS(Status))
    {
        if (Parent) ObDereferenceObject(Parent);
        ObDereferenceObject(Process);
        return Status;
    }

    Process->Pcb.BasePriority = 0;
    Process->Pcb.State = Initialized;
    InitializeListHead(&Process->Pcb.ThreadListHead);

    InsertTailList(&PtProcessList, &Process->ActiveProcessLinks);

    if (ProcessHandle)
    {
        Status = ObInsertObject(
            Process,
            NULL,
            DesiredAccess,
            0,
            NULL,
            ProcessHandle
        );

        if (!VEA_SUCCESS(Status))
        {
            if (Parent) ObDereferenceObject(Parent);
            ObDereferenceObject(Process);
            return Status;
        }
    }

    if (Parent)
    {
        ObDereferenceObject(Parent);
    }

    return STATUS_SUCCESS;
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

PEPROCESS
VEAPI
PtCreateProcess(
    POOL_TYPE ProcessPool,
    PCHAR ImageFileName
)
{
    PEPROCESS Process = NULL;

    VEASTATUS Status = ObCreateObject(
        KernelMode,
        PtEprocessType,
        NULL,
        KernelMode,
        NULL,
        sizeof(EPROCESS), // Pass sizeof(EPROCESS) atau 0
        0,
        0,
        (PVOID*)&Process
    );

    if(!VEA_SUCCESS(Status))
    {
        return NULL;
    }

    // Inisialisasi struct EPROCESS
    Process->Cid.UniqueProcess = PtCounterUniqueProcessId();
    RtlCopyRawStringN(Process->ImageFileName, ImageFileName, 15);
    Process->ObjectTable = UlCreateHandleTable(TEMPORARY_MAX_HANDLE);

    Process->Pcb.DirectoryTableBase = MmAllocatePhysicalPage();
    if(!Process->Pcb.DirectoryTableBase)
    {
        // ObDereferenceObject(Process);
        return NULL;
    }

    PULONG_PTR VirtualAddress = MmMapIoSpace(Process->Pcb.DirectoryTableBase, PAGE_SIZE, MmCached);
    if(!VirtualAddress)
    {
        return NULL;
    }

    RtlZeroMemory(VirtualAddress, PAGE_SIZE);

    PULONG SystemPageDir = (PULONG)MM_GET_PD();

    for(ULONG i = 768; i < 1024; i++)
    {
        VirtualAddress[i] = SystemPageDir[i];
    }

    ULONG Flags = VirtualAddress[1023] & 0xFFF;
    VirtualAddress[1023] = Process->Pcb.DirectoryTableBase | Flags;

    MmUnmapIoSpace(VirtualAddress, PAGE_SIZE);

    Process->Pcb.BasePriority = 0;
    Process->Pcb.State = Initialized;
    
    InitializeListHead(&Process->Pcb.ThreadListHead);
    InsertTailList(&PtProcessList, &Process->ActiveProcessLinks);

    return Process;
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
