/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : utility.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Utility to run AHCI Driver
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include "ahciheader.h"

/* Revision History ------------------------------------------------------
 * DATE       : 26-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file utility.c
 * --------------------------------------------------------------------- */

PHYSICAL_ADDRESS
VEAPI
AhciGetAbarPhysicalAddress(
    IN POBIO_RESOURCE_LIST ResourceList,
    OUT PULONG OutAbarLength
)
{
    PHYSICAL_ADDRESS AbarPhysical = { 0 };
    ULONG MemoryResourceIndex = 0;

    if (!ResourceList || ResourceList->Count == 0)
    {
        return AbarPhysical;
    }

    for (ULONG i = 0; i < ResourceList->Count; i++)
    {
        POBIO_RESOURCE_DESCRIPTOR Res = &ResourceList->Descriptors[i];

        // Filter khusus resource MMIO BAR
        if (Res->Type == OBIO_RES_TYPE_MEMORY)
        {
            /* 
             * Pada AHCI Controller (PCI Class 01h, Subclass 06h):
             * BAR0 - BAR4: Vendor Specific / Legacy IDE Emulation (jika ada)
             * BAR5       : ABAR MMIO (Resource Memory ke-5 atau ukuran >= 0x110)
             */
            if (MemoryResourceIndex == 5 || Res->Memory.Length >= 0x110)
            {
                AbarPhysical = Res->Memory.Base;

                if (OutAbarLength)
                {
                    *OutAbarLength = Res->Memory.Length;
                }

                DPRINT("AHCI: Found ABAR MMIO at Physical Address: 0x%lx (Length: 0x%x)\n",
                       (ULONG)AbarPhysical.QuadPart,
                       Res->Memory.Length);

                return AbarPhysical;
            }

            MemoryResourceIndex++;
        }
    }

    return AbarPhysical;
}

BOOLEAN
VEAPI
AhciGetInterruptInfo(
    IN POBIO_RESOURCE_LIST ResourceList,
    OUT PAHCI_INTERRUPT_INFO IntrInfo
)
{
    if (!ResourceList || ResourceList->Count == 0 || !IntrInfo)
    {
        return FALSE;
    }

    RtlZeroMemory(IntrInfo, sizeof(AHCI_INTERRUPT_INFO));

    for (ULONG i = 0; i < ResourceList->Count; i++)
    {
        POBIO_RESOURCE_DESCRIPTOR Res = &ResourceList->Descriptors[i];

        // 1. PRIORITAS UTAMA: Message Interrupt (MSI / MSI-X)
        if (Res->Type == PnpResourceTypeMessageInterrupt)
        {
            IntrInfo->Mode = AhciInterruptModeMessage;
            IntrInfo->Vector = Res->Interrupt.Vector;       // Granular vector count
            IntrInfo->CapOffset = (UCHAR)Res->Interrupt.Level; // Config space offset

            DPRINT("AHCI: Using Message Signaled Interrupt (CapOffset=0x%02X, Vectors=%u)\n", 
                   IntrInfo->CapOffset, IntrInfo->Vector);
            return TRUE;
        }
        // 2. FALLBACK: Legacy INTx (IRQ Line 0x3C)
        else if (Res->Type == PnpResourceTypeInterrupt)
        {
            IntrInfo->Mode = AhciInterruptModeLegacy;
            IntrInfo->Vector = Res->Interrupt.Vector;
            IntrInfo->CapOffset = 0;

            DPRINT("AHCI: Using Legacy Interrupt Vector 0x%x\n", IntrInfo->Vector);
            return TRUE;
        }
    }

    DPRINT("AHCI: WARN - No Interrupt Resource found in ResourceList!\n");
    return FALSE;
}

