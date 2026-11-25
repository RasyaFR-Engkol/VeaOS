#include <veakrnl.h>

PMM_VAD MmVadRoot = NULL;
RTL_AVL_TREE MmKernelVadTree;
static PMM_VAD MmBootstrapPool = NULL;
static ULONG MmBootstrapIndex = 0;
static PMM_VAD MmVadFreeList = NULL;
KSPIN_LOCK MmVmmLock;
RTL_AVL_TREE MmSectionBasedTree;

LONG 
VEAPI
MmpCompareVad(PRTL_BALANCED_NODE NodeA, PRTL_BALANCED_NODE NodeB) 
{
    PMM_VAD VadA = (PMM_VAD)NodeA;
    PMM_VAD VadB = (PMM_VAD)NodeB;

    if (VadA->StartingVpn < VadB->StartingVpn) return -1;
    if (VadA->StartingVpn > VadB->StartingVpn) return 1;
    return 0;
}

BOOLEAN 
VEAPI 
MmInitializeVmm(VOID)
{
    MmKernelVadTree.Root = NULL;
    MmKernelVadTree.CompareRoutine = MmpCompareVad;
    MmVadRoot = NULL;
    KsInitializeSpinlock(&MmVmmLock);
    return TRUE;
}

PMM_VAD VEAPI MmpAllocateVadNode(VOID)
{
    KIRQL OldIrql;

    // 1. Ambil dari Lookaside / Free List
    KsAcquireSpinLock(&MmVmmLock, &OldIrql);
    if (MmVadFreeList) {
        PMM_VAD ReusedNode = MmVadFreeList;
        MmVadFreeList = (PMM_VAD)ReusedNode->CoreNode.Left;
        KsReleaseSpinLock(&MmVmmLock, OldIrql);
        RtlZeroMemory(ReusedNode, sizeof(MM_VAD));
        return ReusedNode;
    }
    KsReleaseSpinLock(&MmVmmLock, OldIrql);

    // 2. Ambil dari Bootstrap Pool jika masih ada slot
    if (!MmBootstrapPool) {
        ULONG PhysPage = MmAllocateContiguousPhysicalPages(16);
        if (PhysPage == 0) {
            DPRINT("VMM FATAL: Failed to allocate physical pages for Bootstrap Pool!\n\r");
            return NULL;
        }

        ULONG BootstrapVa = PhysPage + 0xC0000000;

        for (ULONG i = 0; i < 16; i++) {
            ULONG CurrentVa = BootstrapVa + (i * PAGE_SIZE);
            ULONG CurrentPhys = PhysPage + (i * PAGE_SIZE);

            if (!MmpEnsurePageTable(CurrentVa)) {
                kdp_print("VMM FATAL: Failed to ensure Bootstrap Page Table!\n\r");
                for (ULONG j = 0; j < i; j++) {
                    ULONG MappedVa = BootstrapVa + (j * PAGE_SIZE);
                    PMMPTE MappedPte = MiAddressToPte(MappedVa);
                    MI_ERASE_PTE(MappedPte);
                    MMI_INVALIDATE_TLB(MappedVa);
                }
                MmFreeContiguousPhysicalPages(PhysPage, 16);
                return NULL;
            }

            // Map PTE ke Alamat Fisik Bootstrap Pool
            PMMPTE pte = MM_GET_PTE(CurrentVa);
            if (!pte->Hard.Valid) {
                MMI_WRITE_PTE_VALID(pte, CurrentPhys >> 12);
                MMI_INVALIDATE_TLB(CurrentVa);
            }
        }

        MmBootstrapPool = (PMM_VAD)BootstrapVa;
        RtlZeroMemory(MmBootstrapPool, 16 * PAGE_SIZE);
    }

    if (MmBootstrapIndex < ((16 * 4096) / sizeof(MM_VAD))) {
        PMM_VAD Node = &MmBootstrapPool[MmBootstrapIndex++];
        RtlZeroMemory(Node, sizeof(MM_VAD));
        return Node;
    }

    // 3. EXPAND VAD POOL SECARA MANDIRI (Tanpa panggil UlAllocatePoolWithTag!)
    // Alokasikan 1 Page Fisik baru khusus VAD, potong-potong jadi Node, lalu masukkan ke Free List
    ULONG NewPagePhys = MmAllocatePhysicalPage();
    if (NewPagePhys == 0) {
        kdp_print("VMM FATAL: Out of physical memory for VAD nodes!\n\r");
        return NULL;
    }

    PMM_VAD NewPageVa = (PMM_VAD)(NewPagePhys + 0xC0000000); // Identity/Direct Mapping
    RtlZeroMemory(NewPageVa, 4096);

    ULONG NodesInPage = 4096 / sizeof(MM_VAD);

    KsAcquireSpinLock(&MmVmmLock, &OldIrql);
    // Masukkan node ke-2 sampai terakhir ke MmVadFreeList
    for (ULONG i = 1; i < NodesInPage; i++) {
        NewPageVa[i].CoreNode.Left = (PRTL_BALANCED_NODE)MmVadFreeList;
        MmVadFreeList = &NewPageVa[i];
    }
    KsReleaseSpinLock(&MmVmmLock, OldIrql);

    // Kembalikan node ke-0 untuk pemanggil saat ini
    return &NewPageVa[0];
}

VOID VEAPI MmpFreeVadNode(PMM_VAD VadNode)
{
    KIRQL OldIrql;
    KsAcquireSpinLock(&MmVmmLock, &OldIrql);
    
    RtlZeroMemory(VadNode, sizeof(MM_VAD));
    VadNode->CoreNode.Left = (PRTL_BALANCED_NODE)MmVadFreeList;
    MmVadFreeList = VadNode;
    
    KsReleaseSpinLock(&MmVmmLock, OldIrql);
}

