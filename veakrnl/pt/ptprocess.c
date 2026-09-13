#include <veakrnl.h>

ULONG PtUniqueProcessId = 100;

ULONG
VEAPI
PtCounterUniqueProcessId(VOID)
{
    PtUniqueProcessId += 4; // Harus ditambahkan ke variabel aslinya!
    return PtUniqueProcessId;
}

PEPROCESS
VEAPI
PtCreateSystemProcess(
    VOID
)
{
    PEPROCESS Process = NULL;

    VEASTATUS Status = ObCreateObject(
        KernelMode,
        PtEprocessType,
        NULL,
        KernelMode,
        NULL,
        sizeof(EPROCESS),
        0,
        0,
        (PVOID)&Process
    );
    
    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("ERROR: Failed to create System Process. Status 0x%x\n\r", Status);
        return NULL;
    }

    Process->Cid.UniqueProcess = 4; 
    RtlCopyRawStringN(Process->ImageFileName, "System", 15);

    Process->ObjectTable = UlCreateHandleTable(100);
    Process->Pcb.DirectoryTableBase = (ULONG_PTR)MiGetSystemPageDirectoryTableBase();
    Process->Pcb.BasePriority = 8; // Prioritas bawaan kernel (Normal/Higher)
    Process->Pcb.State = Initialized;

    InitializeListHead(&Process->Pcb.ThreadListHead);
    InsertTailList(&PtProcessList, &Process->ActiveProcessLinks);

    return Process;
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
    Process->ObjectTable = UlCreateHandleTable(100);

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