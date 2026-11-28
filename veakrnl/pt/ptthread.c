#include <veakrnl.h>

ULONG PtUniqueThreadId = 0x10;
extern KRUNQUEUE PtRunQueue[MAX_CPU];

VOID
VEAPI
PtEnqueueThreadCfs(
    PKRUNQUEUE RunQueue, 
    PKTHREAD Thread
);

VOID
VEAPI
PtpExitThread(
    VEASTATUS ExitStatus
);

ULONG
VEAPI
PtCounterUniqueThreadId(VOID)
{
    PtUniqueThreadId += 4;
    return PtUniqueThreadId;
}

static inline VOID
PtpInitializeThreadFpuState(
    IN PKTHREAD Thread
)
{
    ULONG_PTR RawAddr = (ULONG_PTR)Thread->FxSaveBuffer;
    Thread->FxSaveArea = (PFXSAVE_FORMAT)((RawAddr + 15) & ~15UL);

    RtlZeroMemory(Thread->FxSaveArea, sizeof(FXSAVE_FORMAT));
    Thread->FxSaveArea->ControlWord = 0x037F;     /* Default x87 FCW (mask all exceptions) */
    Thread->FxSaveArea->TagWord     = 0x00;       /* Abridged FTW */
    Thread->FxSaveArea->MxCsr       = 0x00001F80; /* Default SSE MXCSR (mask all exceptions) */
}

VOID
VEAPI
PtpTerminateThreadApc(
    IN PKAPC Apc,
    IN OUT PKNORMAL_ROUTINE *NormalRoutine,
    IN OUT PVOID *NormalContext,
    IN OUT PVOID *SystemArgument1,
    IN OUT PVOID *SystemArgument2
)
{
    VEASTATUS ExitStatus = (VEASTATUS)(ULONG_PTR)(*SystemArgument1);

    UNREFERENCED_PARAMETER(NormalRoutine);
    UNREFERENCED_PARAMETER(NormalContext);
    UNREFERENCED_PARAMETER(SystemArgument2);

    /* Bebaskan struktur KAPC yang dialokasikan saat antre */
    UlFreePool(Apc);

    /* Sekarang kita sudah berada di konteks thread target, aman untuk exit! */
    PtpExitThread(ExitStatus);
}

VOID
VEAPI
PiThreadStartup(VOID)
{
    PKTHREAD Thread = PtGetCurrentThread();
    PKSTART_FRAME StartFrame = (PKSTART_FRAME)((ULONG_PTR)Thread->KernelStack + sizeof(KSWITCH_FRAME));

    KeLowerIrql(PASSIVE_LEVEL);

    DPRINT("PiThreadStartup()\n");

    if (StartFrame->UserThread)
    {
        if (StartFrame->SystemRoutine)
        {
            StartFrame->SystemRoutine(StartFrame->StartRoutine, StartFrame->StartContext);
        }

        RiServiceExit(Thread->TrapFrame);
    }
    else
    {
        if (StartFrame->SystemRoutine)
        {
            StartFrame->SystemRoutine(StartFrame->StartRoutine, StartFrame->StartContext);
        }
        else if (StartFrame->StartRoutine)
        {
            StartFrame->StartRoutine(StartFrame->StartContext);
        }

        // PROTEKSI INTEGRITAS KERNEL:
        // System Thread TIDAK BOLEH melakukan return! Jika return, hentikan sistem dengan BugCheck.
        KsBugCheckEx(
            ABNORMAL_SYSTEM_THREAD_TERMINATION,
            (ULONG_PTR)StartFrame->StartRoutine,
            (ULONG_PTR)Thread,
            0,
            0
        );
    }
}

VOID
VEAPI
PtpSystemThreadStartup(
    IN PKSTART_ROUTINE StartRoutine,
    IN PVOID StartContext
)
{

    KeLowerIrql(PASSIVE_LEVEL);

    if (StartRoutine)
    {
        StartRoutine(StartContext);
    }

    PtpExitThread(STATUS_SUCCESS);
}

VOID
VEAPI
PtpUserThreadStartup(
    IN PKSTART_ROUTINE StartRoutine,
    IN PVOID StartContext
)
{
    PETHREAD Thread = (PETHREAD)PtGetCurrentThread();
    PKTRAP_FRAME TrapFrame;
    BOOLEAN DeadThread = FALSE;
    KIRQL OldIrql;

    UNREFERENCED_PARAMETER(StartContext);

    KeLowerIrql(PASSIVE_LEVEL);

    if (Thread->Settings.DeadThread)
    {
        DeadThread = TRUE;
    }

    // Ambil TrapFrame yang sudah dialokasikan di puncak Kernel Stack (bukan InitialStack!)
    TrapFrame = Thread->Tcb.TrapFrame;
    if (!TrapFrame)
    {
        TrapFrame = (PKTRAP_FRAME)((ULONG_PTR)Thread->Tcb.InitialStack - sizeof(KTRAP_FRAME));
        Thread->Tcb.TrapFrame = TrapFrame;
    }

    // Pastikan target akhir setelah LdrpInit (VzContinue) mengarah ke e_entry ELF (StartRoutine)
    if (StartRoutine)
    {
        TrapFrame->Eip = (ULONG_PTR)StartRoutine;
        TrapFrame->Eax = (ULONG_PTR)StartRoutine;
    }
    TrapFrame->PreviousPreviousMode = UserMode;

    if (!DeadThread)
    {
        OldIrql = KeRaiseIrql(APC_LEVEL);

        KsiInitializeUserApc(
            TrapFrame,
            PtpSystemSoEntryPoint,
            NULL,
            PtpSystemSoBase,
            NULL
        );

        KeLowerIrql(OldIrql);
    }
    else
    {
        PtpTerminateThread(Thread, STATUS_THREAD_IS_TERMINATING, TRUE);
    }

    // WAJIB: Terjun ke Ring 3 via KsiFastCallExitHandler (jangan return ke PiThreadStartup!)
    RiServiceExit(TrapFrame);
}


