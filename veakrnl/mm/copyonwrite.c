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
        PMMPDE pde = MM_GET_PDE(Vas[i]);
        if (!pde->Hard.Valid) {
            ULONG NewPtPhys = MmAllocatePhysicalPage();
            if (NewPtPhys != 0) {
                MI_WRITE_PDE_VALID(pde, NewPtPhys >> 12);
                PVOID PtVirtualAddr = (PVOID)((ULONG)MM_GET_PTE(Vas[i]) & 0xFFFFF000);
                MMI_INVALIDATE_TLB((ULONG)PtVirtualAddr);

                RtlZeroMemory(PtVirtualAddr, 4096);
            }
        }
    }
}
