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
KSPIN_LOCK MmCowScratchLock;

#define MM_PF_READ_AHEAD_SHIFT   16
#define MM_PF_READ_AHEAD_SIZE    (1UL << MM_PF_READ_AHEAD_SHIFT)
#define MM_PF_READ_AHEAD_ENTRIES 32

typedef struct _MM_PF_READ_AHEAD_ENTRY
{
    PFILE_OBJECT FileObject;
    ULONGLONG FileOffset;
    PVOID Buffer;
    ULONG ValidBytes;
    ULONG LastAccessTick;
} MM_PF_READ_AHEAD_ENTRY, *PMM_PF_READ_AHEAD_ENTRY;

static MM_PF_READ_AHEAD_ENTRY MmPfReadAhead[MM_PF_READ_AHEAD_ENTRIES];
static ULONG MmPfReadAheadTick;
KSPIN_LOCK MmPfReadAheadLock;
static BOOLEAN MmPfReadAheadInitialized;

VOID
VEAPI
MmpInitializePageFaultReadAhead(VOID)
{
    RtlZeroMemory(MmPfReadAhead, sizeof(MmPfReadAhead));
    MmPfReadAheadTick = 0;
    KsInitializeSpinlock(&MmPfReadAheadLock);
    MmPfReadAheadInitialized = TRUE;
}

VOID
VEAPI
MmpFlushPageFaultReadAhead(
    IN PFILE_OBJECT FileObject OPTIONAL
)
{
    KIRQL OldIrql;
    PVOID ToFree[MM_PF_READ_AHEAD_ENTRIES];
    ULONG FreeCount = 0;

    if (!MmPfReadAheadInitialized)
        return;

    KsAcquireSpinLock(&MmPfReadAheadLock, &OldIrql);
    for (ULONG i = 0; i < MM_PF_READ_AHEAD_ENTRIES; i++)
    {
        if (MmPfReadAhead[i].Buffer &&
            (!FileObject || MmPfReadAhead[i].FileObject == FileObject))
        {
            ToFree[FreeCount++] = MmPfReadAhead[i].Buffer;
            RtlZeroMemory(&MmPfReadAhead[i], sizeof(MM_PF_READ_AHEAD_ENTRY));
        }
    }
    KsReleaseSpinLock(&MmPfReadAheadLock, OldIrql);

    for (ULONG i = 0; i < FreeCount; i++)
    {
        UlFreePool(ToFree[i]);
    }
}

static
VOID
VEAPI
MmpCopyPhysicalPage(
    ULONG DstPhys,
    ULONG SrcPhys
)
{
    PMMPTE SrcPte = MiAddressToPte(MmScratchSrcVa);
    PMMPTE DstPte = MiAddressToPte(MmScratchDstVa);
    KIRQL  OldIrql;

    KsAcquireSpinLock(&MmCowScratchLock, &OldIrql);

    MMI_WRITE_PTE_VALID(SrcPte, SrcPhys >> 12);
    MMI_WRITE_PTE_VALID(DstPte, DstPhys >> 12);
    MMI_INVALIDATE_TLB(MmScratchSrcVa);
    MMI_INVALIDATE_TLB(MmScratchDstVa);

    RtlCopyMemory((PVOID)MmScratchDstVa, (PVOID)MmScratchSrcVa, PAGE_SIZE);

    MI_ERASE_PTE(SrcPte);
    MI_ERASE_PTE(DstPte);
    MMI_INVALIDATE_TLB(MmScratchSrcVa);
    MMI_INVALIDATE_TLB(MmScratchDstVa);

    KsReleaseSpinLock(&MmCowScratchLock, OldIrql);
}