VOID
VEAPI
PtpExitThread(
    VEASTATUS ExitStatus
)
{
    PKTHREAD KThread = PtGetCurrentThread();
    if (!KThread)
    {
        // Safety net jika dipanggil di luar konteks thread valid
        for (;;) { __Halt(); }
    }

    PETHREAD CurrentThread = CONTAINING_RECORD(KThread, ETHREAD, Tcb);
    PEPROCESS ParentProcess = NULL;
    BOOLEAN ProcessExiting = FALSE;
    ULONG_PTR Flags = PtpSaveAndDisableInterrupts();

    CurrentThread->ExitStatus = ExitStatus;
    CurrentThread->Tcb.State = Terminated;

    DPRINT("%p Thread exit with status: 0x%x\n", CurrentThread, CurrentThread->ExitStatus);

    BOOLEAN WasInProcessList = !IsListEmpty(&CurrentThread->ThreadListEntry);
    if (WasInProcessList)
    {
        RemoveEntryList(&CurrentThread->ThreadListEntry);
        InitializeListHead(&CurrentThread->ThreadListEntry);
    }

    KsSignalDispatcherObject(&CurrentThread->Tcb.Dispatcher);

    if (CurrentThread->Tcb.ApcState.Process)
    {
        ParentProcess = CONTAINING_RECORD(
            CurrentThread->Tcb.ApcState.Process,
            EPROCESS,
            Pcb
        );

        if (WasInProcessList &&
            InterlockedDecrement((PLONG)&ParentProcess->ActiveThreads) == 0)
        {
            if (ParentProcess->ExitStatus == STATUS_PENDING)
            {
                ParentProcess->ExitStatus = ExitStatus;
            }
            ParentProcess->Settings.ProcessDelete = TRUE;
            ProcessExiting = TRUE;
        }
    }

    if ((ULONG_PTR)CurrentThread->Tcb.ServiceTable == (ULONG_PTR)KsServiceTableDescriptorShadow &&
        PtpVeapiCallouts.ThreadCallout != NULL)
    {
        PtpVeapiCallouts.ThreadCallout((PETHREAD)CurrentThread, VeapiThreadCalloutExit);
    }

    if (ProcessExiting &&
        ParentProcess != PtSystemProcess &&
        PtpVeapiCallouts.ProcessCallout != NULL)
    {
        VEASTATUS CalloutStatus = PtpVeapiCallouts.ProcessCallout(ParentProcess, FALSE);
        if (!VEA_SUCCESS(CalloutStatus))
        {
            DPRINT("Process exit callout failed for %p: 0x%x\n", ParentProcess, CalloutStatus);
        }
    }

    if (ProcessExiting)
    {
        KIRQL ListIrql;

        /* Cabut dari daftar proses aktif begitu thread terakhir exit */
        ListIrql = KsAcquireDispatcherLock();
        if (!IsListEmpty(&ParentProcess->ActiveProcessLinks) &&
            ParentProcess->ActiveProcessLinks.Flink != NULL)
        {
            RemoveEntryList(&ParentProcess->ActiveProcessLinks);
            InitializeListHead(&ParentProcess->ActiveProcessLinks);
        }
        KsReleaseDispatcherLock(ListIrql);

        /* Hapus entry Process CID dari PtpCidTable agar tidak dangling */
        if (ParentProcess->UniqueProcessId)
        {
            UlDestroyHandle(PtpCidTable, ParentProcess->UniqueProcessId);
            ParentProcess->UniqueProcessId = NULL;
        }

        KsSignalDispatcherObject(&ParentProcess->Pcb.Dispatcher);
    }

    if (ParentProcess != NULL)
    {
        ObDereferenceObject(ParentProcess);
    }

    Flags = PtpSaveAndDisableInterrupts();
    InsertTailList(&PtDeadThreadListHead, &CurrentThread->ThreadListEntry);
    PtYieldThread();
    PtpRestoreInterrupts(Flags);

    for (;;)
    {
        __Halt();
    }
}

VOID
VEAPI
PiInitializeThread(
    IN PKTHREAD Thread,
    IN PVOID KernelStack,
    IN PVOID InitialStack,
    IN PVOID StackLimit,
    IN CHAR Priority
)
{
    RtlZeroMemory(Thread, sizeof(KTHREAD));

    Thread->Dispatcher.Type = ThreadObject;
    Thread->Dispatcher.Size = sizeof(KTHREAD) / sizeof(ULONG);
    Thread->Dispatcher.SignalState = 0;
    InitializeListHead(&Thread->Dispatcher.WaitListHead);

    Thread->KernelStack = KernelStack;
    Thread->InitialStack = InitialStack;
    Thread->StackLimit = StackLimit;
    Thread->Priority = Priority;
    Thread->State = Initialized;
    Thread->LargeStack = FALSE;

    Thread->ApcStatePointer[OriginalApcEnvironment] = &Thread->ApcState;
    Thread->ApcStatePointer[AttachedApcEnvironment] = &Thread->SavedApcState;
    Thread->ApcStateIndex = OriginalApcEnvironment;

    InitializeListHead(&Thread->ApcState.ApcListHead[KernelMode]);
    InitializeListHead(&Thread->ApcState.ApcListHead[UserMode]);
    InitializeListHead(&Thread->SavedApcState.ApcListHead[KernelMode]);
    InitializeListHead(&Thread->SavedApcState.ApcListHead[UserMode]);

    KsInitializeSpinlock(&Thread->ApcQueueLock);
    InitializeListHead(&Thread->MutantListHead);
    InitializeListHead(&Thread->WaitListEntry);

    Thread->ApcQueueable = TRUE;
    Thread->KernelApcDisable = 0;
    Thread->SpecialApcDisable = 0;
    Thread->Alertable = FALSE;
    Thread->WaitMode = KernelMode;

    PtpInitializeThreadFpuState(Thread);
}

