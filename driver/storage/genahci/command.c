/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : command.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : AHCI Command goes here
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include "ahciheader.h"

/* Revision History ------------------------------------------------------
 * DATE       : 27-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file command.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
AhciSendIdentifyDevice(
    IN PAHCI_DEVICE_EXTENSION DevExt,
    IN PAHCI_PORT_CONTEXT PortCtx,
    OUT PATA_IDENTIFY_DATA OutIdentifyData
)
{
    PAHCI_COMMAND_HEADER CmdHeader;
    PAHCI_COMMAND_TABLE CmdTable;
    PFIS_REG_H2D Fis;
    PVOID DmaVirtual;
    PHYSICAL_ADDRESS DmaPhys, MaxAddr;
    ULONG Slot;
    VEASTATUS Status;

    if (!DevExt || !PortCtx || !OutIdentifyData) return STATUS_INVALID_PARAMETER;

    Slot = AhciAllocateCommandSlot(PortCtx);
    if (Slot == 0xFFFFFFFF) return STATUS_DEVICE_BUSY;

    MaxAddr.QuadPart = DevExt->Supports64BitDma ? 0xFFFFFFFFFFFFFFFFULL : 0xFFFFFFFFULL;
    DmaVirtual = MmAllocateContiguousMemory(512, MaxAddr);
    if (!DmaVirtual)
    {
        AhciFreeCommandSlot(PortCtx, Slot);
        return STATUS_INSUFFICIENT_MEMORY;
    }

    RtlZeroMemory(DmaVirtual, 512);
    DmaPhys = MmGetPhysicalAddress(DmaVirtual);

    AhciPrepareH2dCommand(PortCtx, Slot, FALSE, 1, &CmdHeader, &CmdTable, &Fis);
    AhciSetupSinglePrdt(CmdTable, 0, DmaPhys, 512, TRUE);
    Fis->Command = ATA_CMD_IDENTIFY;

    Status = AhciExecuteSyncCommand(PortCtx, Slot);

    if (VEA_SUCCESS(Status))
    {
        RtlCopyMemory(OutIdentifyData, DmaVirtual, sizeof(ATA_IDENTIFY_DATA));
    }

    MmFreeContiguousMemory(DmaVirtual);
    AhciFreeCommandSlot(PortCtx, Slot);
    return Status;
}

VEASTATUS
VEAPI
AhciExecutePassThroughCommand(
    IN PAHCI_DEVICE_EXTENSION DevExt,
    IN PAHCI_PORT_CONTEXT PortCtx,
    IN OUT PATA_PASS_THROUGH_EX AtaPt,
    IN OUT PVOID DataBuffer OPTIONAL
)
{
    PAHCI_COMMAND_HEADER CmdHeader;
    PAHCI_COMMAND_TABLE CmdTable;
    PFIS_REG_H2D Fis;
    PVOID DmaBufferVirtual = NULL;
    PHYSICAL_ADDRESS DmaBufferPhysical, MaxPhysicalAddress;
    BOOLEAN IsWrite = (AtaPt->AtaFlags & ATA_FLAGS_DATA_OUT) != 0;
    VEASTATUS Status;

    MaxPhysicalAddress.QuadPart = DevExt->Supports64BitDma ? 0xFFFFFFFFFFFFFFFFULL : 0xFFFFFFFFULL;

    if (AtaPt->DataTransferLength > 0 && DataBuffer)
    {
        DmaBufferVirtual = MmAllocateContiguousMemory(AtaPt->DataTransferLength, MaxPhysicalAddress);
        if (!DmaBufferVirtual)
        {
            return STATUS_INSUFFICIENT_MEMORY;
        }

        if (IsWrite)
            RtlCopyMemory(DmaBufferVirtual, DataBuffer, AtaPt->DataTransferLength);
        else
            RtlZeroMemory(DmaBufferVirtual, AtaPt->DataTransferLength);

        DmaBufferPhysical = MmGetPhysicalAddress(DmaBufferVirtual);
    }

    /* 
     * Inisialisasi Slot 0, Command Header, Table, dan Base FIS via Helper
     */
    AhciPrepareH2dCommand(PortCtx, 0, IsWrite, DmaBufferVirtual ? 1 : 0, &CmdHeader, &CmdTable, &Fis);

    if (DmaBufferVirtual)
    {
        AhciSetupSinglePrdt(CmdTable, 0, DmaBufferPhysical, AtaPt->DataTransferLength, TRUE);
    }

    /* Copy Task File Registers dari Request User */
    Fis->FeaturesLow    = AtaPt->CurrentTaskFile[0];
    Fis->SectorCountLow = AtaPt->CurrentTaskFile[1];
    Fis->LbaLow         = AtaPt->CurrentTaskFile[2];
    Fis->LbaMid         = AtaPt->CurrentTaskFile[3];
    Fis->LbaHigh        = AtaPt->CurrentTaskFile[4];
    Fis->Device         = AtaPt->CurrentTaskFile[5];
    Fis->Command        = AtaPt->CurrentTaskFile[6];
    Fis->Control        = AtaPt->CurrentTaskFile[7];

    if (AtaPt->AtaFlags & ATA_FLAGS_48BIT_COMMAND)
    {
        Fis->FeaturesHigh    = AtaPt->PreviousTaskFile[0];
        Fis->SectorCountHigh = AtaPt->PreviousTaskFile[1];
        Fis->LbaLowExp       = AtaPt->PreviousTaskFile[2];
        Fis->LbaMidExp       = AtaPt->PreviousTaskFile[3];
        Fis->LbaHighExp      = AtaPt->PreviousTaskFile[4];
    }

    Status = AhciExecuteSyncCommand(PortCtx, 0);

    if (VEA_SUCCESS(Status) && DmaBufferVirtual && !IsWrite)
    {
        RtlCopyMemory(DataBuffer, DmaBufferVirtual, AtaPt->DataTransferLength);
    }

    if (DmaBufferVirtual)
    {
        MmFreeContiguousMemory(DmaBufferVirtual);
    }

    return Status;
}

