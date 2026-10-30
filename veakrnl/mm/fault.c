/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : fault.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Page Fault Handling & Demand Paging Engine
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

PMM_VAD VEAPI MmpFindVadByVpn(PRTL_BALANCED_NODE Node, ULONG Vpn);
extern KSPIN_LOCK MmVmmLock;

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

    RtlCopyMemory((PVOID)MmScratchDstVa, (PVOID)MmScratchSrcVa, PAGE_SIZE);

    MI_WRITE_INVALID_PTE(SrcPte);
    MI_WRITE_INVALID_PTE(DstPte);
    MMI_INVALIDATE_TLB(MmScratchSrcVa);
    MMI_INVALIDATE_TLB(MmScratchDstVa);
}

BOOLEAN
VEAPI
MmFaultCopyOnWrite(
    ULONG FaultingAddress,
    BOOLEAN WriteFault
)
{
    PAGE_ENTRY *pte = MM_GET_PTE(FaultingAddress);

    if (!pte) return FALSE;

    if (WriteFault && pte->bits.present && !pte->bits.rw && pte->bits.COW) 
    {
        ULONG OldPhys = pte->bits.frame << 12;
        ULONG Index = OldPhys / PAGE_SIZE;

        if (MmPipDatabase[Index].ShareCount == 1) 
        {
            pte->bits.rw = 1;
            pte->bits.COW = 0;
        } 
        else
        {
            ULONG NewPhys = MmAllocatePhysicalPage();
            if (NewPhys == 0) return FALSE;

            MmpCopyPhysicalPage(NewPhys, OldPhys);
            MmFreePhysicalPage(OldPhys); 
            pte->bits.frame = NewPhys >> 12;
            pte->bits.rw = 1;
            pte->bits.COW = 0;
        }

        MMI_INVALIDATE_TLB(FaultingAddress);
        return TRUE;
    }

    return FALSE; // Access Violation sejati (Write ke Read-Only tanpa flag CoW)
}

BOOLEAN
VEAPI
MmFaultDemandPaging(
    ULONG FaultingAddress,
    BOOLEAN WriteFault
)
{
    KIRQL OldIrql;
    ULONG VpnTarget = FaultingAddress >> 12;

    KsAcquireSpinLock(&MmVmmLock, &OldIrql);
    PMM_VAD Vad = MmpFindVadByVpn(MmKernelVadTree.Root, VpnTarget);
    KsReleaseSpinLock(&MmVmmLock, OldIrql);

    if (!Vad)
    {
        return FALSE; 
    }

    // Pastikan Hardware Page Table (PDE) sudah valid
    if (!MmpEnsurePageTable(FaultingAddress))
    {
        return FALSE;
    }

    PAGE_ENTRY *pte = MM_GET_PTE(FaultingAddress);
    if (!pte)
    {
        return FALSE;
    }

    // Alokasikan 1 Frame Physical Page baru
    ULONG PhysPage = MmAllocatePhysicalPage();
    if (PhysPage == 0)
    {
        return FALSE; // Out of Physical Memory
    }

    // Tulis ke PTE & tandai RW
    MMI_WRITE_PTE_VALID(pte, PhysPage >> 12);
    pte->bits.rw = 1;

    // Flush TLB CPU
    MMI_INVALIDATE_TLB(FaultingAddress);

    // Zero-out memori fisik baru lewat Virtual Address yang sudah di-commit
    RtlZeroMemory((PVOID)FaultingAddress, PAGE_SIZE);

    return TRUE;
}

BOOLEAN 
VEAPI
MmAccessFault(
    ULONG FaultingAddress, 
    BOOLEAN WriteFault, 
    BOOLEAN WasPresent
)
{
    DPRINT("[#PF TRACE] MmAccessFault Called! FaultingAddress: 0x%08X (VPN: 0x%X), Write: %d, Present: %d\n\r", 
           FaultingAddress, FaultingAddress >> 12, WriteFault, WasPresent);

    // Skenario 1: Halaman Present tapi Fault -> Tangani Copy-On-Write
    if (WasPresent)
    {
        return MmFaultCopyOnWrite(FaultingAddress, WriteFault);
    }

    // Skenario 2: Halaman Not Present -> Tangani Demand Paging
    return MmFaultDemandPaging(FaultingAddress, WriteFault);
}

/* EOF */