PVOID
VEAPI
PtpSetupInitialThreadContext(
    IN PKTHREAD Thread,
    IN PVOID UserStack,
    IN PKSYSTEM_ROUTINE SystemRoutine,
    IN PKSTART_ROUTINE StartRoutine,
    IN PVOID StartContext,
    IN KPROCESSOR_MODE ProcessorMode
)
{
    if (ProcessorMode == UserMode)
    {
        PKUINIT_FRAME InitFrame = (PKUINIT_FRAME)((ULONG_PTR)Thread->InitialStack - sizeof(KUINIT_FRAME));
        RtlZeroMemory(InitFrame, sizeof(KUINIT_FRAME));

        // 1. Setup TrapFrame untuk Ring 3
        PKTRAP_FRAME TrapFrame = &InitFrame->TrapFrame;
        TrapFrame->SegCs = KGDT32_R3_CODE | 3;
        TrapFrame->SegDs = KGDT32_R3_DATA | 3;
        TrapFrame->SegEs = KGDT32_R3_DATA | 3;
        TrapFrame->SegFs = KGDT32_R0_PCR;
        TrapFrame->HardwareSegSs = KGDT32_R3_DATA | 3;
        TrapFrame->HardwareEsp = (ULONG)UserStack;
        TrapFrame->Eip = (ULONG)StartRoutine;
        TrapFrame->EFlags = 0x202; // IF=1
        TrapFrame->PreviousPreviousMode = UserMode;

        Thread->TrapFrame = TrapFrame;
        Thread->PreviousMode = UserMode;

        // 2. Setup StartFrame
        InitFrame->StartFrame.SystemRoutine = SystemRoutine;
        InitFrame->StartFrame.StartRoutine = StartRoutine;
        InitFrame->StartFrame.StartContext = StartContext;
        InitFrame->StartFrame.UserThread = TRUE;

        // 3. Setup Context Switch Frame
        InitFrame->CtxSwitchFrame.SwapReturnAddress = (ULONG)PiThreadStartup;
        InitFrame->CtxSwitchFrame.EFlags = 0x202;

        return &InitFrame->CtxSwitchFrame;
    }
    else
    {
        PKKINIT_FRAME InitFrame = (PKKINIT_FRAME)((ULONG_PTR)Thread->InitialStack - sizeof(KKINIT_FRAME));
        RtlZeroMemory(InitFrame, sizeof(KKINIT_FRAME));

        Thread->TrapFrame = NULL;
        Thread->PreviousMode = KernelMode;

        // 1. Setup StartFrame
        InitFrame->StartFrame.SystemRoutine = SystemRoutine;
        InitFrame->StartFrame.StartRoutine = StartRoutine;
        InitFrame->StartFrame.StartContext = StartContext;
        InitFrame->StartFrame.UserThread = FALSE;

        // 2. Setup Context Switch Frame
        InitFrame->CtxSwitchFrame.SwapReturnAddress = (ULONG)PiThreadStartup;
        InitFrame->CtxSwitchFrame.EFlags = 0x202;

        return &InitFrame->CtxSwitchFrame;
    }
}

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
)
{
    /* 
     * Memanggil PtpCreateThread dengan:
     * - TargetProcess = NULL (karena akan fallback ke PtSystemProcess jika ProcessHandle NULL)
     * - ThreadContext = NULL (Tanda bahwa ini System Thread, bukan User Thread)
     * - InitialTeb     = NULL (System Thread tidak membutuhkan TEB)
     * - CreateSuspended = FALSE
     */
    return PtpCreateThread(
        ThreadHandle,
        DesiredAccess,
        ObjectAttributes,
        ProcessHandle,
        NULL,               // TargetProcess (Fallback ke PtSystemProcess)
        ClientId,
        NULL,               // ThreadContext (NULL = Kernel/System Thread)
        NULL,               // InitialTeb
        FALSE,              // CreateSuspended
        StartRoutine,
        StartContext
    );
}

PETHREAD
VEAPI
PtCreateSystemThreadForMainProcess(
    IN PKSTART_ROUTINE StartRoutine,
    IN PVOID StartContext,
    IN POOL_TYPE PoolType
)
{
    UNREFERENCED_PARAMETER(PoolType); // PoolType sudah dialokasikan otomatis oleh PtpInitThread

    HANDLE ThreadHandle;
    VEASTATUS Status;
    PETHREAD Thread = NULL;

    Status = PtCreateSystemThread(
        &ThreadHandle,
        THREAD_ALL_ACCESS,
        NULL,
        NULL,                               // ProcessHandle NULL -> default ke PtSystemProcess
        NULL,                               // ClientId
        StartRoutine,
        StartContext
    );

    if (VEA_SUCCESS(Status))
    {
        // Dapatkan pointer PETHREAD dari Handle
        Status = ObReferenceObjectByHandle(
            ThreadHandle,
            THREAD_ALL_ACCESS,
            PtEthreadType,
            KernelMode,
            (PVOID*)&Thread
        );

        // Tutup handle lokal kernel setelah pointer objek di-reference
        VeaClose(ThreadHandle);

        if (!VEA_SUCCESS(Status))
        {
            return NULL;
        }
    }

    return Thread;
}