PMM_VAD 
VEAPI
MmpFindVadByVpn(PRTL_BALANCED_NODE Node, ULONG Vpn)
{
    while (Node != NULL) {
        PMM_VAD Vad = (PMM_VAD)Node;
        
        // Cek apakah VPN berada di dalam rentang VAD ini
        if (Vpn >= Vad->StartingVpn && Vpn <= Vad->EndingVpn) {
            return Vad; 
        }
        
        // Telusuri ke kiri atau ke kanan
        if (Vpn < Vad->StartingVpn) {
            Node = Node->Left;
        } else {
            Node = Node->Right;
        }
    }
    
    return NULL;
}

static
PRTL_BALANCED_NODE
VEAPI
MmpGetFirstAvlNode(IN PRTL_AVL_TREE Tree)
{
    PRTL_BALANCED_NODE Node = Tree ? Tree->Root : NULL;
    while (Node && Node->Left)
    {
        Node = Node->Left;
    }
    return Node;
}

static 
ULONG 
VEAPI
MmpFindFreeVirtualGap(ULONG PageCount, ULONG* LastVpnChecked)
{
    ULONG Candidate = *LastVpnChecked;
    PRTL_BALANCED_NODE CurrentNode = MmpGetFirstAvlNode(&MmKernelVadTree);
    
    while (CurrentNode != NULL) {
        PMM_VAD Vad = (PMM_VAD)CurrentNode;
        
        if (Vad->EndingVpn >= Candidate) {
            /* Wajib cek Vad->StartingVpn > Candidate agar tidak unsigned underflow */
            if (Vad->StartingVpn > Candidate &&
                (Vad->StartingVpn - Candidate) >= PageCount) {
                *LastVpnChecked = Candidate;
                return Candidate << 12;
            }
            Candidate = Vad->EndingVpn + 1;
        }
        CurrentNode = RtlGetNextNodeAvl(CurrentNode);
    }
    
    *LastVpnChecked = Candidate;
    return 0;
}

ULONG 
VEAPI 
MmAllocateVirtualRange(ULONG PageCount)
{
    PMM_VAD NewVad = MmpAllocateVadNode();
    if (!NewVad) {
        return 0;
    }

    KIRQL OldIrql;
    ULONG StartingKva = 0xC1000000; 
    ULONG LastVpnChecked = StartingKva >> 12;
    ULONG AllocatedVa = 0;

    if (PageCount == 0 || PageCount > (0xFFC00000 - StartingKva) / PAGE_SIZE) {
        DPRINT("VMM FATAL: Invalid PageCount=%u (Caller=%p)\n",
           PageCount, __builtin_return_address(0));
        MmpFreeVadNode(NewVad);
        return 0;
    }

    KsAcquireSpinLock(&MmVmmLock, &OldIrql);

    if (!MmKernelVadTree.Root) {
        AllocatedVa = StartingKva;
    } else {
        AllocatedVa = MmpFindFreeVirtualGap(PageCount, &LastVpnChecked);
        if (AllocatedVa == 0) AllocatedVa = LastVpnChecked << 12;
    }

    if (AllocatedVa < StartingKva ||
        (AllocatedVa + (PageCount * PAGE_SIZE)) <= AllocatedVa ||
        (AllocatedVa + (PageCount * PAGE_SIZE)) >= 0xFFC00000) {
        kdp_print("VMM FATAL: Kernel Virtual Space Exhausted!\n\r");
        KsReleaseSpinLock(&MmVmmLock, OldIrql);
        MmpFreeVadNode(NewVad);
        return 0;
    }

    NewVad->StartingVpn = AllocatedVa >> 12;
    NewVad->EndingVpn = NewVad->StartingVpn + PageCount - 1;

    if (MmpFindOverlappingVad(&MmKernelVadTree, NewVad->StartingVpn, NewVad->EndingVpn)) {
        DPRINT("VMM FATAL: Overlapping kernel VAD [%05X..%05X]!\n",
               NewVad->StartingVpn, NewVad->EndingVpn);
        KsReleaseSpinLock(&MmVmmLock, OldIrql);
        MmpFreeVadNode(NewVad);
        return 0;
    }

    RtlInsertElementAvl(&MmKernelVadTree, (PRTL_BALANCED_NODE)NewVad);
    MmVadRoot = (PMM_VAD)MmKernelVadTree.Root;

    KsReleaseSpinLock(&MmVmmLock, OldIrql);
    return AllocatedVa;
}

VOID 
VEAPI 
MmFreeVirtualRange(ULONG VirtualAddress)
{
    KIRQL OldIrql;
    ULONG VpnTarget = VirtualAddress >> 12;

    KsAcquireSpinLock(&MmVmmLock, &OldIrql);
    PMM_VAD VadToFree = MmpFindVadByVpn(MmKernelVadTree.Root, VpnTarget);
    if (!VadToFree) {
        kdp_print("VMM WARNING: Tried to free unallocated virtual address!\n\r");
        KsReleaseSpinLock(&MmVmmLock, OldIrql);
        return;
    }

    if (VpnTarget != VadToFree->StartingVpn) {
        kdp_print("VMM WARNING: Tried to free from middle of a VAD!\n\r");
        KsReleaseSpinLock(&MmVmmLock, OldIrql);
        return;
    }

    ULONG PageCount = (VadToFree->EndingVpn - VadToFree->StartingVpn) + 1;
    ULONG CurrentVa = VadToFree->StartingVpn << 12;

    for (ULONG i = 0; i < PageCount; i++, CurrentVa += 4096) {
        if (!MM_GET_PDE(CurrentVa)->Hard.Valid) {
            continue;
        }

        PMMPTE pte = MM_GET_PTE(CurrentVa);
        
        if (pte->Hard.Valid) {
            ULONG PhysAddress = pte->Hard.PageFrameNumber << 12;

            // Hapus pemetaan di Page Table Entry
            pte->Hard.Valid = 0;
            pte->Hard.PageFrameNumber = 0;

            // Beri tahu CPU bahwa cache alamat ini udah basi
            MMI_INVALIDATE_TLB(CurrentVa);

            // Recycle the frame only after the old mapping has been invalidated.
            MmFreePhysicalPage(PhysAddress);
        }
    }

    RtlDeleteElementAvl(&MmKernelVadTree, (PRTL_BALANCED_NODE)VadToFree);

    MmVadRoot = (PMM_VAD)MmKernelVadTree.Root;
    KsReleaseSpinLock(&MmVmmLock, OldIrql);

    MmpFreeVadNode(VadToFree);
}

