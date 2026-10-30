#include <veakrnl.h>

ULONG MmScratchSrcVa = 0;
ULONG MmScratchDstVa = 0;

VOID
VEAPI
MmpInitializeCowScratch(VOID)
{
    DPRINT("Allocating Scratch...\n");
    MmScratchSrcVa = MmAllocateVirtualRange(1);
    MmScratchDstVa = MmAllocateVirtualRange(1);
    DPRINT("Allocating Scratch Done\n");

    ULONG Vas[2] = { MmScratchSrcVa, MmScratchDstVa };
    for (int i = 0; i < 2; i++) {
        PAGE_ENTRY *pde = MM_GET_PDE(Vas[i]);
        if (!pde->bits.present) {
            ULONG NewPtPhys = MmAllocatePhysicalPage();
            if (NewPtPhys != 0) {
                MMI_WRITE_PDE_VALID(pde, NewPtPhys >> 12);
                PVOID PtVirtualAddr = (PVOID)((ULONG)MM_GET_PTE(Vas[i]) & 0xFFFFF000);
                MMI_INVALIDATE_TLB((ULONG)PtVirtualAddr);

                RtlZeroMemory(PtVirtualAddr, 4096);
            }
        }
    }
}