VEASTATUS
VEAPI
AhciGetBusInterface(
    IN PAHCI_DEVICE_EXTENSION DevExt,
    IN const PGUID Guid,
    OUT PVOID BusInterface,
    IN ULONG Version,
    IN ULONG Size
)
{
    POIP Oip;
    POIP_STACK Stack;
    KEVENT Event;
    VEASTATUS Status;
    OIP_INFORMATION StatusInformation;

    KsInitializeEvent(&Event, NotificationEvent, FALSE);

    Oip = ObioBuildSynchronusFsdRequest(
        OIP_MJ_PNP,
        DevExt->NextDeviceObject,
        NULL,
        0,
        0,
        &Event,
        &StatusInformation
    );

    if (!Oip)
    {
        return STATUS_IO_ALLOCATE_FAILED;
    }

    Oip->StatusInformation.Status = STATUS_NOT_SUPPORTED;
    Oip->StatusInformation.Information = 0;

    Stack = ObioGetNextOipStackLocation(Oip);
    Stack->MinorFunction = OIP_MN_QUERY_INTERFACE;
    Stack->Parameters.QueryInterface.InterfaceType = Guid;
    Stack->Parameters.QueryInterface.Version = Version;
    Stack->Parameters.QueryInterface.Size = Size;
    Stack->Parameters.QueryInterface.InterfaceSpecificData = BusInterface;

    Status = ObioCallObject(DevExt->NextDeviceObject, Oip);
    if (Status == STATUS_PENDING)
    {
        KsWaitForSingleObject(&Event);
        Status = StatusInformation.Status;
    }

    return Status;
}

AHCI_DEVICE_TYPE
VEAPI
AhciCheckForSignature(IN PAHCI_PORT_CONTEXT PortCtx)
{
    ULONG Signature;

    Signature = PortCtx->Registers->Sig;

    switch (Signature)
    {
        case AHCI_DEV_SIG_SATA:
            PortCtx->DeviceType = AhciDeviceTypeSata;
            DPRINT("AHCI: Port %u - Detected SATA Storage Device\n", PortCtx->PortNumber);
            break;

        case AHCI_DEV_SIG_SATAPI:
            PortCtx->DeviceType = AhciDeviceTypeSatapi;
            DPRINT("AHCI: Port %u - Detected SATAPI Optical Drive\n", PortCtx->PortNumber);
            break;

        case AHCI_DEV_SIG_SEMB:
            PortCtx->DeviceType = AhciDeviceTypeEnclosure;
            DPRINT("AHCI: Port %u - Detected Enclosure Management Bridge\n", PortCtx->PortNumber);
            break;

        case AHCI_DEV_SIG_PM:
            PortCtx->DeviceType = AhciDeviceTypePortMultiplier;
            DPRINT("AHCI: Port %u - Detected Port Multiplier\n", PortCtx->PortNumber);
            break;

        default:
            PortCtx->DeviceType = AhciDeviceTypeUnknown;
            DPRINT("AHCI: Port %u - Unknown Signature: 0x%08X\n", PortCtx->PortNumber, Signature);
            break;
    }

    return PortCtx->DeviceType;
}