VEASTATUS
VEAPI
AhciSendFlushCache(
    IN PAHCI_DEVICE_EXTENSION DevExt,
    IN PAHCI_PORT_CONTEXT PortCtx
)
{
    PAHCI_COMMAND_HEADER CmdHeader;
    PAHCI_COMMAND_TABLE CmdTable;
    PFIS_REG_H2D Fis;

    // Slot 0 dipasang untuk internal flush
    AhciPrepareH2dCommand(PortCtx, 0, FALSE, 0, &CmdHeader, &CmdTable, &Fis);

    Fis->Command = (PortCtx->TotalSectors > 0) ? ATA_CMD_FLUSH_CACHE_EX : ATA_CMD_FLUSH_CACHE;

    VEASTATUS Status = AhciExecuteSyncCommand(PortCtx, 0);

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("AHCI: Port %u - FLUSH CACHE Failed (Status: 0x%x)\n", PortCtx->PortNumber, Status);
    }

    return Status;
}

VEASTATUS
VEAPI
AhciPdoReadWrite(
    IN PAHCI_PORT_CONTEXT PortCtx,
    IN POIP Oip,
    IN POIP_STACK OipStack
)
{
    ULONGLONG ByteOffset = (OipStack->MajorFunction == OIP_MJ_READ) ? 
                           OipStack->Parameters.ReadWrite.Offset.QuadPart : 
                           OipStack->Parameters.ReadWrite.Offset.QuadPart;
    ULONG ByteLength = (OipStack->MajorFunction == OIP_MJ_READ) ? 
                       OipStack->Parameters.ReadWrite.Length : 
                       OipStack->Parameters.ReadWrite.Length;
    BOOLEAN IsWrite = (OipStack->MajorFunction == OIP_MJ_WRITE);
    ULONG Slot;
    PAHCI_IO_CHUNK_CONTEXT ChunkContext;

    if (!PortCtx->SectorSize) PortCtx->SectorSize = 512;

    if (ByteLength == 0 || (ByteLength % PortCtx->SectorSize) != 0)
    {
        Oip->StatusInformation.Status = STATUS_INVALID_PARAMETER;
        Oip->StatusInformation.Information = 0;
        return STATUS_INVALID_PARAMETER;
    }

    Slot = AhciAllocateCommandSlot(PortCtx);
    if (Slot == 0xFFFFFFFF)
    {
        Oip->StatusInformation.Status = STATUS_DEVICE_BUSY;
        Oip->StatusInformation.Information = 0;
        return STATUS_DEVICE_BUSY;
    }

    ChunkContext = &PortCtx->ChunkContext[Slot];
    ChunkContext->MasterOip = Oip;
    ChunkContext->CurrentStartLba = ByteOffset / PortCtx->SectorSize;
    ChunkContext->RemainingBytes = ByteLength;
    ChunkContext->TransferredBytes = 0;
    ChunkContext->BufferByteOffset = 0;
    ChunkContext->IsWrite = IsWrite;

    return AhciDispatchSubChunk(PortCtx, Slot);
}

