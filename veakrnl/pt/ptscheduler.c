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
    return __ReadEFlags();
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

        RtlZeroMemory(&PtSystemIdleThread[i], sizeof(KTHREAD));
        
        PtSystemIdleThread[i].State = Ready;
        PtSystemIdleThread[i].Weight = 1;
        PtSystemIdleThread[i].VRuntime = 0;
    }

    // --- TAMBAHAN WAJIB DI SINI ---
    // Daftarkan konteks eksekusi Kernel saat ini sebagai Idle Thread 
    // untuk CPU yang mengeksekusi fungsi ini (biasanya BSP / Core 0).
    PKPRCB Prcb = KeGetCurrentPrcb();
    ULONG CurrentCpu = Prcb->Number;

    // Ubah state jadi Running karena core ini sedang mengeksekusi kernel boot
    PtSystemIdleThread[CurrentCpu].State = Running; 

    // Tanamkan pointer Idle Thread ke PRCB
    Prcb->CurrentThread = (struct _KTHREAD *)&PtSystemIdleThread[CurrentCpu];
    
    // Tanamkan juga ke RunQueue saat ini
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
    ULONG_PTR Flags = PtpSilentInterrupt();
    PKPRCB Prcb = KeGetCurrentPrcb();
    PKTHREAD CurrentThread = (PKTHREAD)Prcb->CurrentThread;
    PKRUNQUEUE RunQueue = &PtRunQueue[Prcb->Number];
    PKTHREAD NextThread = PtSelectNextThread(RunQueue);

    // Ambil IRQL asli thread sebelum interupsi (jika tersimpan) atau simpan PASSIVE_LEVEL jika di-preempt
    if (CurrentThread && CurrentThread != &PtSystemIdleThread[Prcb->Number] && CurrentThread->State == Running)
    {
        PtEnqueueThreadCfs(RunQueue, CurrentThread);
    }

    if (CurrentThread != NextThread)
    {
        PtSetCurrentThread(NextThread);

        PKPROCESS OldProcess = CurrentThread ? CurrentThread->ApcState.Process : NULL;
        PKPROCESS NewProcess = NextThread ? NextThread->ApcState.Process : NULL;

        if (OldProcess != NewProcess && NewProcess != NULL) 
        {
            __WriteCR3(NewProcess->DirectoryTableBase);
        }

        PtSwapContext(CurrentThread, NextThread);

        // Setelah switch ke NextThread, Wajib Turunkan TPR ke IRQL milik ResumedThread (biasanya PASSIVE_LEVEL / 0)
        PKTHREAD ResumedThread = PtGetCurrentThread();
        if (ResumedThread)
        {
            KeSetCurrentIrql(ResumedThread->Irql); 
        }
        else
        {
            KeSetCurrentIrql(PASSIVE_LEVEL);
        }
    }

    PtpRestoreInterrupt(Flags);
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

    if (!Thread || Thread->State != Waiting)
        return FALSE;

    Thread->State = Ready;

    Prcb = KeGetCurrentPrcb();
    PtEnqueueThreadCfs(&PtRunQueue[Prcb->Number], Thread);
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

    if (!CurrentThread) return;

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

        // Jika Candidate di RBT punya VRuntime lebih kecil dibanding CurrentThread
        // (Bisa tambahkan threshold/granularity misal 1-2 ms biar gak terlalu sering switch)
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
    KDPC DelayDpc;

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

    KsInitializeDpc(&DelayDpc, PtpDelayExecutionDpcRoutine, (PVOID)CurrentThread);

    KsSetTimer(&CurrentThread->Timer, *Interval, 0, &DelayDpc);

    PtYieldThread();

    __WriteEFlags(Flags);

    return CurrentThread->WaitStatus;
}

/* EOF */