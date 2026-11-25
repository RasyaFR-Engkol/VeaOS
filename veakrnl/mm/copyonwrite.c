#include <veakrnl.h>

ULONG MmScratchSrcVa = 0;
ULONG MmScratchDstVa = 0;

extern KSPIN_LOCK MmCowScratchLock;

VOID
VEAPI
MmpInitializeCowScratch(VOID)
{
    KsInitializeSpinlock(&MmCowScratchLock);
    MmScratchSrcVa = MmAllocateVirtualRange(1);
    MmScratchDstVa = MmAllocateVirtualRange(1);

    ULONG Vas[2] = { MmScratchSrcVa, MmScratchDstVa };
    for (int i = 0; i < 2; i++) {
        if (!MmpEnsurePageTable(Vas[i])) {
            DPRINT("[MM/COW] Failed to ensure scratch page table for 0x%08X\n",
                   Vas[i]);
        }
    }
}
