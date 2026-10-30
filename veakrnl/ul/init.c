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

VOID
VEAPI
VeaTestSoftwareException(VOID)
{
    DPRINT("\n==================================================\n");
    DPRINT("     TESTING SOFTWARE EXCEPTION INJECTION         \n");
    DPRINT("==================================================\n");

    _VEA_TRY
    {
        DPRINT("[SW_EXCEPT] Inside _VEA_TRY block. Raising STATUS_INSUFFICIENT_MEMORY...\n");

        // Lempar Software Exception secara manual (tanpa CPU Crash!)
        UlRaiseStatus(STATUS_INSUFFICIENT_MEMORY);

        DPRINT("[ERROR] Unreachable code! ExRaiseStatus failed!\n");
    }
    _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
    {
        DPRINT("[SW_EXCEPT] SUCCESS! Caught Software Exception in _VEA_EXCEPT!\n");
        DPRINT("[SW_EXCEPT] Raised Status Code: 0x%X\n", GetExceptionCode());
    }
    _VEA_END;

    DPRINT("==================================================\n\n");
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

    /* Set to Phase 1 */
    UlpInitializationPhase = 1;

    /* Allocate Pool for processing in Phase1*/
    Buffer = (PCHAR)UlAllocatePoolWithTag(NonPagedPool, 0x2000, 'Ex1o');
    if(!Buffer)
    {
        /* Bugcheck */
        KsBugCheckEx(PHASE1_INITIALIZATION_FAILED, 0x1, (ULONG_PTR)Buffer, 0, 0);
    }

    /* Make sure LoaderBlock is fine */
    if(!KsLoaderBlock)
    {
        /* Bugcheck again */
        KsBugCheck(PHASE1_INITIALIZATION_FAILED);
    }
    
    /* Initialize our BvDriver, acquire it then */
    BvInitializeDriver(KsLoaderBlock);
    BvAcquireDisplayState(TRUE);

    /* Parse our argument*/
    if((RtlCheckBootFlag(BootArg, "/CLIMODE")))
    {
        TextBootMode = TRUE;
    }

    /* MiniVeaMode = VeaInstaller or VEAPE mode */
    if((RtlCheckBootFlag(BootArg, "/MINIVEA")))
    {
        MiniVeaMode = TRUE;
    }

    /* Debugging purpose */
    if(RtlCheckBootFlag(BootArg, "/DEBUG"))
    {
        DebugMode = TRUE;

        /* /VERBOSELOG must include /DEBUG flag */
        if(RtlCheckBootFlag(BootArg, "/VERBOSELOG"))
        {
            VerboseDebugMode = TRUE;
        }
    }

    /* For another argument boot option (eg. /MAXMEM=VALUE, /BURNMEMORY=VALUE,
     /REPAIRING, /BAUDRATE=VALUE, /SERIAL=COM1, etc), throw to this function */
    UlParseBootArgument(BootArg);

    if(!TextBootMode)
    {
        /* Initialize Logo Bitmap booting */
        BvDisplayBootLogoFadeIn();
        // BvEnableStringDisplay(FALSE);
    }
    else
    {
        /* Clean our logo hehe */
        // BvEnableStringDisplay(TRUE);
        // BvShowHeaderFooterTextMode();
    }

    if (!ObInitSystem())
    {
        KsBugCheckEx(OBJECT0_INITIALIZATION_FAILED, 1, 0, 0, 0);
    }

    if(!VkInitSystem1(KsLoaderBlock))
    {
        KsBugCheck(VKEY0_INITIALIZATION_FAILED);
    }

    /* Call UlInitSystem1 */
    if (!UlInitSystem())
    {
        /* Failed */
        KsBugCheckEx(PHASE1_INITIALIZATION_FAILED, 1, 0, 0, 0);
    }

    /* Call MmInitialize */
    if (!MmInitialize(KsLoaderBlock))
    {
        KsBugCheckEx(MEMORY_MANAGEMENT_ERROR, 0x2, 0, 0, 0);
    }

    /* Call Vk phase 1*/
    if(!VkInitSystemPhase1())
    {
        /* Bugcheck */
        KsBugCheckEx(VKEY1_INITIALIZATION_FAILED, 1, 0, 0, 0);
    }

    /* Initialize our Io Subsystem*/
    if(!ObioInitSystem1())
    {
        /* Bugcheck */
        KsBugCheckEx(IO1_INITIALIZATION_FAILED, 1, 0, 0, 0);
    }

    ObDumpObjectTree();
    ObioDumpDeviceTree();

    DPRINT("ObioInitSystem1 finished\n");

    VeaTestSoftwareException();

    Loop:
    PtIdleLoop();
}

