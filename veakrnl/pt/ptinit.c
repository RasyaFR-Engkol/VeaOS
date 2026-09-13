#include <veakrnl.h>

PEPROCESS PtSystemProcess;
PETHREAD PtPhase1SystemThread;
KTHREAD PtSystemIdleThread[MAX_CPU];
LIST_ENTRY PtProcessList;

POBJECT_TYPE PtEthreadType;
POBJECT_TYPE PtEprocessType;

BOOLEAN
VEAPI
PtInitSystem(VOID)
{
    // Initialize Process list head
    InitializeListHead(&PtProcessList);

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

    PtInitializeRunQueues();

    PtSystemProcess = PtCreateSystemProcess();
    if(!PtSystemProcess)
    {
        return FALSE;
    }

    PtPhase1SystemThread = PtCreateSystemThread(PtSystemProcess, (PVOID)UlPhase1, NULL, NonPagedPool);
    if(!PtPhase1SystemThread)
    {
        return FALSE;
    }

    return TRUE;
}