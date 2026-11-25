/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : stack.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Kernel Stack Allocate
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 22-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file stack.c
 * --------------------------------------------------------------------- */

//
// We made this because of veapi freetype taking out f***ing stack.
// We need to expand the stack to larger stack
// 

extern KSPIN_LOCK MmVmmLock;
extern RTL_AVL_TREE MmKernelVadTree;

PMM_VAD VEAPI MmpFindVadByVpn(PRTL_BALANCED_NODE Node, ULONG Vpn);

PVOID
VEAPI
MmCreateKernelStack(
    IN BOOLEAN GuiStack,
    IN UCHAR Node
)
{
    ULONG StackBytes;
    ULONG StackPages;
    ULONG TotalPages;
    ULONG VadBase;
    ULONG StackLimit;
    ULONG StackBase;
    ULONG CurrentVa;

    UNREFERENCED_PARAMETER(Node);

    StackBytes = GuiStack ? KERNEL_LARGE_STACK_SIZE : KERNEL_STACK_SIZE;
    StackPages = StackBytes >> PAGE_SHIFT;
    TotalPages = StackPages + KERNEL_STACK_GUARD_PAGES;

    /* 1. Reservasi rentang KVA: [GuardPage (1 page)] + [StackPages] */
    VadBase = MmAllocateVirtualRange(TotalPages);
    if (VadBase == 0)
    {
        DPRINT("[MM/KSTACK] ERROR: Failed to reserve KVA (%u pages)!\n", TotalPages);
        return NULL;
    }

    StackLimit = VadBase + (KERNEL_STACK_GUARD_PAGES * PAGE_SIZE);
    StackBase  = StackLimit + StackBytes;

    /* 2. Halaman pertama (VadBase .. StackLimit - 1) sengaja dibiarkan unmapped
     *    sebagai Guard Page agar stack overflow langsung terdeteksi. */
    CurrentVa = StackLimit;
    for (ULONG i = 0; i < StackPages; i++, CurrentVa += PAGE_SIZE)
    {
        ULONG PhysPage = MmAllocatePhysicalPage();
        if (PhysPage == 0 || PhysPage == 0xFFFFFFFFUL)
        {
            DPRINT("[MM/KSTACK] ERROR: Out of physical memory at page %u/%u!\n",
                   i, StackPages);
            MmFreeVirtualRange(VadBase);
            return NULL;
        }

        if (!MmpEnsurePageTable(CurrentVa))
        {
            MmFreePhysicalPage(PhysPage);
            MmFreeVirtualRange(VadBase);
            return NULL;
        }

        PMMPTE Pte = MiAddressToPte(CurrentVa);
        if (Pte->Hard.Valid)
        {
            MmFreePhysicalPage(PhysPage);
            MmFreeVirtualRange(VadBase);
            return NULL;
        }

        MMI_WRITE_PTE_VALID(Pte, PhysPage >> PAGE_SHIFT);
        MMI_INVALIDATE_TLB(CurrentVa);

        /* Bersihkan isi halaman stack baru */
        RtlZeroMemory((PVOID)CurrentVa, PAGE_SIZE);
    }

    return (PVOID)StackBase;
}

VOID
VEAPI
MmDeleteKernelStack(
    IN PVOID StackBase,
    IN BOOLEAN GuiStack
)
{
    ULONG_PTR TopVa;
    ULONG_PTR ProbeVa;
    ULONG VadBase = 0;
    KIRQL OldIrql;

    if (StackBase == NULL)
    {
        return;
    }

    TopVa = (ULONG_PTR)StackBase;
    ProbeVa = TopVa - PAGE_SIZE;

    /* Cari StartingVpn dari VAD yang menaungi puncak stack ini agar akurat
     * berapa pun ukuran stack-nya (normal maupun large). */
    KsAcquireSpinLock(&MmVmmLock, &OldIrql);
    PMM_VAD Vad = MmpFindVadByVpn(MmKernelVadTree.Root, (ULONG)(ProbeVa >> PAGE_SHIFT));
    if (Vad != NULL)
    {
        VadBase = Vad->StartingVpn << PAGE_SHIFT;
    }
    KsReleaseSpinLock(&MmVmmLock, OldIrql);

    if (VadBase == 0)
    {
        ULONG StackBytes = GuiStack ? KERNEL_LARGE_STACK_SIZE : KERNEL_STACK_SIZE;
        VadBase = (ULONG)(TopVa - StackBytes - (KERNEL_STACK_GUARD_PAGES * PAGE_SIZE));
    }

    /* MmFreeVirtualRange di virt.c otomatis:
     * - Melewati Guard Page (karena PTE-nya !Hard.Valid)
     * - Unmap & mengembalikan halaman fisik stack ke PIP
     * - Menghapus VAD dari MmKernelVadTree */
    MmFreeVirtualRange(VadBase);
}

