#include <veakrnl.h>

PPIP MmPipDatabase = NULL;
ULONG MmTotalPages = 0;

BOOLEAN 
VEAPI
MmInitialize(PBLOCK_BOOT_2 BlockBoot)
{

    if (UlpInitializationPhase == 0)
    {
        if(!MmInitializePip(BlockBoot))
        {
            return FALSE;
        }
        if (!MmInitializeKernelPageDirectory())
        {
            DPRINT("MM FATAL: Failed to initialize kernel page directory master\n");
            return FALSE;
        }
        MmInitializeVmm();

        UlInitializePools();
        MmpInitializePageFaultReadAhead();

        if (!MiInitializePipBitmap())
        {
            DPRINT("MM FATAL: Failed to initialize PIP Bitmap\n");
            return FALSE;
        }

        MmpInitializeCowScratch();
    }
    else if (UlpInitializationPhase == 1)
    {
        MmInitSectionImplementation();
        
        MmInitializeZeroPageThreadEvent();
    }

    DPRINT("MmInitialize done in Phase %d\n", UlpInitializationPhase);

    return TRUE;
}