VOID
VEAPI
PtInitializeHandBuildThread(
    IN PKTHREAD InitialThread, 
    IN PKPROCESS Process,
    IN PVOID StackBase, 
    IN ULONG StackSize, 
    IN PVOID StartAddress
)
{
    // 1. Setup Stack Boundaries
    InitialThread->InitialStack = (PVOID)((PUCHAR)StackBase + StackSize);
    InitialThread->StackLimit = StackBase;
    
    InitialThread->ContextSwitches = 0;
    InitialThread->Priority = 0;
    InitialThread->State = Running; 
    InitialThread->TrapFrame = NULL;
    InitialThread->Queue = NULL;
    InitialThread->ServiceTable = KsServiceTableDescriptor;
    InitializeListHead(&InitialThread->MutantListHead);
    InitialThread->Irql = PASSIVE_LEVEL;
    KsInitializeTimer(&InitialThread->Timer);
    InitializeListHead(&InitialThread->WaitListEntry);

    // 2. Attach ke Process
    InitialThread->ApcState.Process = Process;

    if (Process && IsListEmpty(&InitialThread->WaitListEntry))
    {
        InsertTailList(&Process->ThreadListHead, &InitialThread->WaitListEntry);
    }

    PtpInitializeThreadFpuState(InitialThread);

    // 3. Setup Context Switch Frame
    if (StartAddress != NULL) 
    {
        InitialThread->KernelStack = PtpSetupInitialThreadContext(
            InitialThread,                      // <-- Oper PKTHREAD di sini!
            NULL,                               // No User Stack
            (PVOID)PtpSystemThreadStartup,      // SystemRoutine Wrapper
            (PVOID)StartAddress,                // StartRoutine Target
            NULL,                               // No StartContext
            KernelMode
        );
    } 
    else 
    {
        InitialThread->KernelStack = InitialThread->InitialStack;
    }
}

VOID
VEAPI
PtpReaperThreadRoutine(
    PVOID Context
)
{
    UNREFERENCED_PARAMETER(Context);

    KeLowerIrql(PASSIVE_LEVEL);
    DPRINT("PtpReaperThreadRoutine()\n");

    for (;;)
    {
        ULONG_PTR Flags = PtpSaveAndDisableInterrupts();

        if (!IsListEmpty(&PtDeadThreadListHead))
        {
            // Ambil thread paling depan dari list mati
            PLIST_ENTRY Entry = RemoveHeadList(&PtDeadThreadListHead);
            PETHREAD DeadThread = CONTAINING_RECORD(Entry, ETHREAD, ThreadListEntry);

            PtpRestoreInterrupts(Flags);

            DPRINT("Freeing DeadThread %p\n", DeadThread);

            // Bebaskan Kernel Stack (Aman karena CPU udah gak pake stack ini)
            if (DeadThread->Tcb.StackLimit)
            {
                MmDeleteKernelStack(DeadThread->Tcb.InitialStack, DeadThread->Tcb.LargeStack);
                DeadThread->Tcb.StackLimit = NULL;
                DeadThread->Tcb.KernelStack = NULL;
            }

            // Hancurkan / dereference object ETHREAD
            ObDereferenceObject(DeadThread);
        }
        else
        {
            PtpRestoreInterrupts(Flags);

            // Kalau list kosong, mengalah dulu biar nggak bikin CPU 100%
            PtYieldThread();
        }
    }
}

VEASTATUS
VEAPI
PtpInitThread(
    IN OUT PKTHREAD Thread,
    IN PVOID Environment OPTIONAL,
    IN PKSYSTEM_ROUTINE SystemRoutine,
    IN PKSTART_ROUTINE StartRoutine OPTIONAL,
    IN PVOID StartContext OPTIONAL,
    IN PKTRAP_FRAME TrapFrame OPTIONAL,
    IN PTEB Teb OPTIONAL,
    IN PKPROCESS Process
)
{
    PVOID StackBase;
    PVOID InitialStack;
    PVOID StackLimit;
    PVOID KernelStack;
    KPROCESSOR_MODE Mode;
    UCHAR Prio;

    UNREFERENCED_PARAMETER(Environment);

    //
    // 1. Alokasikan Kernel Stack standar (12 KB + 1 Guard Page unmapped di dasar stack)
    //
    InitialStack = MmCreateKernelStack(FALSE, 0);
    if (!InitialStack)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    StackLimit = (PVOID)((ULONG_PTR)InitialStack - KERNEL_STACK_SIZE);

    //
    // 2. Inisialisasi struktur dasar KTHREAD (List, Spinlock, APC State, IRQL)
    //
    PiInitializeThread(
        Thread,
        NULL, // KernelStack diisi setelah Context Setup
        InitialStack,
        StackLimit,
        Process->BasePriority
    );

    //
    // 3. Attach Thread ke Objek Proses Induk & Atur Service Table
    //
    Thread->ApcState.Process = Process;
    Thread->ServiceTable = KsServiceTableDescriptor;
    Thread->Teb = (PVOID)Teb;
    
    Prio = Process->BasePriority;
    if (Prio > 31) Prio = 31;

    Thread->Weight = PtpPriorityToWeight[Prio];
    Thread->VRuntime = 0;
    KsInitializeTimer(&Thread->Timer);

    //
    // 4. Rakit Stack Context Frame (UserMode vs KernelMode)
    //
    if (TrapFrame != NULL)
    {
        // Thread User Mode (Dibuat via NtCreateThreadEx / PtpCreateThread dengan Context)
        Mode = UserMode;

        /* FIX: Oper 6 parameter lengkap termasuk SystemRoutine (PtpUserThreadStartup) */
        KernelStack = PtpSetupInitialThreadContext(
            Thread,
            (PVOID)TrapFrame->HardwareEsp,
            (PVOID)SystemRoutine,             // PtpUserThreadStartup
            (PVOID)TrapFrame->Eip,            // EIP User Mode (Entry point)
            StartContext,                     // Argument / Context
            Mode
        );
    }
    else
    {
        // Thread Kernel Mode (System Thread)
        Mode = KernelMode;

        /* FIX: Oper 6 parameter lengkap untuk System Thread */
        KernelStack = PtpSetupInitialThreadContext(
            Thread,
            NULL,
            (PVOID)SystemRoutine,             // PtpSystemThreadStartup
            (PVOID)StartRoutine,              // Routine asli (misal UlpWorkerEntryPoint)
            StartContext,                     // Context (CriticalWorkQueue, DelayedWorkQueue, dll)
            Mode
        );

        Thread->TrapFrame = NULL;
    }

    Thread->KernelStack = KernelStack;
    return STATUS_SUCCESS;
}

