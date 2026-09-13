#include <veakrnl.h>

ULONG PtUniqueThreadId = 0x10;
extern KRUNQUEUE PtRunQueue[MAX_CPU];

VOID
VEAPI
PtEnqueueThreadCfs(
    PKRUNQUEUE RunQueue, 
    PKTHREAD Thread
);

ULONG
VEAPI
PtCounterUniqueThreadId(VOID)
{
    PtUniqueThreadId += 4;
    return PtUniqueThreadId;
}

VOID 
VEAPI
PtpSystemThreadStartup(
    PVOID StartAddress, 
    PVOID Argument
)
{
    // 1. Thread baru mewarisi state CPU dari scheduler (DISPATCH_LEVEL).
    // Kita WAJIB menurunkan IRQL ke PASSIVE_LEVEL agar OS tidak macet!
    KeLowerIrql(PASSIVE_LEVEL); // 0 = PASSIVE_LEVEL

    // 2. Eksekusi fungsi target (misal UlPhase1)
    ((VOID (VEAPI*)(PVOID))StartAddress)(Argument);

    // 3. Jika fungsi selesai dan nge-return, hancurkan thread
    // PtpExitThread(0);
}

VOID
VEAPI
PtpExitThread(
    VEASTATUS ExitStatus
)
{

    /*PETHREAD CurrentThread = PsGetCurrentThread(); 
    CurrentThread->ExitStatus = ExitStatus;
    CurrentThread->Tcb.State = Terminated;

    RemoveEntryList(&CurrentThread->ThreadListEntry);
    ObDereferenceObject(CurrentThread->Tcb.Process);

    KiForcedContextSwitch();
    */

    // 6. Kode di bawah ini DIJAMIN TIDAK AKAN PERNAH DIJALANKAN
    // karena CPU sudah pindah ke stack dan thread lain.
    for(;;) {
        asm volatile("hlt");
    }
}

PVOID
VEAPI
PtpSetupInitialThreadContext(
    IN PVOID InitialStack,      // Kernel Stack (Ring 0)
    IN PVOID UserStack,         // User Stack (Hanya diisi jika UserMode, kalau KernelMode isi NULL)
    IN PVOID StartAddress,
    IN PVOID Argument,
    IN KPROCESSOR_MODE ProcessorMode
)
{
    ULONG_PTR StackPtr = (ULONG_PTR)InitialStack;

    // ==========================================
    // 1. SETUP KTRAP_FRAME DI KERNEL STACK
    // ==========================================
    // Trap Frame SELALU ditaruh di Kernel Stack, baik untuk User maupun Kernel thread.
    StackPtr -= sizeof(KTRAP_FRAME);
    PKTRAP_FRAME TrapFrame = (PKTRAP_FRAME)StackPtr;

    RtlZeroMemory(TrapFrame, sizeof(KTRAP_FRAME));

    TrapFrame->Eip = (ULONG)StartAddress;
    TrapFrame->EFlags = 0x202; // PENTING: Enable Interrupt (IF) & Reserved bit 1

    // ==========================================
    // 2. PRIVILEGE-SPECIFIC SETUP
    // ==========================================
    if (ProcessorMode == KernelMode)
    {
        // 1. Setup Switch Frame (sudah mencakup ruang argumen!)
        StackPtr -= sizeof(KSWITCH_FRAME);
        PKSWITCH_FRAME SwitchFrame = (PKSWITCH_FRAME)StackPtr;

        RtlZeroMemory(SwitchFrame, sizeof(KSWITCH_FRAME));

        SwitchFrame->EFlags = 0x202; // IF=1 (Enable Interrupt)
        
        // 2. KUNCI: Lompat ke Wrapper kita terlebih dahulu!
        SwitchFrame->SwapReturnAddress = (ULONG)PtpSystemThreadStartup; 
        
        // 3. Setup Argumen yang akan dibaca oleh Wrapper
        // Karena struktur memori sudah presisi, 'ret 8' akan membuang DummyArg1 & 2,
        // sehingga ESP persis mendarat di WrapperReturnAddress.
        SwitchFrame->WrapperReturnAddress = (ULONG)PtpExitThread;
        SwitchFrame->StartAddress = (ULONG)StartAddress;
        SwitchFrame->Argument = (ULONG)Argument;

        return (PVOID)StackPtr;
    }
    else if (ProcessorMode == UserMode)
    {
        // USER MODE: Segments harus punya RPL = 3 (Ring 3). 
        // Pastikan macro KGDT32_R3_CODE Anda sudah menyertakan ' | 3 ' di nilainya.
        TrapFrame->SegCs |= KGDT32_R3_CODE;
        TrapFrame->SegDs |= KGDT32_R3_DATA;
        TrapFrame->SegGs |= KGDT32_R3_DATA;
        TrapFrame->SegEs |= KGDT32_R3_DATA;

        // IRET ke Ring 3 AKAN me-pop SS dan ESP, ini wajib diisi!
        TrapFrame->HardwareSegSs |= KGDT32_R3_DATA;

        // USER MODE: Argumen dan Return Address ditaruh di USER STACK
        ULONG_PTR UserStackPtr = (ULONG_PTR)UserStack;
        UserStackPtr -= (2 * sizeof(ULONG)); // Pesan 2 slot ULONG
        
        PULONG UserThreadStack = (PULONG)UserStackPtr;
        // TODO: Ganti 0 dengan fungsi Wrapper Ring 3 yang memanggil Syscall Exit
        UserThreadStack[0] = 0; 
        UserThreadStack[1] = (ULONG)Argument;

        // Arahkan HardwareEsp ke User Stack yang sudah disiapkan
        TrapFrame->HardwareEsp = (ULONG)UserStackPtr;
    }

    // Mengembalikan ESP baru yang menunjuk ke DbgEbp (paling bawah KTRAP_FRAME)
    return (PVOID)StackPtr;
}

