/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : vkhv.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaKeyTeam
   PURPOSE     : Hive 
 ----- Effective since 07-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 31-07-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file vkhv.c
 * --------------------------------------------------------------------- */

// Routine for allocation BIN1 cell
VEASTATUS
VEAPI
HvAllocateVolatileCell(
    IN PVK_HIVE Hive
)
{
    // Raise IRQL
    KIRQL OldIRQL = KeRaiseIrql(DISPATCH_LEVEL);

    if(!Hive || Hive->VolatileBinCount >= MAX_VOLATILE_BINS)
    {
        KeLowerIrql(OldIRQL);
        return STATUS_INSUFFICIENT_MEMORY;
    }

    // Allocate New Bin
    PVOID NewBin = (PVOID)UlAllocatePoolWithTag(NonPagedPool, VK_BIN_SIZE, 'VkHv');
    if(!NewBin) {
        KeLowerIrql(OldIRQL);
        return STATUS_INSUFFICIENT_MEMORY;
    }
    
    // Zero it's memory, fill the information with BIN1
    RtlZeroMemory(NewBin, VK_BIN_SIZE);

    PKEY_BASE_BLOCK BaseBlock = (PKEY_BASE_BLOCK)NewBin;

    BaseBlock->Signature        = KEY_BIN_SIG;
    BaseBlock->RelativeOffset   = Hive->VolatileBinCount * VK_BIN_SIZE;
    BaseBlock->Size             = VK_BIN_SIZE;
    BaseBlock->LastWriteTime    = KsQuerySystemTime();
    BaseBlock->Spare            = 0;

    // Attach metadata
    Hive->VolatileBins[Hive->VolatileBinCount] = NewBin;
    Hive->VolatileBinCount++;
    Hive->CurrentBinFreeOffset = sizeof(KEY_BASE_BLOCK);

    // Lower our IRQL
    KeLowerIrql(OldIRQL);

    return STATUS_SUCCESS;
}

// Routine to create Stable Cell and move them
// into a new buffer
VEASTATUS
VEAPI
HvAllocateStableCell(IN PVK_HIVE Hive)
{
    // Raise IRQL
    KIRQL OldIRQL = KeRaiseIrql(DISPATCH_LEVEL);

    if (!Hive || Hive->StableBinCount >= MAX_STABLE_BINS)
    {
        KeLowerIrql(OldIRQL);
        return STATUS_INSUFFICIENT_MEMORY;
    }

    // Should be OK, right?
    PVOID NewBinBase = UlAllocatePoolWithTag(NonPagedPool, VK_BIN_SIZE, 'VkHv');
    if (!NewBinBase)
    {
        KeLowerIrql(OldIRQL);
        KdPrintf("[Vk] ERROR | Failed to allocate new Stable Bin!\n");
        return STATUS_INSUFFICIENT_MEMORY;
    }

    RtlZeroMemory(NewBinBase, VK_BIN_SIZE);

    // New Bind Header
    PKEY_BASE_BLOCK NewBinHeader = (PKEY_BASE_BLOCK)NewBinBase;
    NewBinHeader->Signature      = KEY_BIN_SIG;
    NewBinHeader->RelativeOffset = Hive->Length;   // offset kumulatif sebelum bin ini
    NewBinHeader->Size           = VK_BIN_SIZE;

    // Hive Stable Bins
    ULONG BinIndex = Hive->StableBinCount;
    Hive->StableBins[BinIndex].BinBase       = NewBinBase;
    Hive->StableBins[BinIndex].BinSize       = VK_BIN_SIZE;
    Hive->StableBins[BinIndex].BinBaseOffset = Hive->Length;
    Hive->StableBinCount++;
    Hive->FreeOffset = Hive->Length + sizeof(KEY_BASE_BLOCK);
    Hive->Length    += VK_BIN_SIZE;

    VkpMarkCellDirty(Hive, Hive->StableBins[BinIndex].BinBaseOffset);

    // Lower our IRQL
    KeLowerIrql(OldIRQL);
    return STATUS_SUCCESS;
}