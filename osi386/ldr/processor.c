/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : processor.c
   CREATOR     : RasyaFr-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : For setting KIPCR
 ----- Effective since 07-2026 til FOREVER ------------------------------- */

#include <osi386.h>

/* Revision History ------------------------------------------------------
 * DATE       : 26-07-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file processor.c
 * --------------------------------------------------------------------- */

VOID
VEAPI
LdrSetFsSegmentBase(IN ULONG Base, IN ULONG Limit)
{
    KDESCRIPTOR Gdtr;

    asm volatile("sgdt %0" : "=m"(Gdtr));

    PKGDTENTRY32 GdtTable = (PKGDTENTRY32)Gdtr.Base;
    PKGDTENTRY32 FsEntry = &GdtTable[KGDT32_R0_PCR >> 3];

    // Fill FS GDT info
    FsEntry->BaseLow             = (USHORT)(Base & 0xFFFF);
    FsEntry->HighWord.Bits.BaseMid = (UCHAR)((Base >> 16) & 0xFF);
    FsEntry->HighWord.Bits.BaseHi  = (UCHAR)((Base >> 24) & 0xFF);
    FsEntry->LimitLow            = (USHORT)(Limit & 0xFFFF);
    FsEntry->HighWord.Bits.LimitHi = (UCHAR)((Limit >> 16) & 0x0F);
    FsEntry->HighWord.Bits.Type        = 2; // Read/Write Data Segment
    FsEntry->HighWord.Bits.System      = 1; // 1 = Code/Data Segment
    FsEntry->HighWord.Bits.Dpl         = 0; // Ring 0 (Kernel Privilege)
    FsEntry->HighWord.Bits.Present     = 1; // Valid Segment
    FsEntry->HighWord.Bits.Avl         = 0;
    FsEntry->HighWord.Bits.LongMode    = 0; // 32-bit Mode (Bukan 64-bit)
    FsEntry->HighWord.Bits.DefaultBig  = 1; // 32-bit Operation Size
    FsEntry->HighWord.Bits.Granularity = 0; // Byte Granularity (Bukan 4KB Pages)

    // Reload FS
    asm volatile(
        "mov %0, %%fs"
        :
        : "r" ((USHORT)KGDT32_R0_PCR)
        : "memory"
    );
}

VOID
VEAPI
LdrBuildProcessorControlBlock(IN OUT BLOCK_BOOT_2 *BlockBoot2)
{
    PKPRCB Prcb;
    PKPCR Kpcr;
    PKIPCR Kipcr;

    Kipcr = (PKIPCR)LmAllocatePool(LdrUnreclaimablePool, sizeof(KIPCR));
    if(!Kipcr)
    {
        LdrError(STATUS_INSUFFICIENT_RESOURCES);
        return;
    }

    cmemset(Kipcr, 0, sizeof(KIPCR));

    Kpcr = &Kipcr->Pcr;
    Prcb = &Kipcr->PrcbData;

    Kpcr->SelfPcr = Kpcr;        // Digunakan oleh KeGetPcr() via fs:[0x1C]
    Kpcr->Prcb = Prcb;           // Link dari KPCR ke KPRCB

    // ==========================================
    // 5. INISIALISASI KPCR (Public Region)
    // ==========================================
    Kpcr->Number = 0;            // Boot Processor (CPU #0)
    Kpcr->SetMember = 1;         // Bitmask CPU (1 << 0)
    Kpcr->SetMemberCopy = 1;
    Kpcr->Irql = 0;              // Start di PASSIVE_LEVEL (0)

    Kpcr->MajorVersion = 1;      // Version OS kamu
    Kpcr->MinorVersion = 0;

    // ==========================================
    // 6. INISIALISASI KPRCB (Private Scheduler)
    // ==========================================
    Prcb->Number = 0;
    Prcb->SetMember = 1;
    
    // Inisialisasi Antrean DPC (Circular Double Linked List)
    Prcb->DpcListHead.Flink = &Prcb->DpcListHead;
    Prcb->DpcListHead.Blink = &Prcb->DpcListHead;

    // ==========================================
    // 7. SIMPAN POINTER KE BLOCK_BOOT_2
    // ==========================================
    // (Ganti nama field ini sesuai nama variabel KPCR di struct BLOCK_BOOT_2 kamu)
    BlockBoot2->Prcb = (ULONG_PTR)Prcb;

    LdrSetFsSegmentBase((ULONG)Kpcr, sizeof(KIPCR));
}