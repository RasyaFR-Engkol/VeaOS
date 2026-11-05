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

static
VEASTATUS
VEAPI
MmpReadPageFromFile(
    IN PFILE_OBJECT FileObject,
    IN ULONGLONG FileOffset,
    IN PVOID TargetVa,
    IN ULONG ByteCount
)
{
    PDEVICE_OBJECT  DeviceObject;
    POIP            Oip;
    POIP_STACK      Stack;
    KEVENT          Event;
    OIP_INFORMATION IoStatus;
    LARGE_INTEGER   Offset;
    VEASTATUS       Status;

    if (!FileObject || !FileObject->Vpb || !FileObject->Vpb->DeviceObject)
        return STATUS_INVALID_DEVICE_REQUEST;

    DeviceObject = FileObject->Vpb->DeviceObject;      /* pakai satu device saja, untuk build dan call */

    KsInitializeEvent(&Event, NotificationEvent, FALSE);
    RtlZeroMemory(&IoStatus, sizeof(IoStatus));
    Offset.QuadPart = (LONGLONG)FileOffset;

    Oip = ObioBuildSynchronusFsdRequest(
              OIP_MJ_READ,
              DeviceObject,
              TargetVa,            /* <- buffer tujuan */
              ByteCount,
              &Offset,
              &Event,              /* disinyalkan saat completion */
              &IoStatus);          /* Status + Information diisi saat completion */
    if (!Oip)
        return STATUS_INSUFFICIENT_RESOURCES;

    /* FileObject dan flag paging dipasang di stack location berikutnya */
    Stack = ObioGetNextOipStackLocation(Oip);          /* nama sesuai desainmu */
    Stack->FileObject = FileObject;
    Oip->Flags |= (OIP_PAGING_IO | OIP_NOCACHE);

    Status = ObioCallObject(DeviceObject, Oip);

    if (Status == STATUS_PENDING)
    {
        KsWaitForSingleObject(&Event);
        Status = IoStatus.Status;
    }

    /* Jangan sentuh Oip lagi: sudah di-free saat completion */

    if (Status == STATUS_END_OF_FILE)
    {
        RtlZeroMemory(TargetVa, ByteCount);            /* page di luar ukuran file */
        return STATUS_SUCCESS;
    }

    if (VEA_SUCCESS(Status) && IoStatus.Information < ByteCount)
    {
        RtlZeroMemory((PUCHAR)TargetVa + IoStatus.Information,
                      ByteCount - IoStatus.Information);
    }

    return Status;
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
    ULONG_PTR AlignedFaultVa = PAGE_ALIGN_DOWN(FaultingAddress);

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

    // Tulis ke PTE & tandai RW/Present
    MMI_WRITE_PTE_VALID(pte, PhysPage >> 12);
    
    // Set proteksi awal (RW atau Read-Only)
    if (WriteFault || (Vad->Protection & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE)))
    {
        pte->bits.rw = 1;
    }

    // Flush TLB CPU agar RAM fisik baru bisa langsung diakses lewat Virtual Address
    MMI_INVALIDATE_TLB(FaultingAddress);

    // CHECK SECTION: ILE-BACKED DEMAND PAGING
    PCONTROL_AREA ControlArea = Vad->ControlArea;
    PFILE_OBJECT FileObject = ControlArea ? ControlArea->FilePointer : NULL;

    if (FileObject != NULL)
    {
        /* 1. FILE-BACKED DEMAND PAGING */
        ULONGLONG FileOffset = Vad->ViewOffset + 
            ((ULONGLONG)(VpnTarget - Vad->StartingVpn) * PAGE_SIZE);

        VEASTATUS ReadStatus = MmpReadPageFromFile(
            FileObject,
            FileOffset,
            (PVOID)AlignedFaultVa,
            PAGE_SIZE
        );

        if (!VEA_SUCCESS(ReadStatus))
        {
            DPRINT("[#PF ERROR] Failed to read page from disk at offset 0x%x (Status: 0x%X)\n",
                    (ULONG)FileOffset, ReadStatus);

            /* Rollback PTE jika disk read gagal */
            MI_WRITE_INVALID_PTE(pte);
            MMI_INVALIDATE_TLB(FaultingAddress);
            MmFreePhysicalPage(PhysPage);
            return FALSE;
        }
    }
    else
    {
        RtlZeroMemory((PVOID)AlignedFaultVa, PAGE_SIZE);
    }

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

    // Skenario 1: Halaman Present tapi Fault -> Tangani Copy-On-Write
    if (WasPresent)
    {
        return MmFaultCopyOnWrite(FaultingAddress, WriteFault);
    }

    // Skenario 2: Halaman Not Present -> Tangani Demand Paging
    return MmFaultDemandPaging(FaultingAddress, WriteFault);
}

/* EOF */