VEASTATUS
VEAPI
AhciCallLowerDeviceOip(
    IN PDEVICE_OBJECT TargetDeviceObject,
    IN UCHAR MajorFunction,
    IN UCHAR MinorFunction,
    IN POIP_STACK ParametersStack OPTIONAL,
    OUT POIP_INFORMATION Information
)
{
    POIP Oip;
    POBJECT_HEADER ObjectHeader;
    OIP_INFORMATION LocalStatusInformation;
    POIP_STACK LocalOipStack;
    KEVENT Event;
    VEASTATUS Status;

    if (!TargetDeviceObject || !Information)
    {
        return STATUS_INVALID_PARAMETER;
    }

    /* Reference langsung ke target object (PDO) tanpa cari TopDeviceObject */
    ObReferenceObject(TargetDeviceObject);

    ObjectHeader = OBJECT_TO_OBJECT_HEADER(TargetDeviceObject);

    /* Alokasi OIP sesuai stack depth milik target device */
    Oip = ObioAllocateOip(ObjectHeader->StackDepth ? ObjectHeader->StackDepth : 1);
    if (!Oip)
    {
        ObDereferenceObject(TargetDeviceObject);
        return STATUS_INSUFFICIENT_MEMORY;
    }

    /* Setup status default */
    Oip->StatusInformation.Status = STATUS_NOT_SUPPORTED;
    Oip->StatusInformation.Information = 0;
    LocalStatusInformation.Status = STATUS_NOT_SUPPORTED;
    LocalStatusInformation.Information = 0;

    /* Setup event sinkronisasi */
    KsInitializeEvent(&Event, SynchronizationEvent, FALSE);

    Oip->UserInfoBlock = &LocalStatusInformation;
    Oip->UserEvent = &Event;

    /* Isi header Major/Minor function ke OIP Stack */
    LocalOipStack = ObioGetNextOipStackLocation(Oip);
    RtlZeroMemory(LocalOipStack, sizeof(OIP_STACK));
    LocalOipStack->MajorFunction = MajorFunction;
    LocalOipStack->MinorFunction = MinorFunction;

    /* Salin parameter tambahan jika ada (misal QueryInterface / QueryResources) */
    if (ParametersStack)
    {
        RtlCopyMemory(
            &LocalOipStack->Parameters,
            &ParametersStack->Parameters,
            sizeof(ParametersStack->Parameters)
        );
    }

    /* Panggil LANGSUNG ke TargetDeviceObject (bebas loop recursion) */
    Status = ObioGoInterruptObject((PVOID)TargetDeviceObject, Oip);

    if (Status == STATUS_PENDING)
    {
        KsWaitForSingleObject(&Event);
        Status = LocalStatusInformation.Status;
    }
    else
    {
        LocalStatusInformation = Oip->StatusInformation;
    }

    ObDereferenceObject(TargetDeviceObject);

    Information->Status = Status;
    Information->Information = LocalStatusInformation.Information;

    return Status;
}

ULONG
VEAPI
AhciAllocateCommandSlot(
    IN PAHCI_PORT_CONTEXT PortCtx
)
{
    // Cari bit 0 pertama yang kosong di SlotAllocBitmap
    for (ULONG Slot = 0; Slot < 32; Slot++)
    {
        if (!(PortCtx->SlotAllocBitmap & (1 << Slot)))
        {
            PortCtx->SlotAllocBitmap |= (1 << Slot);
            return Slot; // Dapet slot bebas (0 - 31)
        }
    }
    return 0xFFFFFFFF; // Full/Busy
}

VOID
VEAPI
AhciFreeCommandSlot(
    IN PAHCI_PORT_CONTEXT PortCtx,
    IN ULONG Slot
)
{
    PortCtx->SlotAllocBitmap &= ~(1 << Slot);
}

VOID
VEAPI
AhciParseDeviceCapabilities(
    IN PAHCI_PORT_CONTEXT PortCtx,
    IN PATA_IDENTIFY_DATA IdentifyData
)
{
    PUSHORT RawWords = (PUSHORT)IdentifyData;
    USHORT QueueDepthWord = RawWords[75];
    USHORT SataCapWord     = RawWords[76];
    USHORT CmdSetWord83   = RawWords[83];

    RtlZeroMemory(&PortCtx->Capabilities, sizeof(AHCI_PORT_CAPABILITIES));

    if (CmdSetWord83 & (1 << 10))
    {
        PortCtx->Capabilities.SupportsLba48 = TRUE;
    }

    if ((SataCapWord & (1 << 8)) && (PortCtx->DevExt->HbaRegisters->Cap & AHCI_CAP_SNCQ))
    {
        PortCtx->Capabilities.SupportsNcq = TRUE;
        PortCtx->Capabilities.MaxQueueDepth = (QueueDepthWord & 0x1F) + 1; // 0-based index
    }
    else
    {
        PortCtx->Capabilities.SupportsNcq = FALSE;
        PortCtx->Capabilities.MaxQueueDepth = 1;
    }

    DPRINT("AHCI: Port %u Capabilities -> LBA48: %d, NCQ: %d (Max Queue Depth: %u)\n",
           PortCtx->PortNumber,
           PortCtx->Capabilities.SupportsLba48,
           PortCtx->Capabilities.SupportsNcq,
           PortCtx->Capabilities.MaxQueueDepth);
}

