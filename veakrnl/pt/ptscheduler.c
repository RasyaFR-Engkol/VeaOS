#include <veakrnl.h>

#define PT_CFS_BASE_WEIGHT 1024UL

KRUNQUEUE PtRunQueue[MAX_CPU];

VOID 
VEAPI 
PtSwapContext(
    PKTHREAD OldThread, 
    PKTHREAD NewThread
);

PKTHREAD
VEAPI
PtPickNextThreadCfs(PKRUNQUEUE RunQueue);

VOID
VEAPI
PtDequeueThreadCfs(
    PKRUNQUEUE RunQueue, 
    PKTHREAD Thread
);

VOID
VEAPI
PtEnqueueThreadCfs(
    PKRUNQUEUE RunQueue, 
    PKTHREAD Thread
);

FORCEINLINE
ULONG_PTR
PtpSilentInterrupt(VOID)
{
    ULONG_PTR Flags = __ReadEFlags();
    __Stop();
    return Flags;
}

static 
VOID
PtpRestoreInterrupt(ULONG_PTR Eflags)
{
    __WriteEFlags(Eflags);
}

VOID
VEAPI
PtInitializeRunQueues(VOID)
{
    for (ULONG i = 0; i < MAX_CPU; i++)
    {
        RtlZeroMemory(&PtRunQueue[i], sizeof(KRUNQUEUE));
        RtlInitializeRbTree(&PtRunQueue[i].CfsTree, PtCfsCompareRoutine);
        KsInitializeSpinlock(&PtRunQueue[i].SpinLock);

        PtRunQueue[i].RunningTasks = 0;
        PtRunQueue[i].TotalWeight = 0;
        PtRunQueue[i].CurrentThread = NULL;

        // -------------------------------------------------------------
        // FIX: Inisialisasi KTHREAD Idle Thread secara utuh, bukan zero-mem doang!
        // -------------------------------------------------------------
        PiInitializeThread(
            &PtSystemIdleThread[i],
            NULL,
            NULL,
            NULL,
            0
        );

        PtSystemIdleThread[i].State = Ready;
        PtSystemIdleThread[i].Weight = 1;
        PtSystemIdleThread[i].VRuntime = 0;
    }

    PKPRCB Prcb = KeGetCurrentPrcb();
    ULONG CurrentCpu = Prcb->Number;

    PtSystemIdleThread[CurrentCpu].State = Running; 

    Prcb->CurrentThread = (struct _KTHREAD *)&PtSystemIdleThread[CurrentCpu];
    PtRunQueue[CurrentCpu].CurrentThread = &PtSystemIdleThread[CurrentCpu];
}

PKTHREAD
VEAPI
PtSelectNextThread(PKRUNQUEUE RunQueue)
{
    PKTHREAD NextThread = PtPickNextThreadCfs(RunQueue);

    if (!NextThread)
    {
        UCHAR CpuId = KeGetCurrentProcessorNumber();
        NextThread = &PtSystemIdleThread[CpuId];
    }

    NextThread->State = Running;
    RunQueue->CurrentThread = NextThread;

    return NextThread;
}

VOID
VEAPI
PtYieldThread(VOID)
{
    KIRQL OldIrql = KsAcquireDispatcherLock();
    PKPRCB Prcb = KeGetCurrentPrcb();
    PKTHREAD CurrentThread = (PKTHREAD)Prcb->CurrentThread;

    if (CurrentThread)
    {
        CurrentThread->WaitIrql = OldIrql;
        PtSwapThread(CurrentThread, Prcb);
    }
    else
    {
        KsReleaseDispatcherLock(OldIrql);
    }
}

BOOLEAN
VEAPI
PtWakeThreadEx(
    PKTHREAD Thread,
    VEASTATUS WakeStatus
)
{
    Thread->WaitStatus = WakeStatus;
    return PtWakeThread(Thread);
}

BOOLEAN
VEAPI
PtWakeThread(
    PKTHREAD Thread
)
{
    PKPRCB Prcb;
    ULONG_PTR Flags;

    if (!Thread)
        return FALSE;

    Flags = PtpSaveAndDisableInterrupts();

    if (Thread->State != Waiting ||
        (Thread->ApcState.Process && Thread->ApcState.Process->DirectoryTableBase == 0))
    {
        PtpRestoreInterrupts(Flags);
        return FALSE;
    }

    Thread->State = Ready;

    Prcb = KeGetCurrentPrcb();
    PtEnqueueThreadCfs(&PtRunQueue[Prcb->Number], Thread);

    PtpRestoreInterrupts(Flags);
    return TRUE;
}

static
VOID
VEAPI
PtpDelayExecutionDpcRoutine(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
)
{
    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    PKTHREAD Thread = (PKTHREAD)DeferredContext;

    if (Thread && Thread->State == Waiting)
    {
        // Set status sukses timeout lalu masukkan kembali thread ke CFS RunQueue
        Thread->WaitStatus = STATUS_TIMEOUT;
        PtWakeThread(Thread);
    }
}