VOID
VEAPI
PtUninitThread(
    IN PKTHREAD Thread
)
{
    if (!Thread) return;

    //
    // Bebaskan Kernel Stack jika sudah sempat dialokasikan
    //
    if (Thread->StackLimit)
    {
        UlFreePool(Thread->StackLimit);
        Thread->StackLimit = NULL;
        Thread->InitialStack = NULL;
        Thread->KernelStack = NULL;
    }

    Thread->State = Terminated;
    Thread->ApcState.Process = NULL;
}

VOID
VEAPI
PtpStartThread(
    IN PKTHREAD Thread
)
{
    if (!Thread) return;

    //
    // Hitung ulang Weight CFS berdasarkan Priority KTHREAD
    //
    PtSetThreadPriority(Thread, Thread->Priority);

    //
    // Set status thread menjadi Initialized / Ready
    //
    if (Thread->State != Running)
    {
        Thread->State = Ready;
    }
}

VOID
VEAPI
PtpReadyThread(
    IN PKTHREAD Thread
)
{
    PKPRCB Prcb;

    if (!Thread) return;

    //
    // Ambil RunQueue CPU yang mengeksekusi instruksi ini dan Enqueue ke CFS Tree
    //
    Prcb = KeGetCurrentPrcb();
    PtEnqueueThreadCfs(&PtRunQueue[Prcb->Number], Thread);
}

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
)
// +++
//
// Routine Description:
// Creatig new Thread that attached to Process, then start the Thread afterward or even 
// suspend it according to BOOLEAN CreateSuspended
// 
// Parameters:
// PHANDLE ThreadHandle: 
//      Parameter to returning created Thread Handle
// 
// ACCESS_MASK DesiredAccess:
//      DesiredAccess for step in creating Thread
// 
// POBJECT_ATTRIBUTES ObjAttr:
//      Pointer to Object Attribute describing Attribute for Thread Object
// 
// HANDLE ProcessHandle:
//      Handle pointing toward process
// 
// PEPROCESS TargetProcess:
//      Pointer to EPROCESS in memory
// 
// PCLIENT_ID ClientId:
//      Pointer out to Client ID parameter
// 
// PKTRAP_FRAME ThreadContext:
//      Thread Context to start with
// 
// PINITIAL_TEB InitialTeb:
//      Pointer to Initial Teb 
// 
// BOOLEAN CreateSuspended:
//      Suspend the Thread after creating it
// 
// PKSTART_ROUTINE StartRoutine:
//      StartRoutine to run after Thread started
// 
// PVOID StartContext:
//      Context that will passed to Thread after Thread has running
// 
// Return VEASTATUS:
// STATUS_SUCCESS:
//      Thread successfully Created
// 
// STATUS_INVALID_HANDLE:
//      Handle were invalid for Thread making
// 
// STATUS_INSUFFICIENT_RESOURCES:
//      Memory/Resource were run out for creating Handle or allocating memory
// 
// STATUS_PROCESS_IS_TERMINATING:
//      Passing Process that gonna terminating soon
// 
// +++
{
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();
    VEASTATUS Status;
    PEPROCESS Process;
    PETHREAD Thread;
    HANDLE_TABLE_ENTRY CidEntry;
    PTEB TebBase;
    ACCESS_STATE LocalAccessState;
    PACCESS_STATE AccessState = &LocalAccessState;
    AUX_ACCESS_DATA AuxData;
    HANDLE hThread;

    //
    // The one who actually call with StartRoutine is only kernel. So set PreviousMode
    // to kernel to grant ALL ACCESS freely
    //

    if (StartRoutine) PreviousMode = KernelMode;

    //
    // Should be mentioned with handle to process because Thread needs to be linked and
    // attached to process. reference the process with ObReferenceObjectByHandle. If no
    // ProcessHandle, the argument should pass TargetProcess instead to attach Thread
    // to some Thread (this is called attaching to another). Make sure UserMode didn't 
    // create System Thread and also not trying to attaching to Kernel Process
    //

    if (ProcessHandle)
    {
        Status = ObReferenceObjectByHandle(
            ProcessHandle,
            PROCESS_CREATE_THREAD,
            PtEprocessType,
            PreviousMode,
            (PVOID*)&Process
        );
    }
    else
    {
        if (StartRoutine)
        {
            if (!TargetProcess) TargetProcess = PtSystemProcess;

            ObReferenceObject(TargetProcess);
            Process = TargetProcess;
            Status = STATUS_SUCCESS;
        }
        else
        {
            Status = STATUS_INVALID_HANDLE;
        }
    }

    if (!VEA_SUCCESS(Status)) return Status;

    if ((PreviousMode) != KernelMode && (Process == PtSystemProcess))
    {
        ObDereferenceObject(Process);
        return STATUS_INVALID_HANDLE; 
    }

    //
    // Create the Thread Object, and then Initialize them. Set Exit Status to PENDING
    // cause we wait for the Thread to exit and then give us the Real Status. Initialize
    // CID alias Client Id for tracking Thread ID. Make sure ID given or if not, just 
    // return
    //

    Status = ObCreateObject(
        PreviousMode,
        PtEthreadType,
        ObjAttr,
        PreviousMode,
        NULL,
        sizeof(ETHREAD),
        0,
        0,
        (PVOID*)&Thread
    );

    if (!VEA_SUCCESS(Status))
    {
        ObDereferenceObject(Process);
        return Status;
    }

    RtlZeroMemory(Thread, sizeof(*Thread));

    // TODO:
    // ReactOS has Rundown Protection and then initialized here

    Thread->ExitStatus = STATUS_PENDING;
    Thread->ThreadProcess = Process;
    Thread->Cid.UniqueProcess = Process->UniqueProcessId;

    CidEntry.Object = Thread;
    CidEntry.GrantedAccess = 0;
    Thread->Cid.UniqueThread = UlCreateHandle(PtpCidTable, &CidEntry);
    if (!Thread->Cid.UniqueThread)
    {
        ObDereferenceObject(Thread);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // FIXME:
    // Thread->ReadClusterSize is 0 in ReactOS. no need to implement them.

    KsInitializeSemaphore(&Thread->KpcReplySemaphore, 0, 1);
    InitializeListHead(&Thread->IrpList);
    InitializeListHead(&Thread->PostBlockList);
    InitializeListHead(&Thread->ActiveTimerListHead);
    KsInitializeSpinlock(&Thread->ActiveTimerListLock);

    // TODO:
    // Implement Acquire Rundown Protection

    //
    // Now all ETHREAD struct has ready and now we can adding Context for the TCB block
    // alias KTHREAD itself. Initializing TEB context for the Thread itself. I dont know
    // how it's work but somehow Thread marked as DeleteThread and we need to end it 
    // anyway. But make sense if the Process->Settings.DeleteProcess was turned on because
    // the Process is gonna deleted but at some condition a race happened
    //

    if (ThreadContext)
    {
        Status = MmCreateTeb(Process, &Thread->Cid, InitialTeb, &TebBase);
        if (!VEA_SUCCESS(Status))
        {
            // TODO:
            // UlReleaseRundownPrtotection()
            ObDereferenceObject(Thread);
            return Status;
        }

        _VEA_TRY
        {
            Thread->StartAddress = (PVOID)ThreadContext->Eip;
            Thread->VEAStartAddres = (PVOID)ThreadContext->Eax;
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            Status = GetExceptionCode();
        }
        _VEA_END;

        if (VEA_SUCCESS(Status))
        {
            Status = PtpInitThread(
                &Thread->Tcb,
                NULL,
                PtpUserThreadStartup,
                NULL,
                Thread->StartAddress,
                ThreadContext,
                TebBase,
                &Process->Pcb
            );
        }
    }
    else
    {
        Thread->StartAddress = StartRoutine;

        // TODO:
        // PtpSetCrossThreadFlag(Thread, CT_SYSTEM_THREAD_BIT);

        Status = PtpInitThread(
            &Thread->Tcb,
            NULL,
            PtpSystemThreadStartup,
            StartRoutine,
            StartContext,
            NULL,
            NULL,
            &Process->Pcb
        );
    }

    if (!VEA_SUCCESS(Status))
    {
        if (TebBase) MmDeleteTeb(Process, TebBase);

        // TODO:
        // ExReleaseRundownProtection()
        
        ObDereferenceObject(Thread);
        return Status;
    }

    KsEnterCriticalRegion();

    if (Process->Settings.ProcessDelete) goto QuickWayHereBro;

    if ((Thread->Settings.ThreadDelete) &&
        (ThreadContext) && 
        (Thread->ThreadProcess == Process))
    {
        goto QuickWayHereBro;
    }

    //
    // Now it's all safe. APC must not be interrupting or delivering and then we can insert 
    // out thread to Process List Entry. And then Thread can be started from here
    //

    InsertTailList(&Process->ThreadListHead, &Thread->ThreadListEntry);
    Process->ActiveThreads++;

    PtpStartThread(&Thread->Tcb);

    KsLeaveCriticalRegion();

    // TODO:
    // Call Pt Routine Callback

    ObReferenceObject(Thread);
    ObReferenceObject(Thread);

    if (CreateSuspended)
    {
        Thread->Tcb.SuspendCount = 1;
        Thread->Tcb.State = Waiting; 
    }
    else
    {
        Thread->Tcb.SuspendCount = 0;
        PtpReadyThread(&Thread->Tcb);
    }

    // TODO:
    // Implement this
    Status = SfCreateAccessStateEx(NULL,
                                   ThreadContext ?
                                   (PEPROCESS)PtGetCurrentProcess() : Process,
                                   &LocalAccessState,
                                   &AuxData,
                                   DesiredAccess,
                                   &PtEthreadType->GenericMapping);
    if (!VEA_SUCCESS(Status))
    {
        ObDereferenceObject(Thread);
        return Status;
    }

    //
    // Can the Thread actually inserted with ObInsertObject although it doesn't have name?
    //

    Status = ObInsertObject(
        Thread,
        AccessState,
        DesiredAccess,
        0,
        NULL,
        &hThread
    );

    SfDeleteAccessState(&LocalAccessState);

    if (VEA_SUCCESS(Status))
    {
        _VEA_TRY
        {
            if (ClientId) *ClientId = Thread->Cid;
            *ThreadHandle = hThread;
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            // TODO:
            // Implement this
            /*
             * 
             *
            PspSetCrossThreadFlag(Thread, CT_DEAD_THREAD_BIT);

            if (CreateSuspended) KeResumeThread(&Thread->Tcb);

            KeReadyThread(&Thread->Tcb);

            ObDereferenceObject(Thread);

            ObCloseHandle(hThread, PreviousMode);
             */

            _VEA_YIELD(return GetExceptionCode());
        }
        _VEA_END;
    }
    else
    {
        // TODO:
        // Implement this
        /*
         *  PspSetCrossThreadFlag(Thread, CT_DEAD_THREAD_BIT);
         *  if (CreateSuspended) KeResumeThread(&Thread->Tcb);
         */
    }

    Thread->CreateTime.QuadPart = KsQuerySystemTime();

    //SObDereferenceObject(Thread);

    return Status;

QuickWayHereBro:
    KsLeaveCriticalRegion();

    PtUninitThread(&Thread->Tcb);

    if (TebBase) MmDeleteTeb(Process, TebBase);

    ObDereferenceObject(Thread);
    return STATUS_PROCESS_IS_TERMINATING;
}

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
)
{
    VEASTATUS Status = STATUS_SUCCESS;
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();
    HANDLE LocalThreadHandle = NULL;
    CLIENT_ID LocalClientId;
    OBJECT_ATTRIBUTES LocalObjectAttributes;
    POBJECT_ATTRIBUTES CapturedObjectAttributes = NULL;
    KTRAP_FRAME LocalTrapFrame;
    INITIAL_TEB LocalInitialTeb;

    RtlZeroMemory(&LocalClientId, sizeof(CLIENT_ID));
    RtlZeroMemory(&LocalTrapFrame, sizeof(KTRAP_FRAME));
    RtlZeroMemory(&LocalInitialTeb, sizeof(INITIAL_TEB));

    //
    // 1. VALIDASI POINTER & CAPTURE DATA USER MODE VIA SEH
    //
    if (PreviousMode == UserMode)
    {
        // Validasi Alamat Pointer Wajib (< 0x80000000)
        if (!ThreadHandle || (ULONG_PTR)ThreadHandle >= 0x80000000)
        {
            return STATUS_ACCESS_VIOLATION;
        }

        if (!ThreadContext || (ULONG_PTR)ThreadContext >= 0x80000000)
        {
            return STATUS_ACCESS_VIOLATION;
        }

        if (!InitialTeb || (ULONG_PTR)InitialTeb >= 0x80000000)
        {
            return STATUS_ACCESS_VIOLATION;
        }

        _VEA_TRY
        {
            // Probe Write Access untuk ThreadHandle
            *(volatile HANDLE*)ThreadHandle = *(volatile HANDLE*)ThreadHandle;

            // Probe ClientId jika disuplai
            if (ClientId)
            {
                if ((ULONG_PTR)ClientId >= 0x80000000)
                {
                    return STATUS_ACCESS_VIOLATION;
                }
                *(volatile CLIENT_ID*)ClientId = *(volatile CLIENT_ID*)ClientId;
            }

            // Capture ObjectAttributes
            if (ObjectAttributes)
            {
                if ((ULONG_PTR)ObjectAttributes >= 0x80000000)
                {
                    return STATUS_ACCESS_VIOLATION;
                }
                RtlCopyMemory(&LocalObjectAttributes, ObjectAttributes, sizeof(OBJECT_ATTRIBUTES));
                CapturedObjectAttributes = &LocalObjectAttributes;
            }

            // Direct Copy KTRAP_FRAME dari User Space ke Kernel Stack
            RtlCopyMemory(&LocalTrapFrame, ThreadContext, sizeof(KTRAP_FRAME));

            // Sanitasi Selector Segment Ring 3 (Pastikan RPL = 3 agar tidak GPF)
            if (LocalTrapFrame.SegCs == 0) LocalTrapFrame.SegCs = KGDT32_R3_CODE;
            LocalTrapFrame.SegCs |= 3;

            if (LocalTrapFrame.HardwareSegSs == 0) LocalTrapFrame.HardwareSegSs = KGDT32_R3_DATA;
            LocalTrapFrame.HardwareSegSs |= 3;

            if (LocalTrapFrame.SegDs == 0) LocalTrapFrame.SegDs = KGDT32_R3_DATA;
            LocalTrapFrame.SegDs |= 3;

            if (LocalTrapFrame.SegEs == 0) LocalTrapFrame.SegEs = KGDT32_R3_DATA;
            LocalTrapFrame.SegEs |= 3;

            // Pastikan Interrupt Flag (IF=1) ON di EFlags
            LocalTrapFrame.EFlags |= 0x200;

            // Capture InitialTeb (StackBase & StackLimit User Stack)
            RtlCopyMemory(&LocalInitialTeb, InitialTeb, sizeof(INITIAL_TEB));
        }
        _VEA_EXCEPT(1)
        {
            return STATUS_ACCESS_VIOLATION;
        }
        _VEA_END
    }
    else
    {
        // Panggilan dari Kernel Mode (Direct Access)
        CapturedObjectAttributes = ObjectAttributes;

        if (ThreadContext)
        {
            RtlCopyMemory(&LocalTrapFrame, ThreadContext, sizeof(KTRAP_FRAME));
            LocalTrapFrame.SegCs |= 3;
            LocalTrapFrame.HardwareSegSs |= 3;
            LocalTrapFrame.EFlags |= 0x200;
        }

        if (InitialTeb)
        {
            LocalInitialTeb = *InitialTeb;
        }
    }

    //
    // 2. EKSEKUSI ENGINE UTAMA (PtpCreateThread)
    //
    Status = PtpCreateThread(
        &LocalThreadHandle,
        DesiredAccess,
        CapturedObjectAttributes,
        ProcessHandle,
        NULL,                                      // TargetProcess (Resolved via ProcessHandle)
        &LocalClientId,
        ThreadContext ? &LocalTrapFrame : NULL,    // Pass Direct KTRAP_FRAME Pointer
        InitialTeb ? &LocalInitialTeb : NULL,      // INITIAL_TEB
        CreateSuspended,
        NULL,                                      // StartRoutine NULL untuk User Thread
        NULL                                       // StartContext NULL
    );

    //
    // 3. TULIS KEMBALI RESULT (ThreadHandle & ClientId) KE USER SPACE
    //
    if (VEA_SUCCESS(Status))
    {
        if (PreviousMode == UserMode)
        {
            _VEA_TRY
            {
                *ThreadHandle = LocalThreadHandle;
                if (ClientId)
                {
                    *ClientId = LocalClientId;
                }
            }
            _VEA_EXCEPT(1)
            {
                // Proteksi Kebocoran Handle (Rollback Cleanup)
                ObCloseHandle(LocalThreadHandle, UserMode);
                Status = STATUS_ACCESS_VIOLATION;
            }
            _VEA_END
        }
        else
        {
            *ThreadHandle = LocalThreadHandle;
            if (ClientId)
            {
                *ClientId = LocalClientId;
            }
        }
    }

    return Status;
}