PVOID
VEAPI
KsSwitchKernelStack(
    IN PVOID StackBase,
    IN PVOID StackLimit
)
{
    PKTHREAD Thread = (PKTHREAD)PtGetCurrentThread();
    ULONG_PTR NewStackBase;
    ULONG_PTR NewStackLimit;
    ULONG_PTR OldStackBase;
    ULONG_PTR OldEsp;
    ULONG_PTR OldEbp;
    ULONG_PTR UsedBytes;
    ULONG_PTR NewEsp;
    ULONG_PTR NewEbp;
    LONG_PTR Delta;
    ULONG Eflags;

    if (!Thread || !StackBase || !StackLimit)
    {
        return NULL;
    }

    NewStackBase  = (ULONG_PTR)StackBase;
    NewStackLimit = (ULONG_PTR)StackLimit;

    /* Matikan interupsi selama proses cangkok stack berlangsung */
    __asm__ volatile(
        "pushfl\n\t"
        "popl %0\n\t"
        "cli"
        : "=r"(Eflags)
        :
        : "memory"
    );

    __asm__ volatile(
        "movl %%esp, %0\n\t"
        "movl %%ebp, %1"
        : "=r"(OldEsp), "=r"(OldEbp)
    );

    OldStackBase = (ULONG_PTR)Thread->InitialStack;
    if (OldStackBase == 0)
    {
        OldStackBase = (ULONG_PTR)Thread->KernelStack;
    }

    if (OldEsp >= OldStackBase || (OldStackBase - OldEsp) >= (NewStackBase - NewStackLimit))
    {
        KsBugCheckEx(
            KMODE_EXCEPTION_NOT_HANDLED,
            0xDEAD57AC,
            OldEsp,
            OldStackBase,
            NewStackBase - NewStackLimit
        );
    }

    UsedBytes = OldStackBase - OldEsp;
    Delta     = (LONG_PTR)NewStackBase - (LONG_PTR)OldStackBase;
    NewEsp    = NewStackBase - UsedBytes;
    NewEbp    = (ULONG_PTR)((LONG_PTR)OldEbp + Delta);

    /* 1. Salin seluruh frame aktif (termasuk KTRAP_FRAME di puncak) ke stack baru */
    RtlCopyMemory((PVOID)NewEsp, (PVOID)OldEsp, UsedBytes);

    /* 2. Relokasi setiap Saved EBP di rantai frame kernel agar menunjuk ke stack baru */
    ULONG_PTR FramePtr = NewEbp;
    while (FramePtr >= NewEsp && FramePtr <= NewStackBase - sizeof(ULONG_PTR))
    {
        ULONG_PTR SavedEbp = *(PULONG_PTR)FramePtr;
        if (SavedEbp >= OldEsp && SavedEbp < OldStackBase)
        {
            ULONG_PTR RelocatedEbp = (ULONG_PTR)((LONG_PTR)SavedEbp + Delta);
            *(PULONG_PTR)FramePtr = RelocatedEbp;
            FramePtr = RelocatedEbp;
        }
        else
        {
            /* Sudah mencapai EBP user-mode di dalam KTRAP_FRAME -> berhenti */
            break;
        }
    }

    /* 3. Update metadata stack di KTHREAD & TSS->Esp0 untuk transisi Ring 3 <-> Ring 0 berikutnya */
    Thread->InitialStack = StackBase;
    Thread->KernelStack    = StackBase;
    Thread->StackLimit   = StackLimit;
    Thread->LargeStack   = TRUE;

    if ((ULONG_PTR)Thread->TrapFrame >= OldEsp &&
        (ULONG_PTR)Thread->TrapFrame < OldStackBase)
    {
        Thread->TrapFrame = (PKTRAP_FRAME)((LONG_PTR)Thread->TrapFrame + Delta);
    }

    if (KeGetPcr() && KeGetPcr()->TSS)
    {
        PKTSSENTRY32 Tss = (PKTSSENTRY32)KeGetPcr()->TSS;
        Tss->Esp0 = (ULONG)NewStackBase;
    }

    /* 4. Tukar ESP & EBP secara live ke stack baru, lalu pulihkan EFLAGS */
    __asm__ volatile(
        "movl %0, %%esp\n\t"
        "movl %1, %%ebp\n\t"
        "pushl %2\n\t"
        "popfl"
        :
        : "r"(NewEsp), "r"(NewEbp), "r"(Eflags)
        : "memory"
    );

    return (PVOID)OldStackBase;
}

/* EOF */