VOID
VEAPI
PtSchedulerTick(ULONGLONG DeltaTimeNs)
{
    PKPRCB Prcb = KeGetCurrentPrcb();
    PKTHREAD CurrentThread = (PKTHREAD)Prcb->CurrentThread;
    PKRUNQUEUE RunQueue = &PtRunQueue[Prcb->Number];
    KIRQL OldIrql;
    BOOLEAN ShouldYield = FALSE;
    ULONG_PTR Eflags;

    static HANDLE LastTid = NULL;

    if (!CurrentThread) return;

    PETHREAD EThread = CONTAINING_RECORD(CurrentThread, ETHREAD, Tcb);
    HANDLE CurrentPid = EThread->Cid.UniqueProcess;
    HANDLE CurrentTid = EThread->Cid.UniqueThread;

    if (CurrentTid != LastTid)
    {
        PEPROCESS Process = EThread->ThreadProcess;
        PCHAR ProcName = (Process && Process->ImageFileName[0]) ? Process->ImageFileName : "UserProcess(NoName)";

        LastTid = CurrentTid;
    }

    if (CurrentThread->Weight != 0)
    {
        ULONG WeightShift = 0;
        ULONGLONG Weight = CurrentThread->Weight;

        while (Weight > PT_CFS_BASE_WEIGHT && WeightShift < 5)
        {
            Weight >>= 1;
            WeightShift++;
        }

        CurrentThread->VRuntime += DeltaTimeNs >> WeightShift;
    }

    Eflags = PtpSaveAndDisableInterrupts();

    PRTL_BALANCED_NODE Leftmost = RtlGetLeftmostNodeRb(&RunQueue->CfsTree);
    if (Leftmost)
    {
        PKTHREAD Candidate = CONTAINING_RECORD(Leftmost, KTHREAD, CfsNode);

        if (Candidate->VRuntime < CurrentThread->VRuntime)
        {
            ShouldYield = TRUE;
        }
    }

    PtpRestoreInterrupt(Eflags);

    if (ShouldYield)
    {
        PtYieldThread();
    }
}

VEASTATUS
VEAPI
KsDelayExecutionThread(
    IN KPROCESSOR_MODE WaitMode,
    IN BOOLEAN Alertable,
    IN PLARGE_INTEGER Interval
)
{
    PKTHREAD CurrentThread;
    ULONG_PTR Flags;

    if (!Interval) {
        return STATUS_INVALID_PARAMETER;
    }

    CurrentThread = PtGetCurrentThread();
    if (!CurrentThread) {
        return STATUS_UNSUCCESSFUL;
    }

    // Amankan status interrupt
    Flags = __ReadEFlags();
    __Stop();

    CurrentThread->WaitMode = WaitMode;
    CurrentThread->Alertable = Alertable;
    CurrentThread->WaitStatus = STATUS_PENDING;
    CurrentThread->State = Waiting;

    KsInitializeDpc(&CurrentThread->TimerDpc, PtpDelayExecutionDpcRoutine, (PVOID)CurrentThread);

    KsSetTimer(&CurrentThread->Timer, *Interval, 0, &CurrentThread->TimerDpc);

    PtYieldThread();

    __WriteEFlags(Flags);

    return CurrentThread->WaitStatus;
}

VEASTATUS
VEAPI
VeaYieldExecution(VOID)
{
    PtYieldThread();

    return STATUS_SUCCESS;
}

LONG_PTR
VEAFAST
PtSwapThread(
    IN PKTHREAD Thread,
    IN PKPRCB Prcb
)
{
    KIRQL WaitIrql = Thread->WaitIrql;
    ULONG_PTR Flags = PtpSilentInterrupt();
    PKRUNQUEUE RunQueue = &PtRunQueue[Prcb->Number];
    PKTHREAD IdleThread = &PtSystemIdleThread[Prcb->Number];

    /*
     * Lepas spinlock dispatcher di DISPATCH_LEVEL (tanpa turun ke PASSIVE_LEVEL)
     * supaya thread baru di PiThreadStartup tidak deadlock, sementara IRQ/DPC
     * tetap tertahan sampai PtSwapContext selesai.
     */
    KsReleaseDispatcherLock(DISPATCH_LEVEL);

    /*
     * Enqueue Thread HANYA jika State masih Running (voluntary yield / preemption),
     * dan lakukan SEBELUM PtSelectNextThread memanggil PtPickNextThreadCfs.
     */
    if (Thread && Thread != IdleThread && Thread->State == Running)
    {
        PtEnqueueThreadCfs(RunQueue, Thread);
    }

    PKTHREAD NextThread = PtSelectNextThread(RunQueue);

    if (Thread != NextThread)
    {
        PtSetCurrentThread(NextThread);

        PKPCR Pcr = KeGetPcr();
        if (Pcr && Pcr->TSS)
        {
            ULONG_PTR KernelStackTop = (ULONG_PTR)NextThread->InitialStack;
            ((PKTSSENTRY32)Pcr->TSS)->Esp0 = (ULONG)KernelStackTop;
            __WriteMSR(MSR_IA32_SYSENTER_ESP, KernelStackTop);
        }

        PKPROCESS OldProcess = Thread ? Thread->ApcState.Process : NULL;
        PKPROCESS NewProcess = NextThread ? NextThread->ApcState.Process : NULL;

        if (OldProcess != NewProcess && NewProcess != NULL)
        {
            __WriteCR3(NewProcess->DirectoryTableBase);
        }

        PtSwapContext(Thread, NextThread);
    }

    /* Eksekusi lanjut di sini setelah Thread di-wake dan dijadwalkan balik */
    KeLowerIrql(WaitIrql);
    PtpRestoreInterrupts(Flags);

    if (WaitIrql == PASSIVE_LEVEL &&
        Thread->ApcState.KernelApcPending &&
        !Thread->SpecialApcDisable)
    {
        HctRequestSoftwareInterrupt(APC_LEVEL);
    }

    return (LONG_PTR)Thread->WaitStatus;
}

/* EOF */