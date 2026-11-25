#include <veakrnl.h>

UCHAR UlpInitializationPhase = 0;
static CHAR VeaSystemRootBuffer[256] = "D1P1:/VeaOS";
static CHAR VeaInitialUserProcessBuffer[256] = "/SystemRoot/smss.elf";
extern BOOLEAN BvVideoDriverInstalled;

BOOLEAN
VEAPI
UlpInitSystemPhase0(VOID)
{
    /* TODO: Initialize ERESOURCE */

    UlpInitLookasideList();

    return TRUE;
}

BOOLEAN 
VEAPI
UlpInitSystemPhase1(VOID)
{
    /* Initialize Worker */
    UlpInitializeWorkerThreads();

    if (!UlInitializeEventImplementation())
    {
        return FALSE;
    }

    if (!UlInitializeTimerImplementation())
    {
        return FALSE;
    }

    if (!UlVeapiInit())
    {
        return FALSE;
    }
    
    return TRUE;
}

BOOLEAN
VEAPI
UlInitSystem(VOID)
{
    switch (UlpInitializationPhase)
    {
        case 0:
        {
            return UlpInitSystemPhase0();
        }

        case 1:
        {
            return UlpInitSystemPhase1();
        }

        default:
        {
            KsBugCheck(UNEXPECTED_INITIALIZATION_CALL);
            return FALSE;
        }
    }
}

void ul_initialize_layer(PBLOCK_BOOT_2 block_boot)
{
    /* ini yang harus di inisialisasi (in chronological order)
    1. mm
    2. ob
    3. pt
    4. kpc
    5. ul
    6. hct
    simple */

    UlInitPoolLookasidePointers();

    if(!MmInitialize(block_boot))
    {
        KsBugCheck(MEMORY_INITIALIZATION_FAILURE);
    }

    if (!SfInitSystem(0))
    {
        KsBugCheck(SECURITY_INITIALIZATION_FAILED);
    }

    if(!ObInitSystem())
    {
        KsBugCheck(OBJECT0_INITIALIZATION_FAILED);
    }

    if(!PtInitSystem())
    {
        KsBugCheck(PROCESSTHREAD_INITIALIZATION_FAILED);
    }

    if(!HctInitSystem(0))
    {
        KsBugCheck(HAL0_INITIALIZATION_FAILED);
    }

    if(!UlInitSystem())
    {
        KsBugCheck(PHASE0_INITIALIZATION_FAILED);
    }
}

VOID
VEAPI
UlParseBootArgument(PSTR BootArgument)
{
    
}