PETHREAD
VEAPI
PtCreateSystemThread(
    PEPROCESS ParentProcess,
    PVOID StartAddress,
    PVOID Argument,
    POOL_TYPE PoolType
)
{
    PETHREAD Thread = NULL;

    VEASTATUS Status = ObCreateObject(
        KernelMode,
        PtEthreadType,
        NULL,
        KernelMode,
        NULL,
        sizeof(ETHREAD), // Pass sizeof(ETHREAD) agar dialokasikan dengan benar
        0,
        0,
        (PVOID*)&Thread
    );
    
    if(!VEA_SUCCESS(Status))
    {
        KdPrintf("ERROR: Failed to create System Thread. Status 0x%x\n\r", Status);
        return NULL;
    }

    Thread->Cid.UniqueThread = PtCounterUniqueThreadId(); // Asumsi Anda punya fungsi ini
    Thread->StartAddress = StartAddress;
    Thread->ExitStatus = 0; // Belum exit

    Thread->Tcb.ApcState.Process = &ParentProcess->Pcb;
    Thread->Tcb.State = Initialized; 
    Thread->Tcb.Priority = ParentProcess->Pcb.BasePriority;
    Thread->Tcb.ContextSwitches = 0;

    PVOID StackBase = UlAllocatePoolWithTag(PoolType, PtThreadStackSize, 'Pt  ');
    if(!StackBase)
    {
        // ObDestroyObject(Thread);
        return NULL;
    }

    Thread->Tcb.InitialStack = (PVOID)((ULONG_PTR)StackBase + PtThreadStackSize); 
    Thread->Tcb.StackLimit = StackBase;

    Thread->Tcb.KernelStack = PtpSetupInitialThreadContext(Thread->Tcb.InitialStack, NULL, StartAddress, Argument, KernelMode);

    InsertTailList(&ParentProcess->Pcb.ThreadListHead, &Thread->ThreadListEntry);

    PtEnqueueThreadCfs(&PtRunQueue[0], &Thread->Tcb);

    return Thread;
}

PETHREAD
VEAPI
PtCreateSystemThreadForMainProcess(
    PVOID StartAddress,
    PVOID Argument,
    POOL_TYPE PoolType
)
{
    PEPROCESS Process = PtSystemProcess;
    return PtCreateSystemThread(Process, StartAddress, Argument, PoolType);
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
    InitializeListHead(&InitialThread->WaitListEntry);

    // 2. ATTACH KE PROCESS 
    // Hubungkan APC State ke proses induk
    InitialThread->ApcState.Process = Process;

    if (Process && IsListEmpty(&InitialThread->WaitListEntry))
    {
        // Masukkan thread ini ke daftar thread milik proses
        // Asumsi: Kita menggunakan WaitListEntry sebagai List Node seperti di LdrCreateInitialThreadAndProcess
        InsertTailList(&Process->ThreadListHead, &InitialThread->WaitListEntry);
    }

    // 3. SETUP CONTEXT SWITCH FRAME
    if (StartAddress != NULL) 
    {
        InitialThread->KernelStack = PtpSetupInitialThreadContext(
            InitialThread->InitialStack, 
            NULL,               
            StartAddress,       
            NULL,               
            KernelMode
        );
    } 
    else 
    {
        // Untuk Boot/Idle Thread:
        InitialThread->KernelStack = InitialThread->InitialStack;
    }
}