ULONG
VEAPI
MmGetVirtualRangePageCount(ULONG VirtualAddress)
{
    KIRQL OldIrql;
    ULONG PageCount = 0;

    if (VirtualAddress & (PAGE_SIZE - 1))
    {
        return 0;
    }

    KsAcquireSpinLock(&MmVmmLock, &OldIrql);

    PMM_VAD Vad = MmpFindVadByVpn(
        MmKernelVadTree.Root,
        VirtualAddress >> 12
    );
    if (Vad && Vad->StartingVpn == (VirtualAddress >> 12))
    {
        PageCount = Vad->EndingVpn - Vad->StartingVpn + 1;
    }

    KsReleaseSpinLock(&MmVmmLock, OldIrql);
    return PageCount;
}

LONG VEAPI MmpCompareBasedNode(PRTL_BALANCED_NODE NodeA, PRTL_BALANCED_NODE NodeB)
{
    PSECTION SecA = (PSECTION)NodeA;
    PSECTION SecB = (PSECTION)NodeB;

    if (SecA->Address.StartingVpn < SecB->Address.StartingVpn) return -1;
    if (SecA->Address.StartingVpn > SecB->Address.StartingVpn) return 1;
    return 0;
}

VEASTATUS
VEAPI
MiFindEmptyAddressRangeDownBasedTree(
    IN SIZE_T Size,
    IN ULONG_PTR HighestAddress,
    IN ULONG Alignment,
    IN PRTL_AVL_TREE Tree,
    OUT ULONG_PTR *BaseAddress
)
{
    ULONG_PTR TargetAddress = HighestAddress - Size;
    TargetAddress &= ~(Alignment - 1);

    if (!Tree->Root)
    {
        *BaseAddress = TargetAddress;
        return STATUS_SUCCESS;
    }

    // Cari gap kosong dengan mengiterasi node dari kanan ke kiri (Top-Down)
    PRTL_BALANCED_NODE Current = Tree->Root;
    ULONG_PTR Candidate = TargetAddress;

    while (Current)
    {
        PSECTION Sec = (PSECTION)Current;
        ULONG_PTR SecStart = Sec->Address.StartingVpn;
        ULONG_PTR SecEnd = Sec->Address.EndingVpn;

        // Jika bentrok dengan section yang ada, geser alamat ke bawah section ini
        if (Candidate < SecEnd && (Candidate + Size) > SecStart)
        {
            Candidate = (SecStart - Size) & ~(Alignment - 1);
            Current = Tree->Root; // Reset pencarian dari root untuk alamat baru
            continue;
        }

        Current = Current->Left;
    }

    if (Candidate < 0x00010000) // Di bawah 64KB User Space
    {
        return STATUS_NO_MEMORY;
    }

    *BaseAddress = Candidate;
    return STATUS_SUCCESS;
}

VOID
VEAPI
MiInsertBasedSection(IN PSECTION Section)
{
    if (!MmSectionBasedTree.CompareRoutine)
    {
        MmSectionBasedTree.CompareRoutine = MmpCompareBasedNode;
    }

    RtlInsertElementAvl(&MmSectionBasedTree, (PRTL_BALANCED_NODE)Section);
}

VOID
VEAPI
MmpInitializeProcessVadTree(IN PEPROCESS Process)
{
    Process->VadRoot.Root = NULL;
    Process->VadRoot.Leftmost = NULL;
    Process->VadRoot.CompareRoutine = MmpCompareVad;
    KsInitializeSpinlock(&Process->VadLock);
    Process->VmDeleted = FALSE;
    Process->CommitCharge = 0;
    Process->CommitChargePeak = 0;
    Process->AddressSpaceInitialized = TRUE;
}

PRTL_AVL_TREE
VEAPI
MmpGetVadTree(IN PEPROCESS Process, IN ULONG Vpn, OUT PKSPIN_LOCK *Lock)
{
    if (!MM_VPN_IS_USER(Vpn))
    {
        *Lock = &MmVmmLock;
        return &MmKernelVadTree;
    }
 
    if (Process == NULL || !Process->AddressSpaceInitialized)
    {
        *Lock = NULL;
        return NULL;
    }
 
    *Lock = &Process->VadLock;
    return &Process->VadRoot;
}

PMM_VAD
VEAPI
MmpFindOverlappingVad(IN PRTL_AVL_TREE Tree, IN ULONG StartVpn, IN ULONG EndVpn)
{
    PRTL_BALANCED_NODE Node = Tree->Root;
 
    while (Node)
    {
        PMM_VAD Vad = (PMM_VAD)Node;
 
        if (StartVpn <= Vad->EndingVpn && EndVpn >= Vad->StartingVpn)
        {
            return Vad;
        }
 
        Node = (EndVpn < Vad->StartingVpn) ? Node->Left : Node->Right;
    }
 
    return NULL;
}

static
ULONG
VEAPI
MmpFindGapInTree(IN PRTL_AVL_TREE Tree, IN ULONG PageCount,
                 IN ULONG LowVpn, IN ULONG HighVpnExclusive, IN ULONG AlignPages)
{
    ULONG Candidate = (LowVpn + AlignPages - 1) & ~(AlignPages - 1);
    PRTL_BALANCED_NODE Node = MmpGetFirstAvlNode(Tree);
 
    while (Node)
    {
        PMM_VAD Vad = (PMM_VAD)Node;
 
        if (Vad->EndingVpn >= Candidate)
        {
            if (Vad->StartingVpn > Candidate &&
                (Vad->StartingVpn - Candidate) >= PageCount)
            {
                break;  /* celah sebelum VAD ini cukup */
            }
 
            Candidate = (Vad->EndingVpn + 1 + AlignPages - 1) & ~(AlignPages - 1);
        }
 
        Node = RtlGetNextNodeAvl(Node);
    }
 
    if (Candidate + PageCount < Candidate ||            /* overflow */
        Candidate + PageCount > HighVpnExclusive)
    {
        return 0;
    }
 
    return Candidate;
}

