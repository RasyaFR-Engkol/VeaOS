#include <veakrnl.h>

PPIP MmPipDatabase = NULL;
ULONG MmTotalPages = 0;
ULONG MmFirstFreePip = 0xFFFFFFFF;

BOOLEAN 
VEAPI
MmInitialize(PBLOCK_BOOT_1 BlockBoot)
{
    if(!MmInitializePip(BlockBoot))
    {
        return FALSE;
    }

    MmInitializeVmm();

    UlInitializePools();

    kdp_print("MmInitialization Done.\n\r");

    return TRUE;
}