VEASTATUS
VEAPI
UlpCreateSystemRootLink(
    IN PBLOCK_BOOT_2 LoaderBlock
)
{
    /* STILL TODO */

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
UlpLoadInitialProcess(VOID)
{
    VEASTATUS Status;
    ULONG ParamsSize;
    ULONG EnvSize;
    PCHAR p;
    PCHAR EnvCursor;
    PCHAR SpacePos;
    PCHAR ColonPos;
    ULONG SystemRootLen;
    ULONG SystemDriveLen;
    ULONG ImageLen;
    ULONG CmdLineLen;
    PRTL_USER_PROCESS_PARAMETERS ProcessParams = NULL;
    PCHAR EnvironmentPtr = NULL;
    RTL_USER_PROCESS_INFORMATION ProcessInfo;
    ANSI_STRING SmssName;

    ParamsSize = sizeof(RTL_USER_PROCESS_PARAMETERS) + (256 * 4);
    ProcessParams = (PRTL_USER_PROCESS_PARAMETERS)UlAllocatePoolWithTag(
        NonPagedPool,
        ParamsSize,
        'mrPP'
    );
    if (!ProcessParams)
    {
        DPRINT("[UL/INIT] Unable to allocate Process Parameters\n");
        KsBugCheckEx(SESSION1_INITIALIZATION_FAILED, STATUS_INSUFFICIENT_RESOURCES, 0, 0, 0);
    }

    RtlZeroMemory(ProcessParams, ParamsSize);
    ProcessParams->Length        = ParamsSize;
    ProcessParams->MaximumLength = ParamsSize;
    ProcessParams->Flags         = RTL_USER_PROCESS_PARAMETERS_NORMALIZED |
                                   RTL_USER_PROCESS_PARAMETERS_RESERVE_1MB;

    EnvSize = PAGE_SIZE;
    EnvironmentPtr = (PCHAR)UlAllocatePoolWithTag(
        NonPagedPool,
        EnvSize,
        'vnEU'
    );

    if (!EnvironmentPtr)
    {
        UlFreePool(ProcessParams);
        DPRINT("[UL/INIT] Unable to allocate Process Environment\n");
        KsBugCheckEx(SESSION2_INITIALIZATION_FAILED, STATUS_INSUFFICIENT_RESOURCES, 0, 0, 0);
    }

    RtlZeroMemory(EnvironmentPtr, EnvSize);
    ProcessParams->Environment = EnvironmentPtr;

    SystemRootLen = 0;
    while (VeaSystemRootBuffer[SystemRootLen] != '\0' && SystemRootLen < (256 - 1))
    {
        SystemRootLen++;
    }

    p = (PCHAR)(ProcessParams + 1);
    ProcessParams->CurrentDirectory.DosPath.Buffer        = p;
    ProcessParams->CurrentDirectory.DosPath.MaximumLength = 256;
    ProcessParams->CurrentDirectory.DosPath.Length        = (USHORT)SystemRootLen;
    RtlCopyMemory(p, VeaSystemRootBuffer, SystemRootLen);
    p[SystemRootLen] = '\0';

    p += 256;
    ProcessParams->DllPath.Buffer        = p;
    ProcessParams->DllPath.MaximumLength = 256;
    RtlCopyMemory(p, VeaSystemRootBuffer, SystemRootLen);
    RtlCopyMemory(p + SystemRootLen, "/SystemReserved", 15);
    ProcessParams->DllPath.Length = (USHORT)(SystemRootLen + 15);
    p[ProcessParams->DllPath.Length] = '\0';

    if (VeaInitialUserProcessBuffer[0] == '\0')
    {
        UlFreePool(EnvironmentPtr);
        UlFreePool(ProcessParams);
        KsBugCheckEx(SESSION2_INITIALIZATION_FAILED, STATUS_INVALID_PARAMETER, 0, 0, 0);
    }

    SpacePos = VeaInitialUserProcessBuffer;
    CmdLineLen = 0;
    while (VeaInitialUserProcessBuffer[CmdLineLen] != '\0' && CmdLineLen < (256 - 1))
    {
        CmdLineLen++;
    }

    while (*SpacePos != '\0' && *SpacePos != ' ')
    {
        SpacePos++;
    }
    ImageLen = (ULONG)(SpacePos - VeaInitialUserProcessBuffer);

    p += 256;
    ProcessParams->ImagePathName.Buffer        = p;
    ProcessParams->ImagePathName.MaximumLength = 256;
    ProcessParams->ImagePathName.Length        = (USHORT)ImageLen;
    RtlCopyMemory(p, VeaInitialUserProcessBuffer, ImageLen);
    p[ImageLen] = '\0';

    p += 256;
    ProcessParams->CommandLine.Buffer        = p;
    ProcessParams->CommandLine.MaximumLength = 256;
    ProcessParams->CommandLine.Length        = (USHORT)CmdLineLen;
    RtlCopyMemory(p, VeaInitialUserProcessBuffer, CmdLineLen);
    p[CmdLineLen] = '\0';

    EnvCursor = EnvironmentPtr;

    RtlCopyMemory(EnvCursor, "Path=", 5);
    EnvCursor += 5;
    RtlCopyMemory(EnvCursor, ProcessParams->DllPath.Buffer, ProcessParams->DllPath.Length);
    EnvCursor += ProcessParams->DllPath.Length;
    *EnvCursor++ = '\0';

    ColonPos = VeaSystemRootBuffer;
    while (*ColonPos != '\0' && *ColonPos != ':')
    {
        ColonPos++;
    }

    if (*ColonPos == ':')
    {
        SystemDriveLen = (ULONG)(ColonPos - VeaSystemRootBuffer) + 1; /* Termasuk ':' */
        RtlCopyMemory(EnvCursor, "SystemDrive=", 12);
        EnvCursor += 12;
        RtlCopyMemory(EnvCursor, VeaSystemRootBuffer, SystemDriveLen);
        EnvCursor += SystemDriveLen;
        *EnvCursor++ = '\0';
    }

    RtlCopyMemory(EnvCursor, "SystemRoot=", 11);
    EnvCursor += 11;
    RtlCopyMemory(EnvCursor, VeaSystemRootBuffer, SystemRootLen);
    EnvCursor += SystemRootLen;
    *EnvCursor++ = '\0';

    *EnvCursor++ = '\0';

    SmssName = ProcessParams->ImagePathName;
    RtlZeroMemory(&ProcessInfo, sizeof(RTL_USER_PROCESS_INFORMATION));
    ProcessInfo.Size = sizeof(RTL_USER_PROCESS_INFORMATION);

    Status = RtlCreateUserProcess(
        &SmssName,
        OBJ_CASE_INSENSITIVE,
        ProcessParams,
        NULL,
        NULL,
        NULL,
        FALSE,
        NULL,
        NULL,
        &ProcessInfo
    );

    UlFreePool(EnvironmentPtr);
    UlFreePool(ProcessParams);

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[UL/INIT] Unable to create Session Manager: 0x%08X\n", Status);
        KsBugCheckEx(SESSION3_INITIALIZATION_FAILED, Status, 0, 0, 0);
    }

    DPRINT("[UL/INIT] SMSS Created! PID=%p, TID=%p, EntryPoint=%p\n",
           ProcessInfo.ClientId.UniqueProcess,
           ProcessInfo.ClientId.UniqueThread,
           ProcessInfo.ImageInformation.TransferAddress);

    Status = VzResumeThread(ProcessInfo.ThreadHandle, NULL);
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[UL/INIT] Unable to resume Session Manager: 0x%08X\n", Status);
        VzTerminateProcess(ProcessInfo.ProcessHandle, Status);
        VzClose(ProcessInfo.ThreadHandle);
        VzClose(ProcessInfo.ProcessHandle);
        KsBugCheckEx(SESSION4_INITIALIZATION_FAILED, Status, 0, 0, 0);
    }

    VzClose(ProcessInfo.ThreadHandle);
    VzClose(ProcessInfo.ProcessHandle);

    return STATUS_SUCCESS;
}