BOOLEAN
VEAPI
MmFaultCopyOnWrite(
    ULONG FaultingAddress,
    BOOLEAN WriteFault
)
{
    PMMPTE Pte;
    MMPTE  TempPte;
    ULONG  OldPfn, OldPhys;

    if (!WriteFault) return FALSE;

    /* PDE harus ada dulu, kalau tidak membaca PTE lewat recursive mapping fault lagi */
    if (!MiAddressToPde(FaultingAddress)->Hard.Valid) return FALSE;

    Pte = MiAddressToPte(FaultingAddress);

    /* Bit 9 (CopyOnWrite) baru bermakna kalau PTE valid, makanya Valid dicek dulu */
    if (!Pte->Hard.Valid || Pte->Hard.Write || !Pte->Hard.CopyOnWrite)
    {
        return FALSE; /* Access violation sejati */
    }

    OldPfn  = Pte->Hard.PageFrameNumber;
    OldPhys = OldPfn << 12;
    TempPte = *Pte;           /* pertahankan Owner, cache bits, Accessed/Dirty */

    if (MmPipDatabase[OldPfn].ShareCount == 1)
    {
        /* Satu-satunya pemilik: cukup jadikan writable */
        TempPte.Hard.Write = 1;
        TempPte.Hard.CopyOnWrite = 0;

        MI_WRITE_VALID_PTE(Pte, TempPte);
        MMI_INVALIDATE_TLB(FaultingAddress);
        return TRUE;
    }
    else
    {
        ULONG NewPhys = MmAllocatePhysicalPage();
        if (NewPhys == 0) return FALSE;

        MmpCopyPhysicalPage(NewPhys, OldPhys);

        TempPte.Hard.PageFrameNumber = NewPhys >> 12;
        TempPte.Hard.Write = 1;
        TempPte.Hard.CopyOnWrite = 0;

        MI_WRITE_VALID_PTE(Pte, TempPte);
        MMI_INVALIDATE_TLB(FaultingAddress);

        /* Lepas halaman lama setelah mapping lama dibatalkan */
        MmFreePhysicalPage(OldPhys);
        return TRUE;
    }
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
    PVOID           IoBuffer = TargetVa;
    PVOID           BounceBuffer = NULL;

    if (!FileObject || !FileObject->Vpb || !FileObject->Vpb->DeviceObject)
        return STATUS_INVALID_DEVICE_REQUEST;

    if (ByteCount == 0)
        return STATUS_SUCCESS;

    RtlZeroMemory(TargetVa, ByteCount);

    /* Jangan pernah kirim alamat user-space ke storage I/O (rawan beda CR3 saat completion) */
    if (MI_IS_USER_ADDRESS((ULONG_PTR)TargetVa))
    {
        BounceBuffer = UlAllocatePoolWithTag(NonPagedPool, ByteCount, 'pfRb');
        if (!BounceBuffer)
            return STATUS_INSUFFICIENT_RESOURCES;

        RtlZeroMemory(BounceBuffer, ByteCount);
        IoBuffer = BounceBuffer;
    }

    DeviceObject = FileObject->Vpb->DeviceObject;

    KsInitializeEvent(&Event, NotificationEvent, FALSE);
    RtlZeroMemory(&IoStatus, sizeof(IoStatus));
    Offset.QuadPart = (LONGLONG)FileOffset;

    Oip = ObioBuildSynchronusFsdRequest(
              OIP_MJ_READ,
              DeviceObject,
              IoBuffer,
              ByteCount,
              &Offset,
              &Event,
              &IoStatus);
    if (!Oip)
    {
        if (BounceBuffer) UlFreePool(BounceBuffer);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Oip->RequestorThread = (PETHREAD)PtGetCurrentThread();
    if (Oip->RequestorThread)
    {
        ObReferenceObject(Oip->RequestorThread);
    }

    Stack = ObioGetNextOipStackLocation(Oip);
    Stack->FileObject = FileObject;
    Oip->Flags |= (OIP_PAGING_IO | OIP_NOCACHE);

    Status = ObioCallObject(DeviceObject, Oip);

    if (Status == STATUS_PENDING)
    {
        KsWaitForSingleObject(&Event);
        Status = IoStatus.Status;
    }

    /* Sekarang sudah bangun kembali di konteks CR3 proses yang fault */
    if (Status == STATUS_END_OF_FILE)
    {
        ULONG Valid = (ULONG)IoStatus.Information;
        if (Valid > ByteCount) Valid = ByteCount;

        if (BounceBuffer && Valid > 0)
            RtlCopyMemory(TargetVa, BounceBuffer, Valid);

        if (Valid < ByteCount)
            RtlZeroMemory((PUCHAR)TargetVa + Valid, ByteCount - Valid);

        if (BounceBuffer) UlFreePool(BounceBuffer);
        return STATUS_SUCCESS;
    }

    if (VEA_SUCCESS(Status) && BounceBuffer)
    {
        RtlCopyMemory(TargetVa, BounceBuffer, ByteCount);
    }

    if (BounceBuffer)
    {
        UlFreePool(BounceBuffer);
    }

    return Status;
}

static
VEASTATUS
VEAPI
MmpReadAheadPageFromFile(
    IN PFILE_OBJECT FileObject,
    IN ULONGLONG FileOffset,
    IN PVOID TargetVa,
    IN ULONG ByteCount
)
{
    ULONGLONG ClusterOffset;
    ULONG PageOffset;
    ULONG EntryIndex;
    PMM_PF_READ_AHEAD_ENTRY Entry = NULL;
    KIRQL OldIrql;

    if (!MmPfReadAheadInitialized)
        return STATUS_UNSUCCESSFUL;

    ClusterOffset = FileOffset & ~((ULONGLONG)MM_PF_READ_AHEAD_SIZE - 1);
    PageOffset = (ULONG)(FileOffset - ClusterOffset);
    if (PageOffset + ByteCount > MM_PF_READ_AHEAD_SIZE)
        return MmpReadPageFromFile(FileObject, FileOffset, TargetVa, ByteCount);

    KsAcquireSpinLock(&MmPfReadAheadLock, &OldIrql);
    for (EntryIndex = 0; EntryIndex < MM_PF_READ_AHEAD_ENTRIES; EntryIndex++)
    {
        PMM_PF_READ_AHEAD_ENTRY Candidate = &MmPfReadAhead[EntryIndex];
        if (Candidate->Buffer &&
            Candidate->FileObject == FileObject &&
            Candidate->FileOffset == ClusterOffset)
        {
            Candidate->LastAccessTick = ++MmPfReadAheadTick;
            Entry = Candidate;
            break;
        }
    }

    if (Entry)
    {
        if (PageOffset < Entry->ValidBytes)
        {
            ULONG Copied = min(ByteCount, Entry->ValidBytes - PageOffset);
            RtlCopyMemory(TargetVa, (PUCHAR)Entry->Buffer + PageOffset, Copied);
            if (Copied < ByteCount)
                RtlZeroMemory((PUCHAR)TargetVa + Copied, ByteCount - Copied);
            KsReleaseSpinLock(&MmPfReadAheadLock, OldIrql);
            return STATUS_SUCCESS;
        }
        KsReleaseSpinLock(&MmPfReadAheadLock, OldIrql);
        RtlZeroMemory(TargetVa, ByteCount);
        return STATUS_SUCCESS;
    }
    KsReleaseSpinLock(&MmPfReadAheadLock, OldIrql);

    PVOID ClusterBuffer = UlAllocatePoolWithTag(
        NonPagedPool,
        MM_PF_READ_AHEAD_SIZE,
        'pfRa'
    );
    if (!ClusterBuffer)
        return MmpReadPageFromFile(FileObject, FileOffset, TargetVa, ByteCount);

    /* MmpReadPageFromFile sudah melakukan RtlZeroMemory, tidak perlu double zero */
    VEASTATUS ReadStatus = MmpReadPageFromFile(
        FileObject,
        ClusterOffset,
        ClusterBuffer,
        MM_PF_READ_AHEAD_SIZE
    );
    if (!VEA_SUCCESS(ReadStatus))
    {
        UlFreePool(ClusterBuffer);
        RtlZeroMemory(TargetVa, ByteCount);
        return ReadStatus;
    }

    PVOID EvictedBuffer = NULL;

    KsAcquireSpinLock(&MmPfReadAheadLock, &OldIrql);
    ULONG ReplaceIndex = 0;
    ULONG OldestTick = 0xFFFFFFFFUL;

    for (EntryIndex = 0; EntryIndex < MM_PF_READ_AHEAD_ENTRIES; EntryIndex++)
    {
        if (!MmPfReadAhead[EntryIndex].Buffer)
        {
            ReplaceIndex = EntryIndex;
            break;
        }
        if (MmPfReadAhead[EntryIndex].LastAccessTick < OldestTick)
        {
            OldestTick = MmPfReadAhead[EntryIndex].LastAccessTick;
            ReplaceIndex = EntryIndex;
        }
    }

    EvictedBuffer = MmPfReadAhead[ReplaceIndex].Buffer;
    MmPfReadAhead[ReplaceIndex].FileObject = FileObject;
    MmPfReadAhead[ReplaceIndex].FileOffset = ClusterOffset;
    MmPfReadAhead[ReplaceIndex].Buffer = ClusterBuffer;
    MmPfReadAhead[ReplaceIndex].ValidBytes = MM_PF_READ_AHEAD_SIZE;
    MmPfReadAhead[ReplaceIndex].LastAccessTick = ++MmPfReadAheadTick;

    if (PageOffset < MM_PF_READ_AHEAD_SIZE)
    {
        ULONG Copied = min(ByteCount, MM_PF_READ_AHEAD_SIZE - PageOffset);
        RtlCopyMemory(TargetVa, (PUCHAR)ClusterBuffer + PageOffset, Copied);
        if (Copied < ByteCount)
            RtlZeroMemory((PUCHAR)TargetVa + Copied, ByteCount - Copied);
    }
    else
    {
        RtlZeroMemory(TargetVa, ByteCount);
    }
    KsReleaseSpinLock(&MmPfReadAheadLock, OldIrql);

    if (EvictedBuffer)
        UlFreePool(EvictedBuffer);

    return STATUS_SUCCESS;
}

static
VOID
VEAPI
MmpPrefaultClusterPages(
    IN ULONG BaseFaultVa,
    IN ULONG StartingVpn,
    IN ULONG EndingVpn,
    IN ULONGLONG ViewOffset,
    IN PFILE_OBJECT FileObject,
    IN BOOLEAN Owner,
    IN BOOLEAN Writable,
    IN ULONG EffectiveProtect
)
{
    ULONG BaseVpn = BaseFaultVa >> PAGE_SHIFT;
    ULONGLONG BaseFileOffset = ViewOffset + ((ULONGLONG)(BaseVpn - StartingVpn) * PAGE_SIZE);
    ULONGLONG ClusterStartOffset = BaseFileOffset & ~((ULONGLONG)MM_PF_READ_AHEAD_SIZE - 1);
    ULONGLONG ClusterEndOffset = ClusterStartOffset + MM_PF_READ_AHEAD_SIZE;

    /* Mulai dari halaman pertama dalam chunk 64 KB yang sama (mundur + maju) */
    ULONG PagesBefore = (ULONG)((BaseFileOffset - ClusterStartOffset) >> PAGE_SHIFT);
    ULONG CurVpn = ((BaseVpn - StartingVpn) > PagesBefore)
                       ? (BaseVpn - PagesBefore)
                       : StartingVpn;

    /* Pastikan tetap dalam Page Table (PDE) yang sama */
    if ((CurVpn >> 10) != (BaseVpn >> 10))
        CurVpn = BaseVpn & ~0x3FFUL;

    for (; CurVpn < EndingVpn && ((CurVpn >> 10) == (BaseVpn >> 10)); CurVpn++)
    {
        if (CurVpn == BaseVpn)
            continue;

        ULONGLONG CurFileOffset = ViewOffset + ((ULONGLONG)(CurVpn - StartingVpn) * PAGE_SIZE);
        if (CurFileOffset >= ClusterEndOffset)
            break;

        ULONG CurVa = CurVpn << PAGE_SHIFT;
        PMMPTE NextPte = MiAddressToPte(CurVa);
        if (NextPte->Hard.Valid)
            continue;

        ULONG ExtraPhys = MmAllocatePhysicalPage();
        if (ExtraPhys == 0)
            break;

        MMPTE TempPte;
        MI_MAKE_VALID_PTE(TempPte, ExtraPhys >> 12, 0, 1);
        MI_WRITE_VALID_PTE(NextPte, TempPte);
        MMI_INVALIDATE_TLB(CurVa);

        /* Langsung copy 4 KB dari cache MmPfReadAhead (0 disk I/O) */
        VEASTATUS Status = MmpReadAheadPageFromFile(
            FileObject,
            CurFileOffset,
            (PVOID)CurVa,
            PAGE_SIZE
        );
        if (!VEA_SUCCESS(Status))
        {
            MI_ERASE_PTE(NextPte);
            MMI_INVALIDATE_TLB(CurVa);
            MmFreePhysicalPage(ExtraPhys);
            break;
        }

        MI_MAKE_VALID_PTE(TempPte, ExtraPhys >> 12, Owner, Writable ? 1 : 0);
        if (EffectiveProtect & MM_NOCACHE)
        {
            TempPte.Hard.CacheDisable = 1;
            TempPte.Hard.WriteThrough = 1;
        }
        MI_WRITE_VALID_PTE(NextPte, TempPte);
        MMI_INVALIDATE_TLB(CurVa);
    }
}

BOOLEAN
VEAPI
MmFaultDemandPaging(
    ULONG FaultingAddress,
    BOOLEAN WriteFault
)
{
    KIRQL OldIrql;
    ULONG VpnTarget = FaultingAddress >> PAGE_SHIFT;
    ULONG_PTR AlignedFaultVa = PAGE_ALIGN_DOWN(FaultingAddress);
    PEPROCESS Process = (PEPROCESS)PtGetCurrentProcess();
    PKSPIN_LOCK Lock = NULL;
    PRTL_AVL_TREE Tree = NULL;
    PMM_VAD Vad = NULL;
    ULONG StartingVpn = 0, Protect = 0, EndingVpn = 0;
    ULONGLONG ViewOffset = 0;
    PCONTROL_AREA ControlArea = NULL;
    PFILE_OBJECT FileObject;
    PMMPTE Pte;
    MMPTE  TempPte;
    ULONG  PhysPage;
    BOOLEAN Writable, IsCopyOnWrite, Owner, HandledAsProgramHdr = FALSE;

    /* 1. Dapatkan AVL Tree yang tepat (Kernel atau User Process) */
    Tree = MmpGetVadTree(Process, VpnTarget, &Lock);
    if (!Tree || !Lock) return FALSE;

    KsAcquireSpinLock(Lock, &OldIrql);
    Vad = MmpFindVadByVpn(Tree->Root, VpnTarget);
    if (Vad)
    {
        StartingVpn = Vad->StartingVpn;
        EndingVpn   = Vad->EndingVpn;
        Protect     = Vad->u.VadFlags.Protection;
        ViewOffset  = (ULONGLONG)Vad->u2.VadFlags2.FileOffset << PAGE_SHIFT;
        ControlArea = Vad->ControlArea;
    }
    KsReleaseSpinLock(Lock, OldIrql);

    if (!Vad) return FALSE;

    FileObject = ControlArea ? ControlArea->FilePointer : NULL;
    if (!MI_IS_USER_ADDRESS(FaultingAddress) && !FileObject)
    {
        return FALSE;
    }

    if (Protect == 0) Protect = MM_READWRITE;

    ULONG EffectiveProtect = Protect;

    /* Jika ELF Image (PT_LOAD), pakai proteksi per-segmen (.text RX vs .data/.bss RW) */
    if (ControlArea &&
        ControlArea->u.Flags.Image &&
        ControlArea->Segment &&
        ControlArea->Segment->u2.ImageInformation)
    {
        PMM_IMAGE_SECTION_OBJECT ImageSection = CONTAINING_RECORD(
            ControlArea->Segment->u2.ImageInformation,
            MM_IMAGE_SECTION_OBJECT,
            ImageInformation
        );

        if (ImageSection->SegFlags & MM_IMAGE_ELF_PROGRAM_HDR)
        {
            ULONG_PTR PageRva    = AlignedFaultVa - ((ULONG_PTR)StartingVpn << PAGE_SHIFT);
            ULONG_PTR PageEndRva = PageRva + PAGE_SIZE;
            ULONG MergedProt     = MM_ZERO_ACCESS;

            for (ULONG i = 0; i < ImageSection->NrSegments; i++)
            {
                PMM_SECTION_SEGMENT Seg = &ImageSection->Segments[i];
                ULONG_PTR SegStartRva   = (ULONG_PTR)Seg->Image.VirtualAddress;
                ULONG_PTR SegEndRva     = SegStartRva + (ULONG_PTR)Seg->Length.QuadPart;

                if (PageRva < SegEndRva && PageEndRva > SegStartRva)
                {
                    /* Jika halaman masuk ke segmen writable (.data/.bss), izinkan write */
                    if (Seg->Protection == MM_READWRITE ||
                        Seg->Protection == MM_WRITECOPY ||
                        Seg->Protection == MM_EXECUTE_READWRITE ||
                        Seg->Protection == MM_EXECUTE_WRITECOPY)
                    {
                        MergedProt = Seg->Protection;
                        break;
                    }
                    MergedProt = Seg->Protection;
                }
            }

            if (MergedProt != MM_ZERO_ACCESS)
            {
                ULONG ConvertedProt = MiMakeProtectionMask(MergedProt);
                if (ConvertedProt != MM_INVALID_PROTECTION)
                {
                    EffectiveProtect = ConvertedProt;
                }
            }
        }
    }

    /* 2. Cek hak akses menggunakan Internal MM Mask */
    ULONG AccessProtect = EffectiveProtect & MM_PROTECT_ACCESS;

    if ((EffectiveProtect & MM_GUARDPAGE) ||
        AccessProtect == MM_ZERO_ACCESS ||
        EffectiveProtect == MM_NOACCESS)
    {
        return FALSE;
    }

    IsCopyOnWrite = (AccessProtect == MM_WRITECOPY ||
                     AccessProtect == MM_EXECUTE_WRITECOPY);

    Writable = (AccessProtect == MM_READWRITE ||
                AccessProtect == MM_EXECUTE_READWRITE ||
                IsCopyOnWrite);

    if (WriteFault && !Writable) return FALSE;

    /* 3. Pastikan Page Table ada, lalu cek apakah PTE sudah terisi */
    if (!MmpEnsurePageTable(FaultingAddress)) return FALSE;

    Pte = MiAddressToPte(FaultingAddress);

    if (Pte->Hard.Valid)
    {
        return (!WriteFault || Pte->Hard.Write) ? TRUE : FALSE;
    }

    /* 4. Alokasi physical frame */
    PhysPage = MmAllocatePhysicalPage();
    if (PhysPage == 0) return FALSE;

    Owner = MI_IS_USER_ADDRESS(FaultingAddress) ? 1 : 0;

    /* Fase 1: Map sementara supervisor-only + RW supaya kernel bisa mengisi halaman */
    MI_MAKE_VALID_PTE(TempPte, PhysPage >> 12, 0, 1);
    MI_WRITE_VALID_PTE(Pte, TempPte);
    MMI_INVALIDATE_TLB(FaultingAddress);

    if (FileObject != NULL)
    {
        if (ControlArea->u.Flags.Image &&
            ControlArea->Segment &&
            ControlArea->Segment->u2.ImageInformation)
        {
            PMM_IMAGE_SECTION_OBJECT ImageSection = CONTAINING_RECORD(
                ControlArea->Segment->u2.ImageInformation,
                MM_IMAGE_SECTION_OBJECT,
                ImageInformation
            );

            if (ImageSection->SegFlags & MM_IMAGE_ELF_PROGRAM_HDR)
            {
                HandledAsProgramHdr = TRUE;
                ULONG_PTR PageRva    = AlignedFaultVa - ((ULONG_PTR)StartingVpn << PAGE_SHIFT);
                ULONG_PTR PageEndRva = PageRva + PAGE_SIZE;

                for (ULONG i = 0; i < ImageSection->NrSegments; i++)
                {
                    PMM_SECTION_SEGMENT Seg = &ImageSection->Segments[i];
                    ULONG_PTR SegStartRva   = (ULONG_PTR)Seg->Image.VirtualAddress;
                    ULONG_PTR SegFileEndRva = SegStartRva + (ULONG_PTR)Seg->RawLength.QuadPart;

                    ULONG_PTR OverlapStart = (PageRva > SegStartRva) ? PageRva : SegStartRva;
                    ULONG_PTR OverlapEnd   = (PageEndRva < SegFileEndRva) ? PageEndRva : SegFileEndRva;

                    if (OverlapStart < OverlapEnd)
                    {
                        ULONG BytesToRead = (ULONG)(OverlapEnd - OverlapStart);
                        ULONG PageOffset  = (ULONG)(OverlapStart - PageRva);
                        ULONGLONG FileOff = (ULONGLONG)Seg->Image.FileOffset +
                                            (ULONGLONG)(OverlapStart - SegStartRva);

                        VEASTATUS ReadStatus = MmpReadAheadPageFromFile(
                            FileObject,
                            FileOff,
                            (PUCHAR)AlignedFaultVa + PageOffset,
                            BytesToRead
                        );

                        if (!VEA_SUCCESS(ReadStatus))
                        {
                            DPRINT("[#PF ERROR] Image segment read failed at offset 0x%x (Status: 0x%X)\n",
                                (ULONG)FileOff, ReadStatus);

                            MI_ERASE_PTE(Pte);
                            MMI_INVALIDATE_TLB(FaultingAddress);
                            MmFreePhysicalPage(PhysPage);
                            return FALSE;
                        }
                    }
                }
            }
        }

        /* Berlaku untuk Data Section biasa (!Image) maupun ET_REL */
        if (!HandledAsProgramHdr)
        {
            ULONGLONG FileOffset = ViewOffset +
                ((ULONGLONG)(VpnTarget - StartingVpn) * PAGE_SIZE);

            VEASTATUS ReadStatus = MmpReadAheadPageFromFile(
                FileObject,
                FileOffset,
                (PVOID)AlignedFaultVa,
                PAGE_SIZE
            );

            if (!VEA_SUCCESS(ReadStatus))
            {
                DPRINT("[#PF ERROR] Failed to read page from disk at offset 0x%x (Status: 0x%X)\n",
                    (ULONG)FileOffset, ReadStatus);

                MI_ERASE_PTE(Pte);
                MMI_INVALIDATE_TLB(FaultingAddress);
                MmFreePhysicalPage(PhysPage);
                return FALSE;
            }
        }
    }
    else
    {
        RtlZeroMemory((PVOID)AlignedFaultVa, PAGE_SIZE);
    }

    /* Fase 2: Tulis PTE final sesuai proteksi efektif */
    if (IsCopyOnWrite && !WriteFault)
    {
        MI_MAKE_VALID_PTE(TempPte, PhysPage >> 12, Owner, 0);
        TempPte.Hard.CopyOnWrite = 1;
    }
    else
    {
        MI_MAKE_VALID_PTE(TempPte, PhysPage >> 12, Owner, Writable ? 1 : 0);
        TempPte.Hard.CopyOnWrite = 0;
    }

    if (EffectiveProtect & MM_NOCACHE)
    {
        TempPte.Hard.CacheDisable = 1;
        TempPte.Hard.WriteThrough = 1;
    }

    MI_WRITE_VALID_PTE(Pte, TempPte);
    MMI_INVALIDATE_TLB(FaultingAddress);

    if (FileObject != NULL && !Owner && !HandledAsProgramHdr && !IsCopyOnWrite)
    {
        MmpPrefaultClusterPages(
            (ULONG)AlignedFaultVa,
            StartingVpn,
            EndingVpn,
            ViewOffset,
            FileObject,
            Owner,
            Writable,
            EffectiveProtect
        );
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
    if (!MI_IS_USER_ADDRESS(FaultingAddress))
    {
        PMMPDE Pde = MiAddressToPde(FaultingAddress);
        BOOLEAN WasKernelPdeMissing = !Pde->Hard.Valid;

        if (!MmpSynchronizeKernelPageTable(FaultingAddress))
        {
            return FALSE;
        }

        if (WasKernelPdeMissing &&
            (Pde->Hard.LargePage || MiAddressToPte(FaultingAddress)->Hard.Valid))
        {
            return TRUE;
        }
    }

    // Skenario 1: Halaman Present tapi Fault -> Tangani Copy-On-Write
    if (WasPresent)
    {
        return MmFaultCopyOnWrite(FaultingAddress, WriteFault);
    }

    // Skenario 2: Halaman Not Present -> Tangani Demand Paging
    return MmFaultDemandPaging(FaultingAddress, WriteFault);
}

/* EOF */