ULONG
VEAPI
PtResumeThread(
    IN PKTHREAD Thread,
    OUT PULONG PreviousSuspendCount OPTIONAL
)
{
    ULONG PreviousCount;
    ULONG_PTR Flags;

    if (!Thread) return 0;

    Flags = PtpSaveAndDisableInterrupts();

    PreviousCount = Thread->SuspendCount;

    if (Thread->SuspendCount > 0)
    {
        Thread->SuspendCount--;

        /* Jika SuspendCount menyentuh angka 0, langsung masukkan thread ke CFS RunQueue! */
        if (Thread->SuspendCount == 0)
        {
            PtpReadyThread(Thread);
        }
    }

    PtpRestoreInterrupts(Flags);

    if (PreviousSuspendCount)
    {
        *PreviousSuspendCount = PreviousCount;
    }

    return PreviousCount;
}

VEASTATUS
VEAPI
PtpTerminateThread(
    IN PETHREAD Thread,
    IN VEASTATUS ExitStatus,
    IN BOOLEAN Direct
)
{
    PKAPC Apc;
    PETHREAD CurrentThread = CONTAINING_RECORD(PtGetCurrentThread(), ETHREAD, Tcb);

    if (!Thread)
    {
        return STATUS_INVALID_PARAMETER;
    }

    /* Cek apakah thread ini critical (BreakOnTermination) */
    PtpCatchCriticalBreak(Thread, FALSE, ExitStatus);

    /* Tandai DeadThread agar tidak menerima APC baru atau batal saat startup */
    Thread->Settings.DeadThread = TRUE;

    /* Jika mematikan diri sendiri (atau dipanggil langsung dari thread ybs) */
    if (Direct || Thread == CurrentThread)
    {
        PtpExitThread(ExitStatus);
        /* Unreachable */
        return STATUS_SUCCESS;
    }

    /* Jangan inject APC ke System Thread kernel */
    if (Thread->Tcb.PreviousMode == KernelMode)
    {
        return STATUS_ACCESS_DENIED;
    }

    /* Alokasi Special Kernel APC untuk memaksa thread target memanggil PtpExitThread */
    Apc = (PKAPC)UlAllocatePoolWithTag(NonPagedPool, sizeof(KAPC), 'cpAP');
    if (!Apc)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    KsInitializeApc(
        Apc,
        &Thread->Tcb,
        OriginalApcEnvironment,
        PtpTerminateThreadApc,
        NULL,
        NULL,
        KernelMode,
        NULL
    );

    /* Pastikan thread tidak tertahan dalam kondisi Suspended */
    while (Thread->Tcb.SuspendCount > 0)
    {
        PtResumeThread(&Thread->Tcb, NULL);
    }

    if (!KsInsertQueueApc(Apc, (PVOID)(ULONG_PTR)ExitStatus, NULL))
    {
        /* Thread sudah terlanjur Terminated */
        UlFreePool(Apc);
        return STATUS_THREAD_IS_TERMINATING;
    }

    /* Bangunkan thread jika sedang tidur di Wait / DelayExecution */
    PtWakeThreadEx(&Thread->Tcb, STATUS_ALERTED);

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
VeaResumeThread(
    IN HANDLE ThreadHandle,
    OUT PULONG PreviousSuspendCount OPTIONAL
)
{
    VEASTATUS Status;
    PETHREAD Thread;
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();
    ULONG LocalPreviousCount = 0;

    Status = ObReferenceObjectByHandle(
        ThreadHandle,
        THREAD_SUSPEND_RESUME,
        PtEthreadType,
        PreviousMode,
        (PVOID*)&Thread
    );

    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    LocalPreviousCount = PtResumeThread(&Thread->Tcb, NULL);

    if (PreviousSuspendCount)
    {
        if (PreviousMode == UserMode)
        {
            _VEA_TRY
            {
                *PreviousSuspendCount = LocalPreviousCount;
            }
            _VEA_EXCEPT(1)
            {
                Status = STATUS_ACCESS_VIOLATION;
            }
            _VEA_END
        }
        else
        {
            *PreviousSuspendCount = LocalPreviousCount;
        }
    }

    ObDereferenceObject(Thread);
    return Status;
}

VEASTATUS
VEAPI
VeaTerminateThread(
    IN HANDLE ThreadHandle,
    IN VEASTATUS ExitStatus
)
{
    VEASTATUS Status;
    PETHREAD Thread;
    PETHREAD CurrentThread = CONTAINING_RECORD(PtGetCurrentThread(), ETHREAD, Tcb);
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();

    /* Di ReactOS/Windows, ThreadHandle == NULL dianggap invalid parameter */
    if (!ThreadHandle)
    {
        return STATUS_INVALID_PARAMETER;
    }

    /* Handle pseudo-handle NtCurrentThread() / (HANDLE)-2 */
    if (ThreadHandle == NtCurrentThread())
    {
        Thread = CurrentThread;
        ObReferenceObject(Thread);
    }
    else
    {
        Status = ObReferenceObjectByHandle(
            ThreadHandle,
            THREAD_TERMINATE,
            PtEthreadType,
            PreviousMode,
            (PVOID*)&Thread
        );
        if (!VEA_SUCCESS(Status))
        {
            return Status;
        }
    }

    /* Proteksi: UserMode tidak boleh mematikan thread milik PtSystemProcess */
    if (PreviousMode != KernelMode && Thread->ThreadProcess == PtSystemProcess)
    {
        ObDereferenceObject(Thread);
        return STATUS_ACCESS_DENIED;
    }

    if (Thread == CurrentThread)
    {
        /* Drop reference sebelum exit karena PtpExitThread tidak pernah return */
        ObDereferenceObject(Thread);
        return PtpTerminateThread(Thread, ExitStatus, TRUE);
    }

    Status = PtpTerminateThread(Thread, ExitStatus, FALSE);
    ObDereferenceObject(Thread);
    return Status;
}

/* EOF */