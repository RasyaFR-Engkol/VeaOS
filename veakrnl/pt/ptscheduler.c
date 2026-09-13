#include <veakrnl.h>

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

VOID
VEAPI
PtInitializeRunQueues(VOID)
{
    for (ULONG i = 0; i < MAX_CPU; i++)
    {
        RtlZeroMemory(&PtRunQueue[i], sizeof(KRUNQUEUE));
        RtlInitializeRbTree(&PtRunQueue[i].CfsTree, PtCfsCompareRoutine);

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
        // RBT Kosong! Gunakan Idle Thread CPU saat ini
        UCHAR CpuId = KeGetCurrentProcessorNumber();
        NextThread = &PtSystemIdleThread[CpuId];
        NextThread->State = Running;
        RunQueue->CurrentThread = NextThread;
    }

    return NextThread;
}

VOID
VEAPI
PtYieldThread(VOID)
{
    PKPRCB Prcb = KeGetCurrentPrcb();
    PKTHREAD CurrentThread = (PKTHREAD)Prcb->CurrentThread;
    PKRUNQUEUE RunQueue = &PtRunQueue[Prcb->Number];

    if (CurrentThread && CurrentThread != &PtSystemIdleThread[Prcb->Number])
    {
        PtEnqueueThreadCfs(RunQueue, CurrentThread);
    }
    
    PKTHREAD NextThread = PtSelectNextThread(RunQueue);

    // Lakukan Swap Context HANYA jika threadnya berbeda!
    if (CurrentThread != NextThread)
    {
        PtSetCurrentThread(NextThread);

        // Amankan pointer dari kemungkinan NULL (Idle Thread tidak punya Process)
        PKPROCESS OldProcess = CurrentThread ? CurrentThread->ApcState.Process : NULL;
        PKPROCESS NewProcess = NextThread ? NextThread->ApcState.Process : NULL;

        // Cek jika proses berbeda DAN proses baru itu valid (bukan NULL)
        if (OldProcess != NewProcess && NewProcess != NULL) 
        {
            __asm__ volatile ("mov %0, %%cr3" : : "r"(NewProcess->DirectoryTableBase) : "memory");
        }

        PtSwapContext(CurrentThread, NextThread);
    }
}

VOID
VEAPI
PtSchedulerTick(ULONGLONG DeltaTimeNs)
{
    PKPRCB Prcb = KeGetCurrentPrcb();
    PKTHREAD CurrentThread = (PKTHREAD)Prcb->CurrentThread;
    PKRUNQUEUE RunQueue = &PtRunQueue[Prcb->Number];

    if (!CurrentThread) return;

    // A. Update VRuntime Thread yang sedang berjalan (O(1))
    // Formula sederhana CFS: VRuntime += DeltaTime * (NICE_0_LOAD / Weight)
    CurrentThread->VRuntime += DeltaTimeNs;

    // B. Cek Preemption: Apakah ada thread lain di RBT yang VRuntime-nya JAUH lebih kecil?
    PRTL_BALANCED_NODE Leftmost = RtlGetLeftmostNodeRb(&RunQueue->CfsTree);
    if (Leftmost)
    {
        PKTHREAD Candidate = CONTAINING_RECORD(Leftmost, KTHREAD, CfsNode);

        // Jika Candidate di RBT punya VRuntime lebih kecil dibanding CurrentThread
        // (Bisa tambahkan threshold/granularity misal 1-2 ms biar gak terlalu sering switch)
        if (Candidate->VRuntime < CurrentThread->VRuntime)
        {
            // Pemicu Involuntary Preemption!
            PtYieldThread();
        }
    }
}