VEASTATUS
VEAPI
AhciSendTrimCommand(
    IN PAHCI_PORT_CONTEXT PortCtx,
    IN PDEVICE_DATA_SET_RANGE Ranges,
    IN ULONG RangeCount
)
{
    PAHCI_COMMAND_HEADER CmdHeader;
    PAHCI_COMMAND_TABLE CmdTable;
    PFIS_REG_H2D Fis;
    PATA_TRIM_RANGE_ENTRY TrimPayload;
    PHYSICAL_ADDRESS TrimPhys, MaxAddr;
    ULONG Slot, RangesProcessed = 0;
    VEASTATUS Status = STATUS_SUCCESS;

    if (!PortCtx || !Ranges || RangeCount == 0) return STATUS_INVALID_PARAMETER;

    while (RangesProcessed < RangeCount)
    {
        ULONG BatchCount = (RangeCount - RangesProcessed > 64) ? 64 : (RangeCount - RangesProcessed);

        Slot = AhciAllocateCommandSlot(PortCtx);
        if (Slot == 0xFFFFFFFF) return STATUS_DEVICE_BUSY;

        MaxAddr.QuadPart = PortCtx->DevExt->Supports64BitDma ? 0xFFFFFFFFFFFFFFFFULL : 0xFFFFFFFFULL;
        TrimPayload = (PATA_TRIM_RANGE_ENTRY)MmAllocateContiguousMemory(512, MaxAddr);
        if (!TrimPayload)
        {
            AhciFreeCommandSlot(PortCtx, Slot);
            return STATUS_INSUFFICIENT_MEMORY;
        }

        RtlZeroMemory(TrimPayload, 512);
        for (ULONG i = 0; i < BatchCount; i++)
        {
            PDEVICE_DATA_SET_RANGE DsmRange = &Ranges[RangesProcessed + i];
            TrimPayload[i].StartLba = DsmRange->StartingOffset / PortCtx->SectorSize;
            TrimPayload[i].SectorCount = DsmRange->LengthInBytes / PortCtx->SectorSize;
        }

        TrimPhys = MmGetPhysicalAddress(TrimPayload);

        /* Clean Setup via Helper */
        AhciPrepareH2dCommand(PortCtx, Slot, TRUE, 1, &CmdHeader, &CmdTable, &Fis);
        AhciSetupSinglePrdt(CmdTable, 0, TrimPhys, 512, TRUE);

        Fis->Command        = ATA_CMD_DATA_SET_MANAGEMENT;
        Fis->FeaturesLow    = 0x01; // TRIM Attribute
        Fis->SectorCountLow = 1;
        Fis->Device         = 0x40;

        Status = AhciExecuteSyncCommand(PortCtx, Slot);

        MmFreeContiguousMemory(TrimPayload);
        AhciFreeCommandSlot(PortCtx, Slot);

        if (!VEA_SUCCESS(Status)) break;
        RangesProcessed += BatchCount;
    }

    return Status;
}

/* EOF */