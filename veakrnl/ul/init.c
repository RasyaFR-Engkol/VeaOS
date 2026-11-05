#include <veakrnl.h>

UCHAR UlpInitializationPhase = 0;

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

    if(!SfInitSystem(0))
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

    /*if(!kpc_initialize())
    {
        //ks_bug_check();
    }*/

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

static ULONG MockSpinLock = 0;

static VOID AcquireMockLock(VOID) {
    MockSpinLock = 1;
    DPRINT((" -> [LOCK] Spinlock didapatkan.\n"));
}

static VOID ReleaseMockLock(VOID) {
    MockSpinLock = 0;
    DPRINT((" -> [LOCK] Spinlock berhasil dilepas oleh __finally!\n"));
}

VOID VeaSehTestExecute(VOID)
{
    DPRINT(("[VeaSEH Test] Memulai pengujian SEH Unwind & Exception Handling...\n"));

    _VEA_TRY {
        DPRINT(("[Outer TRY] Masuk ke Outer Block.\n"));

        _VEA_TRY {
            DPRINT(("[Inner TRY] Mengambil Spinlock...\n"));
            AcquireMockLock();

            DPRINT(("[Inner TRY] Memicu Fault: Divide by Zero...\n"));
            
            // Trik Inline ASM untuk memicu Hardware Division Error (Vector 0 / STATUS_INTEGER_DIVIDE_BY_ZERO)
            volatile int Zero = 0;
            volatile int Result = 42 / Zero;
            (VOID)Result;

            DPRINT(("[Inner TRY] KODE INI TIDAK BOLEH DIEKSEKUSI!\n"));
        }
        _VEA_FINALLY {
            DPRINT("[Inner FINALLY] Unwind Pass 2 berjalan! AbnormalTermination = %d\n", 
                     AbnormalTermination());
            
            // Melepas Lock agar tidak terjadi Deadlock di Kernel
            ReleaseMockLock();
        }
        _VEA_END;

    }
    _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER) {
        DPRINT("[Outer EXCEPT] Exception tertangkap dengan sukses!\n");
        DPRINT(" -> Exception Code : 0x%08X\n", GetExceptionCode());
        DPRINT(" -> TrapFrame Ptr  : %p\n", GetExceptionInformation()->TrapFrame);
    }
    _VEA_END;

    // Evaluasi integritas kernel pasca-unwind
    DPRINT("[VeaSEH Test] Status Spinlock Akhir: %s\n", 
             MockSpinLock == 0 ? "OK (Bebas)" : "TERKUNCI (Memory Leak!)");
    
    if (MockSpinLock == 0) {
        DPRINT(("[VeaSEH Test] SUCCESS: Dual-pass unwinding & register restore bekerja sempurna.\n"));
    } else {
        DPRINT(("[VeaSEH Test] FAILED: Unwind melompati __finally!\n"));
    }
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

    if (!MmInitialize(KsLoaderBlock))  KsBugCheckEx(MEMORY_MANAGEMENT_ERROR, 0x2, 0, 0, 0);

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

    // TODO:
    // if (!KpcInitSystem()) KsBugCheck(KPC_INITIALIZATION_FAILED);

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
        1. Unmap LoaderBlock and set that to NULL
        2. SfInitSystem1
        3. Update progress bar to 100
        4. and then call FinalizeBootLogo
        5. Allow Inbv to display string
        6. call ExpLoadInitialProcess to load initial process and then wait for 5 seconds
        7. if Session terminated, bugcheck SESSION5_FAILED
        8. Close that Process Handle
        9. Free virtual memory of Environment PVOID and Initial process parameter
        10. Increase UlpInitializtaionPhase to Phase2
        11. Free InitBuffer
    
    */

    //
    // Dump this so we know something fine in VeaOS Ob Tree and Device Tree
    //
    
    ObDumpObjectTree();
    ObioDumpDeviceTree();

    VeaSehTestExecute();

    Loop:
    PtIdleLoop();
}