VOID
VEAPI
UlPhase1(VOID)
{
    PCHAR Buffer;
    BOOLEAN MiniVeaMode = FALSE, DebugMode = FALSE, VerboseDebugMode = FALSE;
    BOOLEAN TextBootMode = FALSE;
    PSTR BootArg = KsLoaderBlock->VeaBootArgument;
    VEASTATUS Status;

    KdPrintf("PHASE1\n\r");

    //
    // Notify Ulp
    //
    UlpInitializationPhase = 1;

    //
    // TODO: Buffer on NT is not actually PUCHAR Buffer. But instead, it is struct BUFFER_INIT
    //       and we need to change it later.
    //
    Buffer = (PCHAR)UlAllocatePoolWithTag(NonPagedPool, 0x2000, 'Ex1o');
    if(!Buffer)
    {
        KsBugCheckEx(PHASE1_INITIALIZATION_FAILED, 0x1, (ULONG_PTR)Buffer, 0, 0);
    }

    if(!KsLoaderBlock)
    {
        KsBugCheck(PHASE1_INITIALIZATION_FAILED);
    }
    
    BvInitializeDriver(KsLoaderBlock);
    BvAcquireDisplayState(TRUE);

    //
    // CLI Mode is the VeaOS argument to set Text Boot Mode. Actually,
    // CLI mode will changing the chain of booting on VeaOS. Normally,
    // Phase 0 => UlPhase1 => SSMS => WinLogon
    //
    // It's actually changed to
    // Phase 0 => UlPhase1 => SSMS => WinTerminal
    //
    // Passing argument /WinTerminal for SSMS will actually spawning
    // WinTerminal instead of WinLogon
    //

    if((RtlCheckBootFlag(BootArg, "/CLIMODE")))
    {
        TextBootMode = TRUE;
    }

    //
    // This is what they call MiniNT (aka MiniVea here).
    //
    // NB: Purpose of MiniVea is only for Graphics Installer now.  
    //

    if((RtlCheckBootFlag(BootArg, "/MINIVEA")))
    {
        MiniVeaMode = TRUE;
    }

    //
    // If /DEBUG specified, Kdp will not filter DPRINT and will send DPRINT
    // always. Not only DPRINT1 printed out but DPRINT too.
    //

    if(RtlCheckBootFlag(BootArg, "/DEBUG"))
    {
        DebugMode = TRUE;

        /* /VERBOSELOG must include /DEBUG flag */
        if(RtlCheckBootFlag(BootArg, "/VERBOSELOG"))
        {
            VerboseDebugMode = TRUE;
        }
    }

    //
    // For another argument boot option (eg. /MAXMEM=VALUE, /BURNMEMORY=VALUE,
    // /REPAIRING, /BAUDRATE=VALUE, /SERIAL=COM1, etc), throw to this function.
    // The result will be passed to struct called ARGUMENT_CAPABILITY
    //

    UlParseBootArgument(BootArg);

    if(!TextBootMode)
    {
        //
        // This will spawn new Thread for Bv to work on PASSIVE_LEVEL and
        // rendering Logo + Loading Screen. To terminate it, use BvFinalizeLogo()
        //

        BvDisplayBootLogoFadeIn();
        // BvEnableStringDisplay(FALSE);
    }
    else
    {
        //
        // CLI Mode Boot in VeaOS typically not showing Header and Footer of the text.
        // It will just print out Raw String instead of what Kernel doing
        //
        // N.B: Putting /VERBOSELOG together with /CLIMODE may increasing boot time
        //      as it is printing out to Graphic more often rather than rendering logo
        //
        
        // BvEnableStringDisplay(TRUE);
    }

    //
    // PoInitSystem will actually initializing Shutdown List (Vea Power System Standard
    // said that S5 must be available for Po to work perfectly). S5 must be available
    // on ACPI table.
    //
    
    if (!PoInitSystem(0)) KsBugCheck(INTERNAL_POWER_ERROR);

    /*
    
        Block of Y2k Hack Legacy Code

    */

    /*
    
        Block for checking processor and start all processor
    
    */

    /* 
    
        Block for Create the string of MP Message and show it to
        screen
    
    */

    //
    // Initializing Ob System, Vk System, Ul System, Ks System
    // Note for VK: Vk only needed at Phase 1 so it is okay to call it
    // on phase 1
    //

    if (!ObInitSystem()) KsBugCheckEx(OBJECT0_INITIALIZATION_FAILED, 1, 0, 0, 0);

    if(!VkInitSystem1(KsLoaderBlock)) KsBugCheck(VKEY0_INITIALIZATION_FAILED);

    if (!UlInitSystem()) KsBugCheckEx(PHASE1_INITIALIZATION_FAILED, 1, 0, 0, 0);

    // TODO: Initialize Syscall and Machine Depedent Feature
    // if (!KsInitSystem()) KsBugCheckEx(PHASE1_INITIALIZATION_FAILED, 1, 0, 0, 0);

    if (!SfInitSystem(UlpInitializationPhase)) KsBugCheck(SECURITY_INITIALIZATION_FAILED);

    if(!VkInitSystemPhase1()) KsBugCheckEx(VKEY1_INITIALIZATION_FAILED, 1, 0, 0, 0);

    //
    // In actual ReactOS, UlpCreateSystemRootLink will point to Arc Name (in VeaOS
    // its BDI). And then SystemRoot relocated to Windows NT boot path 
    // (such as C:/Windows/) at IO Initialization.
    //

    Status = UlpCreateSystemRootLink(KsLoaderBlock);
    if (!VEA_SUCCESS(Status))
    {
        KsBugCheck(SYMBOLIC_INITIALIZATION_FAILED);
    }

    if (!MmInitialize(KsLoaderBlock)) KsBugCheckEx(MEMORY_MANAGEMENT_ERROR, 0x2, 0, 0, 0);

    /* Todo: Initialize Cache (we should create Cache Manager) */

    /*
    
        Block of code to initialize time if timezone resetted
    
    */

    //
    // TODO:
    // We haven't create FsRtl because we still work on kernel and kernel
    // unfrequently work using File Object (it's mainly on UserMode
    // who always use that). FsRtl should be implemented if you want more 
    // faster IO (Which is not needed for now)
    //

    //if (!FsRtlInitSystem()) KsBugCheckEx(FILE_INITIALIZATION_FAILED, 1, 0, 0, 0);

    /*
    
        Block of code to:
        1. initialize RtlRangeListPackage()
        2. HalReportResourceUsage()
        3. KdDebuggerInitializeDll()
        4. PpInitSystem phase 1
        5. Update progress bar to 20
    
    */

    if (!KpcInitSystem()) KsBugCheck(KPC_INITIALIZATION_FAILED);

    /*
    
        Block of code to:
        Check command line (No need, we already do it upthere)

    */

    /*
    
        Block of code to:
        init XIP, Update progress bar to 25, set KdpTimeSlipPending,
        set progress bar subset floor 25 ceiling 75
    
    */

    if(!ObioInitSystem1()) KsBugCheckEx(IO1_INITIALIZATION_FAILED, 1, 0, 0, 0);

    /*
    
        Block of code to:
        1. set progress bar ceiling 100 floor 0
        2. open key to CCS/Control/Safeboot, check for AlternateShell
           create 'Option' key, and check for AlternateShell. if yes,
           set value key of UseAlternateShell to TRUE
        3. if in WinPE mode, open Control key and create MiniNT key
        4. MmArmInitSystem that won't do anything
        5. Update progress bar to 80
        6. 386 VDM Initialize (no needed tho)
    
    */

    //
    // Actual Po implementation in ReactOS is quitely not ready for production use.
    // So, Im gonna reverse engineering ntoskrnl Windows 11 version to learn
    // how Po module actually work in newer version of Windows
    //
    // Note: PoInitSystem Phase1 actually registering Lid, Battery, and Power Button
    //       but in Windows XP source code, it's doing more than that
    //

    if (!PoInitSystem(1)) KsBugCheck(INTERNAL_POWER_ERROR);

    /*
    
        Block of code to:
        Update progress bar to 90
    
    */

    if (!PtInitSystem()) KsBugCheck(PROCESSTHREAD_INITIALIZATION_FAILED);

    /* 
    
        Block of code to:
        DONE: 1. Unmap LoaderBlock and set that to NULL
        2. SfRmInitSystem1
        3. Update progress bar to 100
        DONE 4. and then call FinalizeBootLogo
        5. Allow Inbv to display string
        DONE 6. call ExpLoadInitialProcess to load initial process and then wait for 5 seconds
        7. if Session terminated, bugcheck SESSION5_INITIALIZATION_FAILED
        8. Close that Process Handle
        9. Free virtual memory of Environment PVOID and Initial process parameter
        10. Increase UlpInitializtaionPhase to Phase2
        11. Free InitBuffer
    
    */

    if (KsLoaderBlock) KsLoaderBlock = NULL;
    // MmFreeLoaderBlock(KsLoaderBlock);

    // if (!SfRfInitSystem1()) KsBugCheck(PROCESSTHREAD_INITIALIZATION_FAILED);

    if (BvVideoDriverInstalled) BvFinalizeBootLogo();

    //
    // DUMP
    //

    ObDumpObjectTree();

    UlpLoadInitialProcess();

    UlpInitializationPhase++;

    Loop:
    PtIdleLoop();
}

