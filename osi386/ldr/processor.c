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

VOID VEAPI LdrpReloadSegments(IN PKDESCRIPTOR Gdtr, IN USHORT TssSelector);

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
    PUCHAR DpcStackBuffer = (PUCHAR)LmAllocatePool(LdrUnreclaimablePool, 12 * 1024);
    if (DpcStackBuffer)
    {
        // PENTING: Tambahkan offset 12 KB agar menunjuk ke Top of Stack!
        Prcb->DpcStack = (PVOID)(DpcStackBuffer + (12 * 1024));
    }

    // ==========================================
    // 7. SIMPAN POINTER KE BLOCK_BOOT_2
    // ==========================================
    // (Ganti nama field ini sesuai nama variabel KPCR di struct BLOCK_BOOT_2 kamu)
    BlockBoot2->Prcb = (ULONG_PTR)Prcb;

    LdrSetFsSegmentBase((ULONG)Kpcr, sizeof(KIPCR));
}

static VOID
LdrpSetGdtEntry(
    PKGDTENTRY32 Entry,
    ULONG Base,
    ULONG Limit,
    UCHAR Type,
    UCHAR Dpl,
    BOOLEAN IsSystem,
    BOOLEAN Granularity,
    BOOLEAN DefaultBig
)
{
    cmemset(Entry, 0, sizeof(KGDTENTRY32));

    Entry->LimitLow = (USHORT)(Limit & 0xFFFF);
    Entry->BaseLow  = (USHORT)(Base & 0xFFFF);

    Entry->HighWord.Bits.BaseMid     = (Base >> 16) & 0xFF;
    Entry->HighWord.Bits.Type        = Type;
    Entry->HighWord.Bits.System      = IsSystem ? 1 : 0;
    Entry->HighWord.Bits.Dpl         = Dpl;
    Entry->HighWord.Bits.Present     = 1;
    Entry->HighWord.Bits.LimitHi     = (Limit >> 16) & 0x0F;
    Entry->HighWord.Bits.Avl         = 0;
    Entry->HighWord.Bits.LongMode    = 0;
    Entry->HighWord.Bits.DefaultBig  = DefaultBig ? 1 : 0;
    Entry->HighWord.Bits.Granularity = Granularity ? 1 : 0;
    Entry->HighWord.Bits.BaseHi      = (Base >> 24) & 0xFF;
}