VEASTATUS
VEAPI
AhciResetPortEngine(
    IN PAHCI_PORT_CONTEXT PortCtx
)
{
    PAHCI_PORT_REGISTERS PortRegs = PortCtx->Registers;
    ULONG Timeout;

    PortRegs->Cmd = (PortRegs->Cmd & ~AHCI_PxCMD_ICC_MASK) | AHCI_PxCMD_ICC_ACTIVE;
    PortRegs->Cmd &= ~AHCI_PxCMD_ST;
    PortRegs->Cmd &= ~AHCI_PxCMD_FRE;

    Timeout = 1000;
    while (Timeout > 0)
    {
        if (!(PortRegs->Cmd & AHCI_PxCMD_CR) && !(PortRegs->Cmd & AHCI_PxCMD_FR))
        {
            break;
        }
        KsStallExecutionProcessor(10);
        Timeout--;
    }

    PortRegs->Sctl = (PortRegs->Sctl & ~AHCI_PxSCTL_DET_MASK) | AHCI_PxSCTL_DET_INIT;
    KsStallExecutionProcessor(1000); // Tahan minimal 1 ms untuk pulsa COMRESET
    PortRegs->Sctl = (PortRegs->Sctl & ~AHCI_PxSCTL_DET_MASK) | AHCI_PxSCTL_DET_NOINIT;

    Timeout = 10000;
    while (Timeout > 0)
    {
        if ((PortRegs->Ssts & AHCI_PxSSTS_DET_MASK) == AHCI_PxSSTS_DET_PRESENT_PHY)
        {
            break;
        }
        KsStallExecutionProcessor(10);
        Timeout--;
    }

    PortRegs->Serr = 0xFFFFFFFF; // Clear PxSERR
    PortRegs->Is   = PortRegs->Is; // Clear PxIS
    PortRegs->Cmd |= AHCI_PxCMD_FRE;
    PortRegs->Cmd |= AHCI_PxCMD_ST;

    if ((PortRegs->Ssts & AHCI_PxSSTS_DET_MASK) == AHCI_PxSSTS_DET_PRESENT_PHY)
    {
        DPRINT("AHCI: Port %u Link successfully recovered via COMRESET!\n", PortCtx->PortNumber);
        return STATUS_SUCCESS;
    }

    DPRINT("AHCI: Port %u Link IS DEAD after COMRESET!\n", PortCtx->PortNumber);
    return STATUS_LINK_FAILED;
}

VOID
VEAPI
AhciPrepareH2dCommand(
    IN PAHCI_PORT_CONTEXT PortCtx,
    IN ULONG Slot,
    IN BOOLEAN IsWrite,
    IN USHORT PrdtCount,
    OUT PAHCI_COMMAND_HEADER *OutHeader,
    OUT PAHCI_COMMAND_TABLE *OutTable,
    OUT PFIS_REG_H2D *OutFis
)
{
    PAHCI_COMMAND_HEADER CmdHeader;
    PAHCI_COMMAND_TABLE CmdTable;
    PHYSICAL_ADDRESS CmdTablePhys;

    CmdHeader = &PortCtx->CommandListVirtual[Slot];
    RtlZeroMemory(CmdHeader, sizeof(AHCI_COMMAND_HEADER));

    CmdTable = (PAHCI_COMMAND_TABLE)(
        (ULONG_PTR)PortCtx->ReceivedFisVirtual + 256 + (Slot * AHCI_CMD_TABLE_SIZE)
    );
    CmdTablePhys.QuadPart = PortCtx->ReceivedFisPhysical.QuadPart + 256 + (Slot * AHCI_CMD_TABLE_SIZE);
    RtlZeroMemory(CmdTable, sizeof(AHCI_COMMAND_TABLE));

    CmdHeader->Cfl   = sizeof(FIS_REG_H2D) / sizeof(ULONG); // 5 DWORDs
    CmdHeader->W     = IsWrite ? 1 : 0;
    CmdHeader->Prdtl = PrdtCount;
    CmdHeader->Ctba  = CmdTablePhys.LowPart;
    CmdHeader->Ctbau = CmdTablePhys.HighPart;

    PFIS_REG_H2D Fis = (PFIS_REG_H2D)&CmdTable->Cfb;
    Fis->FisType  = FIS_TYPE_REG_H2D;
    Fis->PmPort_C = 0x80; // Command bit = 1

    if (OutHeader) *OutHeader = CmdHeader;
    if (OutTable)  *OutTable  = CmdTable;
    if (OutFis)    *OutFis    = Fis;
}

