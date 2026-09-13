#include <veakrnl.h>

static
VOID
VEAPI
MmpCopyPhysicalPage(
    ULONG DstPhys,
    ULONG SrcPhys
)
{
    PAGE_ENTRY *SrcPte = MM_GET_PTE(MmScratchSrcVa);
    PAGE_ENTRY *DstPte = MM_GET_PTE(MmScratchDstVa);

    MMI_WRITE_PTE_VALID(SrcPte, SrcPhys >> 12);
    MMI_WRITE_PTE_VALID(DstPte, DstPhys >> 12);
    MMI_INVALIDATE_TLB(MmScratchSrcVa);
    MMI_INVALIDATE_TLB(MmScratchDstVa);

    RtlCopyMemory((PVOID)MmScratchDstVa, (PVOID)MmScratchSrcVa, 4096);

    MI_WRITE_INVALID_PTE(SrcPte);
    MI_WRITE_INVALID_PTE(DstPte);
    MMI_INVALIDATE_TLB(MmScratchSrcVa);
    MMI_INVALIDATE_TLB(MmScratchDstVa);
}

BOOLEAN 
VEAPI
MmAccessFault(
    ULONG FaultingAddress, 
    BOOLEAN WriteFault, 
    BOOLEAN WasPresent
)
{
    if (!WasPresent) 
    {
        // bukan urusan CoW — ini demand-paging/invalid access, jalur beda
        return FALSE;
    }

    PAGE_ENTRY *pte = MM_GET_PTE(FaultingAddress);

    if (WriteFault && pte->bits.present && !pte->bits.rw && pte->bits.COW) 
    {
        ULONG OldPhys = pte->bits.frame << 12;
        ULONG Index = OldPhys / 4096;

        if (MmPipDatabase[Index].ShareCount == 1) 
        {
            // cuma proses ini yang pegang -> gak perlu copy, langsung writable
            pte->bits.rw = 1;
            pte->bits.COW = 0;
        } 
        else
        {
            // masih di-share -> alokasi page baru, copy isi, lepas 1 referensi lama
            ULONG NewPhys = MmAllocatePhysicalPage();
            MmpCopyPhysicalPage(NewPhys, OldPhys);
            MmFreePhysicalPage(OldPhys);          // decrement ShareCount lama
            pte->bits.frame = NewPhys >> 12;
            pte->bits.rw = 1;
            pte->bits.COW = 0;
        }
        MMI_INVALIDATE_TLB(FaultingAddress);
        return TRUE;
    }

    return FALSE; // beneran access violation
}