VOID
VEAPI
LdrInitializeGdtTss(IN OUT BLOCK_BOOT_2 *BlockBoot2)
{
    /*
    
        BOOTLOADER GDT Segment:
        1. GDT_NULL
        2. GDT_CODE
        3. GDT_DATA
        4. GDT_CODE16
        5. GDT_DATA16
        6. GDT_RESERVED
        7. GDT_PCR

        KERNEL GDT Segment:
        1. GDT_NULL
        2. GDT_CODE
        3. GDT_DATA
        4. GDT_CODE_R3
        5. GDT_DATA_R3
        6. GDT_TSS
        7. GDT_PCR
    */

    PKGDTENTRY32 GdtHigherHalf;
    PKTSSENTRY32 TssObject;
    PKTSSENTRY32 DfTssObject;
    KDESCRIPTOR Gdtr;
    PKIPCR Kipcr;
    PKPCR Kpcr;
    ULONG CR3 = __ReadCR3();

    GdtHigherHalf = (PKGDTENTRY32)LmAllocatePool(
        LdrUnreclaimablePool, 
        sizeof(KGDTENTRY32) * GDT_ENTRY_COUNT
    );
    if (!GdtHigherHalf)
    {
        LdrError(STATUS_INSUFFICIENT_RESOURCES);
        return;
    }
    cmemset(GdtHigherHalf, 0, sizeof(KGDTENTRY32) * GDT_ENTRY_COUNT);

    TssObject = (PKTSSENTRY32)LmAllocatePool(LdrUnreclaimablePool, sizeof(KTSSENTRY32));
    if (!TssObject)
    {
        LdrError(STATUS_INSUFFICIENT_RESOURCES);
        return;
    }
    cmemset(TssObject, 0, sizeof(KTSSENTRY32));

    TssObject->Ss0       = KGDT32_R0_DATA;
    TssObject->Esp0      = 0;
    TssObject->IoMapBase = sizeof(KTSSENTRY32);

    DfTssObject = (PKTSSENTRY32)LmAllocatePool(LdrUnreclaimablePool, sizeof(KTSSENTRY32));
    if (!DfTssObject) return;
    cmemset(DfTssObject, 0, sizeof(KTSSENTRY32));

    DfTssObject->Cs        = KGDT32_R0_CODE;
    DfTssObject->Ds        = KGDT32_R0_DATA;
    DfTssObject->Es        = KGDT32_R0_DATA;
    DfTssObject->Fs        = KGDT32_R0_PCR;
    DfTssObject->Gs        = KGDT32_R0_DATA;
    DfTssObject->Ss        = KGDT32_R0_DATA;
    DfTssObject->Ss0       = KGDT32_R0_DATA;
    DfTssObject->Eflags    = 0x00000002;
    DfTssObject->Cr3       = CR3;
    DfTssObject->IoMapBase = sizeof(KTSSENTRY32);
    
    Kipcr = (PKIPCR)CONTAINING_RECORD(BlockBoot2->Prcb, KIPCR, PrcbData);
    Kpcr  = &Kipcr->Pcr;

    // Index 0: KGDT32_R0_NULL (0x00)
    LdrpSetGdtEntry(&GdtHigherHalf[0], 0, 0, 0, 0, FALSE, FALSE, FALSE);

    // Index 1: KGDT32_R0_CODE (0x08) -> Kernel Code (Flat 4GB)
    LdrpSetGdtEntry(&GdtHigherHalf[1], 0x00000000, 0xFFFFF, 0xB, 0, TRUE, TRUE, TRUE);

    // Index 2: KGDT32_R0_DATA (0x10) -> Kernel Data (Flat 4GB)
    LdrpSetGdtEntry(&GdtHigherHalf[2], 0x00000000, 0xFFFFF, 0x3, 0, TRUE, TRUE, TRUE);

    // Index 3: KGDT32_R3_CODE (0x18 / RPL 3 -> 0x1B) -> User Code (Flat 4GB)
    LdrpSetGdtEntry(&GdtHigherHalf[3], 0x00000000, 0xFFFFF, 0xB, 3, TRUE, TRUE, TRUE);

    // Index 4: KGDT32_R3_DATA (0x20 / RPL 3 -> 0x23) -> User Data (Flat 4GB)
    LdrpSetGdtEntry(&GdtHigherHalf[4], 0x00000000, 0xFFFFF, 0x3, 3, TRUE, TRUE, TRUE);

    // Index 5: KGDT32_TSS (0x28) -> Task State Segment
    LdrpSetGdtEntry(
        &GdtHigherHalf[5], 
        (ULONG_PTR)TssObject, 
        sizeof(KTSSENTRY32) - 1, 
        0x9, // Available 32-bit TSS
        0, 
        FALSE, // System Descriptor
        FALSE, // Byte Granularity
        FALSE
    );

    // Index 6: KGDT32_R0_PCR (0x30) -> KPCR (FS Register Base)
    LdrpSetGdtEntry(
        &GdtHigherHalf[6], 
        (ULONG_PTR)Kpcr, 
        sizeof(KIPCR) - 1, 
        0x3, // Read/Write Data
        0, 
        TRUE, 
        FALSE, // Byte Granularity
        TRUE
    );
    
    // Index 6: KGDT32_DF_TSS (0x38) 
    LdrpSetGdtEntry(
        &GdtHigherHalf[7], 
        (ULONG_PTR)DfTssObject, 
        sizeof(KTSSENTRY32) - 1, 
        0x9,   // Type 0x9 = Available 32-bit TSS
        0,     // DPL 0 (Kernel)
        FALSE, // System Descriptor (0)
        FALSE, // Byte Granularity
        FALSE
    );

    Gdtr.Limit = (sizeof(KGDTENTRY32) * GDT_ENTRY_COUNT) - 1;
    Gdtr.Base  = (ULONG_PTR)GdtHigherHalf;

    LdrpReloadSegments(&Gdtr, KGDT32_TSS);
    
    Kpcr->GDT = (PVOID)GdtHigherHalf;
    Kpcr->TSS = (PVOID)TssObject;
}