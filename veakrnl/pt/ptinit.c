#include <veakrnl.h>

PEPROCESS PtSystemProcess;
PETHREAD PtPhase1SystemThread;
KTHREAD PtSystemIdleThread[MAX_CPU];
LIST_ENTRY PtProcessList;
LIST_ENTRY PtDeadThreadListHead;
LIST_ENTRY PtLoadedModuleList;
POBJECT_TYPE PtEthreadType;
POBJECT_TYPE PtEprocessType;
PHANDLE_TABLE PtpCidTable;

VOID
VEAPI
PtpReaperThreadRoutine(
    PVOID Context
);

BOOLEAN
VEAPI
PtpInitSystem0(VOID)
{
    HANDLE ObjHandle;
    OBJECT_ATTRIBUTES ObjAttr;
    VEASTATUS Status;

    // Initialize Process list head
    InitializeListHead(&PtProcessList);
    InitializeListHead(&PtDeadThreadListHead);

    // Initialize Object Type for Pt Subsystem
    OBJECT_TYPE_INITIALIZER PtInitializer;

    // Make new ETHREAD Object
    RtlZeroMemory(&PtInitializer, sizeof(OBJECT_TYPE_INITIALIZER));
    PtInitializer.ObjectSize = sizeof(ETHREAD);
    PtInitializer.AllowAttachByDefault = FALSE;
    PtInitializer.CreateRoutine = NULL;
    PtInitializer.DeleteRoutine = NULL; // TODO: PtDeleteThread
    PtInitializer.ParseRoutine = NULL;
    PtInitializer.InterruptHandler = NULL;
    PtInitializer.PoolTag = 'Pt  ';
    PtInitializer.PoolType = PagedPool;
    PtInitializer.Length = sizeof(OBJECT_TYPE_INITIALIZER);
    ObCreateObjectType("EThread", &PtInitializer, &PtEthreadType);

    // Make new EPROCESS Object
    RtlZeroMemory(&PtInitializer, sizeof(OBJECT_TYPE_INITIALIZER));
    PtInitializer.ObjectSize = sizeof(EPROCESS);
    PtInitializer.AllowAttachByDefault = FALSE;
    PtInitializer.CreateRoutine = NULL;
    PtInitializer.DeleteRoutine = PtDeleteProcess;
    PtInitializer.ParseRoutine = NULL;
    PtInitializer.InterruptHandler = NULL;
    PtInitializer.PoolTag = 'Pt  ';
    PtInitializer.PoolType = PagedPool;
    PtInitializer.Length = sizeof(OBJECT_TYPE_INITIALIZER);
    ObCreateObjectType("EProcess", &PtInitializer, &PtEprocessType);

    // Initialize RunQueue by every CPU
    PtInitializeRunQueues();

    //
    // Initializing PtpCidTable for Client ID making
    //

    PtpCidTable = UlCreateHandleTable(1000);
    if (!PtpCidTable) return FALSE;

    // Make System Process
    InitializeObjectAttributes(&ObjAttr, NULL, OBJ_KERNEL_HANDLE | OBJ_PERMANENT, NULL, NULL);
    Status = PtCreateSystemProcess(&ObjHandle, 1000, &ObjAttr);
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("Failed to create SystemProcess: 0x%x\n", Status);
        return FALSE;
    }

    // Reference PtSystemProcess and point to PtSystemProcess
    Status = ObReferenceObjectByHandle(
        ObjHandle,
        1000,
        PtEprocessType,
        KernelMode,
        (PVOID*)&PtSystemProcess
    );
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("Failed to reference SystemProcess: 0x%x\n", Status);
        return FALSE;
    }
    
    if(!PtSystemProcess)
    {
        DPRINT("PtSystemProcess not exist\n");
        return FALSE;
    }

    VeaClose(ObjHandle);

    //
    // Make Phase1 System Thread for more futher VeaOS initialization
    //

    HANDLE ThreadHandle;

    Status = PtCreateSystemThread(
        &ThreadHandle,
        THREAD_ALL_ACCESS,
        NULL,
        NULL,                       // ProcessHandle NULL -> otomatis fallback ke PtSystemProcess
        NULL,                       // ClientId (Opsional)
        (PKSTART_ROUTINE)UlPhase1, // Routine Start
        NULL                        // Argument/Context
    );

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("Failed to create Phase1 System Thread: 0x%x\n", Status);
        return FALSE;
    }

    Status = ObReferenceObjectByHandle(
        ThreadHandle,
        THREAD_ALL_ACCESS,
        PtEthreadType,
        KernelMode,
        (PVOID*)&PtPhase1SystemThread
    );

    VeaClose(ThreadHandle);

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("Failed to reference Phase1 System Thread object: 0x%x\n", Status);
        return FALSE;
    }

    //
    // Create ReaperThread. In case Thread has shut down and the memory should be freed
    //

    PETHREAD ReaperThread = PtCreateSystemThreadForMainProcess(
        (PVOID)PtpReaperThreadRoutine,
        NULL,
        NonPagedPool
    );

    if (!ReaperThread)
    {
        DPRINT("Gagal spawn Reaper Thread!\n");
        return FALSE;
    }

    return TRUE;
}

BOOLEAN
VEAPI
PtpInitSystem1(VOID)
{
    VEASTATUS Status;

    // 1. Cari LdrInitializeThunk (Loader Entrypoint Utama Ring 3)
    Status = PtpLookupSystemSoEntryPoint(
        "LdrInitializeThunk",
        &PtpSystemSoEntryPoint
    );
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("Failed to found LdrInitializeThunk in VeaSO!\n");
        KsBugCheckEx(PROCESSTHREAD_INITIALIZATION_FAILED, Status, 7, 0, 0);
        return FALSE;
    }

    // 2. Cari Dispatcher Trampoline Ring 3 Tambahan
    Status = PtpLookupSystemSoEntryPoint(
        "KsiUserApcDispatcher",
        &KsUserApcDispatcher
    );
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("Failed to found KsiUserApcDispatcher in VeaSO\n");
        KsBugCheckEx(PROCESSTHREAD_INITIALIZATION_FAILED, Status, 8, 0, 0);
        return FALSE;
    }

    Status = PtpLookupSystemSoEntryPoint(
        "KsiUserExceptionDispatcher",
        &KsUserExceptionDispatcher
    );
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("KiUserExceptionDispatcher not found in VEASO.SO .\n");
    }

    Status = PtpLookupSystemSoEntryPoint(
        "KsiUserCallbackDispatcher",
        &KsUserCallbackDispatcher
    );

    Status = PtpLookupSystemSoEntryPoint(
        "KsiRaiseUserExceptionDispatcher",
        &KsRaiseUserExceptionDispatcher
    );

    DPRINT("[PT/INIT1] Phase 1 System SO Entry Points successfully linked!\n");
    return TRUE;
}

BOOLEAN
VEAPI
PtInitSystem(VOID)
{
    switch (UlpInitializationPhase)
    {
        case 0:
            return PtpInitSystem0();

        case 1:
            return PtpInitSystem1();

        default:
            KsBugCheck(UNKNOWN_INITIALIZATION_CALL);
    }
}