VEASTATUS
VEAPI
MmpInsertVadAt(IN PRTL_AVL_TREE Tree, IN PKSPIN_LOCK Lock, IN PMM_VAD Vad)
{
    KIRQL OldIrql;
 
    KsAcquireSpinLock(Lock, &OldIrql);
 
    if (MmpFindOverlappingVad(Tree, Vad->StartingVpn, Vad->EndingVpn))
    {
        KsReleaseSpinLock(Lock, OldIrql);
        return STATUS_CONFLICTING_ADDRESSES;
    }
 
    RtlInsertElementAvl(Tree, (PRTL_BALANCED_NODE)Vad);
    if (Tree == &MmKernelVadTree)
    {
        MmVadRoot = (PMM_VAD)MmKernelVadTree.Root;
    }
 
    KsReleaseSpinLock(Lock, OldIrql);
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
MmpInsertVadAnywhere(IN PRTL_AVL_TREE Tree, IN PKSPIN_LOCK Lock,
                     IN PMM_VAD Vad, IN ULONG PageCount,
                     IN ULONG LowVpn, IN ULONG HighVpnExclusive,
                     IN ULONG AlignPages)
{
    KIRQL OldIrql;
    ULONG Vpn;
 
    if (PageCount == 0 || AlignPages == 0 || (AlignPages & (AlignPages - 1)))
    {
        return STATUS_INVALID_PARAMETER;
    }
 
    KsAcquireSpinLock(Lock, &OldIrql);
 
    Vpn = MmpFindGapInTree(Tree, PageCount, LowVpn, HighVpnExclusive, AlignPages);
    if (Vpn == 0)
    {
        KsReleaseSpinLock(Lock, OldIrql);
        return STATUS_NO_MEMORY;
    }
 
    Vad->StartingVpn = Vpn;
    Vad->EndingVpn = Vpn + PageCount - 1;
 
    RtlInsertElementAvl(Tree, (PRTL_BALANCED_NODE)Vad);
    if (Tree == &MmKernelVadTree)
    {
        MmVadRoot = (PMM_VAD)MmKernelVadTree.Root;
    }
 
    KsReleaseSpinLock(Lock, OldIrql);
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
MmCopyVirtualMemory(
    IN PEPROCESS SourceProcess,
    IN PVOID SourceAddress,
    IN PEPROCESS TargetProcess,
    IN PVOID TargetAddress,
    IN SIZE_T BufferSize,
    IN KPROCESSOR_MODE PreviousMode,
    OUT PSIZE_T NumberOfBytesCopied
)
{
    PEPROCESS CurrentProcess = (PEPROCESS)PtGetCurrentProcess();
    PVOID KernelBuffer = NULL;
    SIZE_T AllocSize = 0;
    SIZE_T BytesRemaining = BufferSize;
    SIZE_T TotalCopied = 0;
    PUCHAR SrcPtr = (PUCHAR)SourceAddress;
    PUCHAR DstPtr = (PUCHAR)TargetAddress;

    if (BufferSize == 0)
    {
        if (NumberOfBytesCopied) *NumberOfBytesCopied = 0;
        return STATUS_SUCCESS;
    }

    // Alokasikan transit buffer di NonPagedPool (Maksimal 64KB per chunk)
    // Memori NonPagedPool (0xC0000000+) terlihat identik di SELURUH CR3 proses!
    AllocSize = min(BufferSize, 64 * 1024);
    KernelBuffer = UlAllocatePoolWithTag(NonPagedPool, AllocSize, 'pCmM');
    if (!KernelBuffer)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    while (BytesRemaining > 0)
    {
        SIZE_T CurrentChunk = min(BytesRemaining, AllocSize);

        if (SourceProcess != CurrentProcess)
        {
            PtAttachProcess((PKPROCESS)SourceProcess);
        }

        RtlCopyMemory(KernelBuffer, SrcPtr, CurrentChunk);

        if (SourceProcess != CurrentProcess)
        {
            PtDetachProcess();
        }

        if (TargetProcess != CurrentProcess)
        {
            PtAttachProcess((PKPROCESS)TargetProcess);
        }

        RtlCopyMemory(DstPtr, KernelBuffer, CurrentChunk);

        if (TargetProcess != CurrentProcess)
        {
            PtDetachProcess();
        }

        SrcPtr += CurrentChunk;
        DstPtr += CurrentChunk;
        BytesRemaining -= CurrentChunk;
        TotalCopied += CurrentChunk;
    }

    UlFreePool(KernelBuffer);

    if (NumberOfBytesCopied)
    {
        *NumberOfBytesCopied = TotalCopied;
    }

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
VeaAllocateVirtualMemory(
    IN HANDLE ProcessHandle,
    IN OUT PVOID *UBaseAddress,
    IN ULONG_PTR ZeroBits,
    IN OUT PSIZE_T URegionSize,
    IN ULONG AllocationType,
    IN ULONG Protect
)
{
    PEPROCESS Process = NULL;
    PMM_VAD Vad = NULL, FoundVad = NULL;
    VEASTATUS Status = STATUS_SUCCESS;
    PVOID PBaseAddress = NULL;
    SIZE_T PRegionSize = 0;
    ULONG_PTR StartingAddress = 0, EndingAddress = 0;
    ULONG PageCount = 0;
    ULONG ProtectionMask = 0;
    BOOLEAN Attached = FALSE;
    PEPROCESS CurrentProcess = (PEPROCESS)PtGetCurrentProcess();
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();

    /* 1. Validasi Parameter Dasar */
    if (!UBaseAddress || !URegionSize)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (ZeroBits > 21)
    {
        return STATUS_INVALID_PARAMETER_3;
    }

    if (AllocationType & ~(MEM_COMMIT | MEM_RESERVE | MEM_RESET | MEM_TOP_DOWN | MEM_LARGE_PAGES))
    {
        return STATUS_INVALID_PARAMETER_5;
    }

    if (!(AllocationType & (MEM_COMMIT | MEM_RESERVE | MEM_RESET)))
    {
        return STATUS_INVALID_PARAMETER_5;
    }

    /* 2. Konversi Proteksi ke Internal MM Mask */
    ProtectionMask = MiMakeProtectionMask(Protect);
    if (ProtectionMask == MM_INVALID_PROTECTION)
    {
        return STATUS_INVALID_PAGE_PROTECTION;
    }

    /* 3. Salin Parameter Input dengan Aman sesuai PreviousMode */
    if (PreviousMode == UserMode)
    {
        if ((ULONG_PTR)UBaseAddress >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS ||
            (ULONG_PTR)URegionSize  >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS)
        {
            return STATUS_ACCESS_VIOLATION;
        }

        _VEA_TRY
        {
            PBaseAddress = *UBaseAddress;
            PRegionSize  = *URegionSize;
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            return GetExceptionCode();
        }
        _VEA_END;
    }
    else
    {
        PBaseAddress = *UBaseAddress;
        PRegionSize  = *URegionSize;
    }

    if (PRegionSize == 0)
    {
        return STATUS_INVALID_PARAMETER_4;
    }

    /* Pastikan alokasi tetap berada di dalam User Space */
    if ((ULONG_PTR)PBaseAddress > (ULONG_PTR)MM_HIGHEST_USER_ADDRESS)
    {
        return STATUS_INVALID_PARAMETER_2;
    }

    if (((ULONG_PTR)MM_HIGHEST_USER_ADDRESS + 1 - (ULONG_PTR)PBaseAddress) < PRegionSize)
    {
        return STATUS_INVALID_PARAMETER_4;
    }

    /* 4. Dapatkan Object EPROCESS & Attach CR3 jika Beda Proses */
    if (ProcessHandle == NtCurrentProcess() || ProcessHandle == (HANDLE)-1)
    {
        Process = CurrentProcess;
    }
    else
    {
        Status = ObReferenceObjectByHandle(
            ProcessHandle,
            PROCESS_VM_OPERATION,
            PtEprocessType,
            PreviousMode,
            (PVOID*)&Process
        );
        if (!VEA_SUCCESS(Status))
        {
            return Status;
        }
    }

    if (!Process)
    {
        return STATUS_INVALID_HANDLE;
    }

    if (CurrentProcess != Process)
    {
        PtAttachProcess((PKPROCESS)Process);
        Attached = TRUE;
    }

    /* -----------------------------------------------------------------
     * JALUR A: RESERVATION BARU (PBaseAddress == NULL Atau MEM_RESERVE)
     * ----------------------------------------------------------------- */
    if (!PBaseAddress || (AllocationType & MEM_RESERVE))
    {
        Vad = MmpAllocateVadNode();
        if (!Vad)
        {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto Exit;
        }

        Vad->u.VadFlags.Protection    = ProtectionMask;
        Vad->u.VadFlags.MemCommit     = (AllocationType & MEM_COMMIT) ? 1 : 0;
        Vad->u.VadFlags.PrivateMemory = 1;
        Vad->u.VadFlags.VadType       = 0;
        Vad->ControlArea              = NULL;

        if (!PBaseAddress)
        {
            PRegionSize = ALIGN_UP_BY(PRegionSize, PAGE_SIZE);
            PageCount = (ULONG)(PRegionSize >> PAGE_SHIFT);

            Status = MmpInsertVadAnywhere(
                &Process->VadRoot,
                &Process->VadLock,
                Vad,
                PageCount,
                MM_LOWEST_USER_VPN,
                MM_HIGHEST_USER_VPN + 1,
                MM_VAD_ALIGN_64K
            );

            if (!VEA_SUCCESS(Status))
            {
                MmpFreeVadNode(Vad);
                goto Exit;
            }

            StartingAddress = (ULONG_PTR)Vad->StartingVpn << PAGE_SHIFT;
        }
        else
        {
            StartingAddress = PAGE_ALIGN_DOWN((ULONG_PTR)PBaseAddress);
            EndingAddress   = PAGE_ROUND_UP((ULONG_PTR)PBaseAddress + PRegionSize) - 1;

            Vad->StartingVpn = StartingAddress >> PAGE_SHIFT;
            Vad->EndingVpn   = EndingAddress >> PAGE_SHIFT;
            PageCount        = (Vad->EndingVpn - Vad->StartingVpn) + 1;
            PRegionSize      = PageCount << PAGE_SHIFT;

            Status = MmpInsertVadAt(&Process->VadRoot, &Process->VadLock, Vad);

            if (!VEA_SUCCESS(Status))
            {
                MmpFreeVadNode(Vad);
                goto Exit;
            }
        }

        if (AllocationType & MEM_COMMIT)
        {
            for (ULONG_PTR va = StartingAddress; va <= StartingAddress + PRegionSize - 1; va += PAGE_SIZE)
            {
                if (MmpEnsurePageTable(va))
                {
                    PMMPTE Pte = MiAddressToPte(va);
                    if (Pte->Hard.Valid && (Pte->Hard.PageFrameNumber < 256))
                    {
                        MI_ERASE_PTE(Pte);
                        MMI_INVALIDATE_TLB(va);
                    }
                }
            }
        }

        Status = STATUS_SUCCESS;
        goto Exit;
    }

    /* -----------------------------------------------------------------
     * JALUR B: COMMIT PADA VAD YANG SUDAH DI-RESERVE SEBELUMNYA
     * ----------------------------------------------------------------- */
    StartingAddress = PAGE_ALIGN_DOWN((ULONG_PTR)PBaseAddress);
    EndingAddress   = PAGE_ROUND_UP((ULONG_PTR)PBaseAddress + PRegionSize) - 1;
    PRegionSize     = (EndingAddress - StartingAddress) + 1;

    FoundVad = MmpFindOverlappingVad(
        &Process->VadRoot,
        StartingAddress >> PAGE_SHIFT,
        EndingAddress >> PAGE_SHIFT
    );

    if (!FoundVad)
    {
        Status = STATUS_CONFLICTING_ADDRESSES;
        goto Exit;
    }

    if ((StartingAddress >> PAGE_SHIFT) < FoundVad->StartingVpn ||
        (EndingAddress >> PAGE_SHIFT) > FoundVad->EndingVpn)
    {
        Status = STATUS_CONFLICTING_ADDRESSES;
        goto Exit;
    }

    FoundVad->u.VadFlags.MemCommit = 1;
    FoundVad->u.VadFlags.Protection = ProtectionMask;

    for (ULONG_PTR va = StartingAddress; va <= EndingAddress; va += PAGE_SIZE)
    {
        if (MmpEnsurePageTable(va))
        {
            PMMPTE Pte = MiAddressToPte(va);
            if (Pte->Hard.Valid && (Pte->Hard.PageFrameNumber < 256))
            {
                MI_ERASE_PTE(Pte);
                MMI_INVALIDATE_TLB(va);
            }
        }
    }

    Status = STATUS_SUCCESS;

Exit:
    /* Kembalikan CR3 ke proses pemanggil (smss.elf) SEBELUM menyentuh pointer output */
    if (Attached)
    {
        PtDetachProcess();
    }

    if (Process && ProcessHandle != NtCurrentProcess() && ProcessHandle != (HANDLE)-1)
    {
        ObDereferenceObject(Process);
    }

    /* Tulis kembali hasil alokasi ke buffer pemanggil jika sukses */
    if (VEA_SUCCESS(Status))
    {
        if (PreviousMode == UserMode)
        {
            _VEA_TRY
            {
                *UBaseAddress = (PVOID)StartingAddress;
                *URegionSize  = PRegionSize;
            }
            _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
            {
                Status = GetExceptionCode();
            }
            _VEA_END;
        }
        else
        {
            *UBaseAddress = (PVOID)StartingAddress;
            *URegionSize  = PRegionSize;
        }
    }

    return Status;
}

VEASTATUS
VEAPI
VeaReadVirtualMemory(
    IN HANDLE ProcessHandle,
    IN PVOID BaseAddress,
    OUT PVOID Buffer,
    IN SIZE_T NumberOfBytesToRead,
    OUT PSIZE_T NumberOfBytesRead OPTIONAL
)
{
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();
    PEPROCESS TargetProcess = NULL;
    VEASTATUS Status = STATUS_SUCCESS;
    SIZE_T BytesRead = 0;

    // 1. Validasi Pointer & Bound Checking untuk User Mode Callers
    if (PreviousMode != KernelMode)
    {
        // Cegah Integer Overflow & Pembacaan ke Area Kernel (>= 0xC0000000)
        if ((((ULONG_PTR)BaseAddress + NumberOfBytesToRead) < (ULONG_PTR)BaseAddress) ||
            (((ULONG_PTR)Buffer + NumberOfBytesToRead) < (ULONG_PTR)Buffer) ||
            (((ULONG_PTR)BaseAddress + NumberOfBytesToRead) > 0xC0000000) ||
            (((ULONG_PTR)Buffer + NumberOfBytesToRead) > 0xC0000000))
        {
            return STATUS_ACCESS_VIOLATION;
        }
    }

    if (NumberOfBytesToRead == 0)
    {
        if (NumberOfBytesRead) *NumberOfBytesRead = 0;
        return STATUS_SUCCESS;
    }

    // 2. Dapatkan Object EPROCESS Target dari Handle Table
    if (ProcessHandle == NtCurrentProcess() || ProcessHandle == (HANDLE)-1)
    {
        TargetProcess = (PEPROCESS)PtGetCurrentProcess();
    }
    else
    {
        Status = ObReferenceObjectByHandle(
            ProcessHandle,
            PROCESS_VM_READ,
            PtEprocessType,
            PreviousMode,
            (PVOID*)&TargetProcess
        );
        if (!VEA_SUCCESS(Status)) return Status;
    }

    // 3. Eksekusi Pembacaan Memori
    // Source: TargetProcess (BaseAddress) -> Target: CurrentProcess (Buffer)
    Status = MmCopyVirtualMemory(
        TargetProcess,
        BaseAddress,
        (PEPROCESS)PtGetCurrentProcess(),
        Buffer,
        NumberOfBytesToRead,
        PreviousMode,
        &BytesRead
    );

    if (ProcessHandle != NtCurrentProcess() && ProcessHandle != (HANDLE)-1)
    {
        ObDereferenceObject(TargetProcess);
    }

    if (VEA_SUCCESS(Status) && NumberOfBytesRead)
    {
        *NumberOfBytesRead = BytesRead;
    }

    return Status;
}

VEASTATUS
VEAPI
VeaWriteVirtualMemory(
    IN HANDLE ProcessHandle,
    IN PVOID BaseAddress,
    IN PVOID Buffer,
    IN SIZE_T NumberOfBytesToWrite,
    OUT PSIZE_T NumberOfBytesWritten OPTIONAL
)
{
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();
    PEPROCESS TargetProcess = NULL;
    VEASTATUS Status = STATUS_SUCCESS;
    SIZE_T BytesWritten = 0;

    // 1. Validasi Pointer & Bound Checking untuk User Mode Callers
    if (PreviousMode != KernelMode)
    {
        // Cegah Integer Overflow & Penulisan ke Area Kernel (>= 0xC0000000)
        if ((((ULONG_PTR)BaseAddress + NumberOfBytesToWrite) < (ULONG_PTR)BaseAddress) ||
            (((ULONG_PTR)Buffer + NumberOfBytesToWrite) < (ULONG_PTR)Buffer) ||
            (((ULONG_PTR)BaseAddress + NumberOfBytesToWrite) > 0xC0000000) ||
            (((ULONG_PTR)Buffer + NumberOfBytesToWrite) > 0xC0000000))
        {
            return STATUS_ACCESS_VIOLATION;
        }
    }

    if (NumberOfBytesToWrite == 0)
    {
        if (NumberOfBytesWritten) *NumberOfBytesWritten = 0;
        return STATUS_SUCCESS;
    }

    // 2. Dapatkan Object EPROCESS Target dari Handle Table
    if (ProcessHandle == NtCurrentProcess() || ProcessHandle == (HANDLE)-1)
    {
        TargetProcess = (PEPROCESS)PtGetCurrentProcess();
    }
    else
    {
        Status = ObReferenceObjectByHandle(
            ProcessHandle,
            PROCESS_VM_WRITE,
            PtEprocessType,
            PreviousMode,
            (PVOID*)&TargetProcess
        );
        if (!VEA_SUCCESS(Status)) return Status;
    }

    // 3. Eksekusi Penulisan Memori
    // Source: CurrentProcess (Buffer) -> Target: TargetProcess (BaseAddress)
    Status = MmCopyVirtualMemory(
        (PEPROCESS)PtGetCurrentProcess(),
        Buffer,
        TargetProcess,
        BaseAddress,
        NumberOfBytesToWrite,
        PreviousMode,
        &BytesWritten
    );

    if (ProcessHandle != NtCurrentProcess() && ProcessHandle != (HANDLE)-1)
    {
        ObDereferenceObject(TargetProcess);
    }

    if (VEA_SUCCESS(Status) && NumberOfBytesWritten)
    {
        *NumberOfBytesWritten = BytesWritten;
    }

    return Status;
}

static
VOID
VEAPI
MmpUnmapAndFreePageRange(
    IN ULONG_PTR StartVa,
    IN ULONG_PTR EndVa
)
{
    for (ULONG_PTR va = StartVa; va <= EndVa; va += PAGE_SIZE)
    {
        PMMPDE Pde = MiAddressToPde(va);
        if (!Pde->Hard.Valid) continue;

        PMMPTE Pte = MiAddressToPte(va);
        if (Pte->Hard.Valid)
        {
            ULONG PhysPage = Pte->Hard.PageFrameNumber << PAGE_SHIFT;

            MI_ERASE_PTE(Pte);
            MMI_INVALIDATE_TLB(va);
            MmFreePhysicalPage(PhysPage);
        }
        else if (Pte->Long != 0)
        {
            MI_ERASE_PTE(Pte);
            MMI_INVALIDATE_TLB(va);
        }
    }
}

VEASTATUS
VEAPI
VeaFreeVirtualMemory(
    IN HANDLE ProcessHandle,
    IN OUT PVOID *UBaseAddress,
    IN OUT PSIZE_T URegionSize,
    IN ULONG FreeType
)
{
    PEPROCESS Process = NULL;
    PMM_VAD Vad = NULL, NewVad = NULL;
    VEASTATUS Status = STATUS_SUCCESS;
    PVOID PBaseAddress = NULL;
    SIZE_T PRegionSize = 0;
    ULONG_PTR StartingAddress = 0, EndingAddress = 0;
    ULONG_PTR StartVpn = 0, EndVpn = 0;
    BOOLEAN Attached = FALSE;
    KIRQL OldIrql;
    PEPROCESS CurrentProcess = (PEPROCESS)PtGetCurrentProcess();
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();

    /* 1. Validasi Parameter FreeType (Harus MEM_RELEASE atau MEM_DECOMMIT secara eksklusif) */
    if ((FreeType != MEM_RELEASE && FreeType != MEM_DECOMMIT) || (FreeType == (MEM_RELEASE | MEM_DECOMMIT)))
    {
        return STATUS_INVALID_PARAMETER_4;
    }

    if (!UBaseAddress || !URegionSize)
    {
        return STATUS_INVALID_PARAMETER;
    }

    PBaseAddress = *UBaseAddress;
    PRegionSize = *URegionSize;

    /* MEM_RELEASE wajib menentukan RegionSize 0 (Free All) atau BaseAddress yang valid */
    if ((FreeType & MEM_RELEASE) && (PRegionSize == 0) && (PBaseAddress == NULL))
    {
        return STATUS_INVALID_PARAMETER_2;
    }

    /* Validasi rentang alamat User Space (< 0xC0000000) */
    if ((ULONG_PTR)PBaseAddress >= 0xC0000000)
    {
        return STATUS_INVALID_PARAMETER_2;
    }

    if ((0xC0000000 - (ULONG_PTR)PBaseAddress) < PRegionSize)
    {
        return STATUS_INVALID_PARAMETER_3;
    }

    /* 2. Dapatkan Object EPROCESS & Attach CR3 jika Beda Proses */
    if (ProcessHandle == NtCurrentProcess() || ProcessHandle == (HANDLE)-1)
    {
        Process = CurrentProcess;
    }
    else
    {
        Status = ObReferenceObjectByHandle(
            ProcessHandle,
            PROCESS_VM_OPERATION,
            PtEprocessType,
            PreviousMode,
            (PVOID*)&Process
        );
        if (!VEA_SUCCESS(Status)) return Status;
    }

    if (CurrentProcess != Process)
    {
        PtAttachProcess((PKPROCESS)Process);
        Attached = TRUE;
    }

    /* 3. Hitung Alamat Awal & Cari Node VAD yang Menaunginya */
    StartingAddress = PAGE_ALIGN_DOWN((ULONG_PTR)PBaseAddress);
    StartVpn = StartingAddress >> PAGE_SHIFT;

    KsAcquireSpinLock(&Process->VadLock, &OldIrql);
    Vad = MmpFindVadByVpn(Process->VadRoot.Root, StartVpn);
    KsReleaseSpinLock(&Process->VadLock, OldIrql);

    if (!Vad)
    {
        DPRINT("[MM/FREE] ERROR: No VAD found at address 0x%08X!\n", StartingAddress);
        Status = STATUS_MEMORY_NOT_ALLOCATED;
        goto Exit;
    }

    /* Memori yang bisa di-free via API ini wajib Private Memory & Bukan VAD Terproteksi */
    if (!Vad->u.VadFlags.PrivateMemory || Vad->u.VadFlags.NoChange)
    {
        DPRINT("[MM/FREE] ERROR: Cannot free protected or section-backed VAD!\n");
        Status = STATUS_UNABLE_TO_DELETE_SECTION;
        goto Exit;
    }

    // =========================================================================
    // JALUR A: MEM_DECOMMIT (Membebaskan RAM Fisik, Tapi VAD Tetap Ada)
    // =========================================================================
    if (FreeType & MEM_DECOMMIT)
    {
        if (PRegionSize == 0)
        {
            if (StartVpn != Vad->StartingVpn)
            {
                Status = STATUS_FREE_VM_NOT_AT_BASE;
                goto Exit;
            }
            EndingAddress = (Vad->EndingVpn << PAGE_SHIFT) | (PAGE_SIZE - 1);
        }
        else
        {
            EndingAddress = PAGE_ROUND_UP((ULONG_PTR)PBaseAddress + PRegionSize) - 1;
        }

        EndVpn = EndingAddress >> PAGE_SHIFT;
        if (EndVpn > Vad->EndingVpn)
        {
            Status = STATUS_UNABLE_TO_FREE_VM;
            goto Exit;
        }

        /* Unmap physical RAM pages & clear PTEs */
        MmpUnmapAndFreePageRange(StartingAddress, EndingAddress);

        PRegionSize = (EndingAddress - StartingAddress) + 1;
        *UBaseAddress = (PVOID)StartingAddress;
        *URegionSize = PRegionSize;
        Status = STATUS_SUCCESS;
        goto Exit;
    }

    // =========================================================================
    // JALUR B: MEM_RELEASE (Membebaskan Virtual Address & Menghapus/Memotong VAD)
    // =========================================================================
    if (FreeType & MEM_RELEASE)
    {
        if (PRegionSize == 0)
        {
            /* Jika RegionSize == 0, seluruh VAD harus dihapus */
            if (StartVpn != Vad->StartingVpn)
            {
                Status = STATUS_FREE_VM_NOT_AT_BASE;
                goto Exit;
            }

            StartingAddress = Vad->StartingVpn << PAGE_SHIFT;
            EndingAddress = (Vad->EndingVpn << PAGE_SHIFT) | (PAGE_SIZE - 1);

            /* 1. Unmap seluruh fisik RAM */
            MmpUnmapAndFreePageRange(StartingAddress, EndingAddress);

            /* 2. Cabut Node VAD dari AVL Tree */
            KsAcquireSpinLock(&Process->VadLock, &OldIrql);
            RtlDeleteElementAvl(&Process->VadRoot, (PRTL_BALANCED_NODE)Vad);
            KsReleaseSpinLock(&Process->VadLock, OldIrql);

            /* 3. Kembalikan struct VAD ke pool */
            MmpFreeVadNode(Vad);
        }
        else
        {
            EndingAddress = PAGE_ROUND_UP((ULONG_PTR)PBaseAddress + PRegionSize) - 1;
            EndVpn = EndingAddress >> PAGE_SHIFT;

            if (EndVpn > Vad->EndingVpn)
            {
                Status = STATUS_UNABLE_TO_FREE_VM;
                goto Exit;
            }

            /* --- SKENARIO 1: FULL VAD RELEASE (CASE D) --- */
            if (StartVpn == Vad->StartingVpn && EndVpn == Vad->EndingVpn)
            {
                MmpUnmapAndFreePageRange(StartingAddress, EndingAddress);

                KsAcquireSpinLock(&Process->VadLock, &OldIrql);
                RtlDeleteElementAvl(&Process->VadRoot, (PRTL_BALANCED_NODE)Vad);
                KsReleaseSpinLock(&Process->VadLock, OldIrql);

                MmpFreeVadNode(Vad);
            }
            /* --- SKENARIO 2: HEAD TRIM (CASE A: Potong Bagian Depan VAD) --- */
            else if (StartVpn == Vad->StartingVpn)
            {
                MmpUnmapAndFreePageRange(StartingAddress, EndingAddress);

                KsAcquireSpinLock(&Process->VadLock, &OldIrql);
                Vad->StartingVpn = EndVpn + 1;
                KsReleaseSpinLock(&Process->VadLock, OldIrql);
            }
            /* --- SKENARIO 3: TAIL TRIM (CASE C: Potong Bagian Belakang VAD) --- */
            else if (EndVpn == Vad->EndingVpn)
            {
                MmpUnmapAndFreePageRange(StartingAddress, EndingAddress);

                KsAcquireSpinLock(&Process->VadLock, &OldIrql);
                Vad->EndingVpn = StartVpn - 1;
                KsReleaseSpinLock(&Process->VadLock, OldIrql);
            }
            /* --- SKENARIO 4: MIDDLE SPLIT (CASE B: Potong Tengah & Pecah Jadi 2 VAD) --- */
            else
            {
                NewVad = MmpAllocateVadNode();
                if (!NewVad)
                {
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    goto Exit;
                }

                /* Setup VAD baru untuk bagian pecahan belakang */
                NewVad->StartingVpn = EndVpn + 1;
                NewVad->EndingVpn   = Vad->EndingVpn;
                NewVad->u.LongFlags = Vad->u.LongFlags;
                NewVad->u2.LongFlags2 = Vad->u2.LongFlags2;
                NewVad->ControlArea = NULL;

                /* Unmap RAM fisik bagian tengah */
                MmpUnmapAndFreePageRange(StartingAddress, EndingAddress);

                /* Potong bagian belakang VAD lama, lalu insert VAD baru */
                KsAcquireSpinLock(&Process->VadLock, &OldIrql);
                Vad->EndingVpn = StartVpn - 1;
                RtlInsertElementAvl(&Process->VadRoot, (PRTL_BALANCED_NODE)NewVad);
                KsReleaseSpinLock(&Process->VadLock, OldIrql);
            }
        }

        PRegionSize = (EndingAddress - StartingAddress) + 1;
        *UBaseAddress = (PVOID)StartingAddress;
        *URegionSize = PRegionSize;
        Status = STATUS_SUCCESS;
    }

Exit:
    if (Attached)
    {
        PtDetachProcess();
    }

    if (Process && ProcessHandle != NtCurrentProcess() && ProcessHandle != (HANDLE)-1)
    {
        ObDereferenceObject(Process);
    }

    return Status;
}

/* EOF */