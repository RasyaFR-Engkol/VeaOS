#include <veakrnl.h>

PEPROCESS PtSystemProcess;
PETHREAD PtPhase1SystemThread;
KTHREAD PtSystemIdleThread[MAX_CPU];
LIST_ENTRY PtProcessList;
LIST_ENTRY PtDeadThreadListHead;

POBJECT_TYPE PtEthreadType;
POBJECT_TYPE PtEprocessType;

VOID
VEAPI
PtpReaperThreadRoutine(
    PVOID Context
);

BOOLEAN
VEAPI
PtInitSystem(VOID)
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
    PtInitializer.DeleteRoutine = NULL; // TODO: PtDeleteProcess
    PtInitializer.ParseRoutine = NULL;
    PtInitializer.InterruptHandler = NULL;
    PtInitializer.PoolTag = 'Pt  ';
    PtInitializer.PoolType = PagedPool;
    PtInitializer.Length = sizeof(OBJECT_TYPE_INITIALIZER);
    ObCreateObjectType("EProcess", &PtInitializer, &PtEprocessType);

    // Initialize RunQueue by every CPU
    PtInitializeRunQueues();

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

    // Close it. It will not closed
    VeaClose(ObjHandle);

    // Make UlPhase1 System Thread
    PtPhase1SystemThread = PtCreateSystemThread(PtSystemProcess, (PVOID)UlPhase1, NULL, NonPagedPool);
    if(!PtPhase1SystemThread)
    {
        return FALSE;
    }

    // Make Reaper Thread 
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

    return TRUE;
}