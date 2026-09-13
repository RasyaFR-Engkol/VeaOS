#include <veakrnl.h>

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

    if(!VkInitSystem1(block_boot))
    {
        KsBugCheck(VKEY0_INITIALIZATION_FAILED);
    }

    /*if(!kpc_initialize())
    {
        //ks_bug_check();
    }*/

    if(!HctInitSystem(0))
    {
        KsBugCheck(HAL0_INITIALIZATION_FAILED);
    }

    /*if(!ul_initialize())
    {
        //ks_bug_check();
    }*/
}

VOID
VEAPI
UlParseBootArgument(PSTR BootArgument)
{

}

VOID
VEAPI
UlPhase1(VOID)
{
    PCHAR Buffer;
    BOOLEAN MiniVeaMode = FALSE, DebugMode = FALSE, VerboseDebugMode = FALSE;
    BOOLEAN TextBootMode = FALSE;
    PSTR BootArg = KsLoaderBlock->VeaBootArgument;

    KdPrintf("PHASE1\n\r");

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

    Loop:
    PtIdleLoop();
}