VOID
VEAPI
AhciSetupSinglePrdt(
    IN PAHCI_COMMAND_TABLE CmdTable,
    IN ULONG EntryIndex,
    IN PHYSICAL_ADDRESS PhysAddr,
    IN ULONG ByteCount,
    IN BOOLEAN InterruptOnCompletion
)
{
    PPHYSICAL_REGION_DESCRIPTOR_TABLE Prdt = &CmdTable->Prdt[EntryIndex];
    Prdt->Dba  = PhysAddr.LowPart;
    Prdt->Dbau = PhysAddr.HighPart;
    Prdt->Dbc  = ByteCount - 1; // 0-based byte count
    Prdt->I    = InterruptOnCompletion ? 1 : 0;
}

VEASTATUS
VEAPI
AhciExecuteSyncCommand(
    IN PAHCI_PORT_CONTEXT PortCtx,
    IN ULONG Slot
)
{
    PAHCI_PORT_REGISTERS PortRegs = PortCtx->Registers;
    KEVENT Event;
    VEASTATUS Status;

    KsInitializeEvent(&Event, SynchronizationEvent, FALSE);
    PortCtx->CompletionEvent = &Event;

    /* Clear sisa interrupt/SATA error sebelum issue */
    PortRegs->Is   = PortRegs->Is;
    PortRegs->Serr = PortRegs->Serr;

    /* Fire Command Issue */
    PortRegs->Ci |= (1 << Slot);

    Status = KsWaitForSingleObject(&Event);

    PortCtx->CompletionEvent = NULL;
    return Status;
}

VEASTATUS
VEAPI
AhciDispatchSubChunk(
    IN PAHCI_PORT_CONTEXT PortCtx,
    IN ULONG Slot
)
{
    PAHCI_PORT_REGISTERS PortRegs = PortCtx->Registers;
    PAHCI_IO_CHUNK_CONTEXT Chunk = &PortCtx->ChunkContext[Slot];
    PAHCI_COMMAND_HEADER CmdHeader;
    PAHCI_COMMAND_TABLE CmdTable;
    PFIS_REG_H2D Fis;
    POIP Oip = Chunk->MasterOip;
    PMDL Mdl = Oip->MdlAddress;
    ULONG MaxChunkSize = AHCI_COMMAND_TABLE_PRDT_COUNT * 4096;
    ULONG CurrentChunkSize = (Chunk->RemainingBytes > MaxChunkSize) ? MaxChunkSize : Chunk->RemainingBytes;
    ULONG SectorCount = CurrentChunkSize / PortCtx->SectorSize;
    PVOID UserCurrentBuffer = NULL;

    if (Mdl)
    {
        UserCurrentBuffer = (PVOID)((ULONG_PTR)Mdl->MappedSystemVa + Chunk->BufferByteOffset);
    }
    else if (Oip->UserBuffer)
    {
        UserCurrentBuffer = (PVOID)((ULONG_PTR)Oip->UserBuffer + Chunk->BufferByteOffset);
    }

    BOOLEAN NeedsBounce = !AhciIsBufferAligned(UserCurrentBuffer, CurrentChunkSize);

    Chunk->UsesBounceBuffer = FALSE;

    AhciPrepareH2dCommand(PortCtx, Slot, Chunk->IsWrite, 0, &CmdHeader, &CmdTable, &Fis);

    ULONG PrdtEntries = 0;

    if (NeedsBounce && PortCtx->BounceBufferVirtual)
    {
        Chunk->UsesBounceBuffer = TRUE;

        if (Chunk->IsWrite)
        {
            // Jika WRITE: Copy data dari buffer unaligned user KE Bounce Buffer dulu
            RtlCopyMemory(PortCtx->BounceBufferVirtual, UserCurrentBuffer, CurrentChunkSize);
        }

        // PRDT cuma butuh 1 entry menunjuk ke Bounce Buffer Kontigu
        AhciSetupSinglePrdt(CmdTable, 0, PortCtx->BounceBufferPhysical, CurrentChunkSize, TRUE);
        PrdtEntries = 1;
    }
    else
    {
        if (Mdl)
        {
            PPFN_NUMBER PageArray = (PPFN_NUMBER)(Mdl + 1);
            ULONG TotalOffset = Mdl->ByteOffset + Chunk->BufferByteOffset;
            ULONG PageIndex = TotalOffset / 4096;
            ULONG OffsetInPage = TotalOffset % 4096;
            ULONG BytesRemainingInChunk = CurrentChunkSize;

            while (BytesRemainingInChunk > 0 && PrdtEntries < AHCI_MAX_PRDT_ENTRIES)
            {
                PHYSICAL_ADDRESS PagePhys;
                PagePhys.QuadPart = ((ULONGLONG)PageArray[PageIndex] << 12) + OffsetInPage;
                
                ULONG ChunkSize = 4096 - OffsetInPage;
                if (ChunkSize > BytesRemainingInChunk) ChunkSize = BytesRemainingInChunk;

                AhciSetupSinglePrdt(CmdTable, PrdtEntries, PagePhys, ChunkSize, FALSE);

                BytesRemainingInChunk -= ChunkSize;
                OffsetInPage = 0;
                PageIndex++;
                PrdtEntries++;
            }
            
            if (PrdtEntries > 0)
            {
                CmdTable->Prdt[PrdtEntries - 1].I = 1;
            }
        }
        else if (Oip->UserBuffer)
        {
            PHYSICAL_ADDRESS PhysAddr = MmGetPhysicalAddress(UserCurrentBuffer);
            AhciSetupSinglePrdt(CmdTable, 0, PhysAddr, CurrentChunkSize, TRUE);
            PrdtEntries = 1;
        }
    }

    CmdHeader->Prdtl = PrdtEntries;

    /* Setup Standard LBA48 DMA FIS */
    Fis->Command         = Chunk->IsWrite ? ATA_CMD_WRITE_DMA_EX : ATA_CMD_READ_DMA_EX;
    Fis->LbaLow          = (UCHAR)(Chunk->CurrentStartLba & 0xFF);
    Fis->LbaMid          = (UCHAR)((Chunk->CurrentStartLba >> 8) & 0xFF);
    Fis->LbaHigh         = (UCHAR)((Chunk->CurrentStartLba >> 16) & 0xFF);
    Fis->Device          = 0x40;
    Fis->LbaLowExp       = (UCHAR)((Chunk->CurrentStartLba >> 24) & 0xFF);
    Fis->LbaMidExp       = (UCHAR)((Chunk->CurrentStartLba >> 32) & 0xFF);
    Fis->LbaHighExp      = (UCHAR)((Chunk->CurrentStartLba >> 40) & 0xFF);
    Fis->SectorCountLow  = (UCHAR)(SectorCount & 0xFF);
    Fis->SectorCountHigh = (UCHAR)((SectorCount >> 8) & 0xFF);

    PortCtx->PendingOip[Slot] = Oip;

    PortRegs->Is   = PortRegs->Is;
    PortRegs->Serr = PortRegs->Serr;
    PortRegs->Ci  |= (1 << Slot);

    return STATUS_PENDING;
}

BOOLEAN
VEAPI
AhciIsBufferAligned(
    IN PVOID VirtualAddress,
    IN ULONG Length
)
{
    // Minimal 4-byte (DWORD) alignment untuk address dan length
    if (((ULONG_PTR)VirtualAddress & 0x03) != 0 || (Length & 0x03) != 0)
    {
        return FALSE; 
    }
    return TRUE;
}

/* EOF */