/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : engine.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : NVMe Engine
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include "nvmeheader.h"

/* Revision History ------------------------------------------------------
 * DATE       : 08-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file engine.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
NVMeCallLowerPnp(
    IN PDEVICE_OBJECT LowerDevice,
    IN UCHAR MinorFunction,
    OUT POIP_INFORMATION Information
)
{
    POIP Oip;
    POIP_STACK OipStack;
    VEASTATUS Status;

    if (!LowerDevice || !Information)
    {
        return STATUS_INVALID_PARAMETER;
    }

    Information->Status = STATUS_NOT_SUPPORTED;
    Information->Information = 0;

    Oip = ObioAllocateOip(1);
    if (!Oip)
    {
        Information->Status = STATUS_INSUFFICIENT_MEMORY;
        return Information->Status;
    }

    Oip->StatusInformation.Status = STATUS_NOT_SUPPORTED;
    Oip->StatusInformation.Information = 0;

    OipStack = ObioGetNextOipStackLocation(Oip);
    OipStack->MajorFunction = OIP_MJ_PNP;
    OipStack->MinorFunction = MinorFunction;

    Status = ObioGoInterruptObject(LowerDevice, Oip);
    *Information = Oip->StatusInformation;
    if (!VEA_SUCCESS(Status))
    {
        Information->Status = Status;
    }
    else
    {
        Status = Information->Status;
    }

    ObioFreeOip(Oip);
    return Status;
}

VEASTATUS
VEAPI
NVMeSubmitAdminCommand(
    IN PNVME_DEVICE_EXTENSION DevExt,
    IN PNVME_COMMAND Command,
    OUT PNVME_COMPLETION Completion OPTIONAL
)
{
    PNVME_QUEUE AdminQ = &DevExt->AdminQueue;
    PNVME_COMMAND SqEntry;
    volatile NVME_COMPLETION *CqEntry;
    USHORT Tail, Head;
    UCHAR ExpectedPhase;
    ULONG Timeout;
    KIRQL OldIrql;
    
    KsAcquireSpinLock(&AdminQ->Lock, &OldIrql);

    Tail = AdminQ->SubmissionTail;
    SqEntry = &((PNVME_COMMAND)AdminQ->SubmissionVa)[Tail];

    RtlCopyMemory(SqEntry, Command, sizeof(NVME_COMMAND));

    Tail = (Tail + 1) % AdminQ->Depth;
    AdminQ->SubmissionTail = Tail;

    *AdminQ->SubmissionDoorbell = Tail;

    KsReleaseSpinLock(&AdminQ->Lock, OldIrql);

    Head = AdminQ->CompletionHead;
    ExpectedPhase = AdminQ->Phase;
    CqEntry = &((volatile NVME_COMPLETION *)AdminQ->CompletionVa)[Head];

    Timeout = DevExt->ReadyTimeoutMs * 10;
    while ((CqEntry->StatusAndPhase & 0x0001) != ExpectedPhase)
    {
        if (Timeout-- == 0)
        {
            DPRINT("NVMe: Admin command timeout (Opcode 0x%02X)\n", Command->Opcode);
            return STATUS_IO_TIMEOUT;
        }
        KsStallExecutionProcessor(100);
    }

    if (Completion)
    {
        *Completion = *(PNVME_COMPLETION)CqEntry;
    }

    // Advance Head
    Head = (Head + 1) % AdminQ->Depth;
    if (Head == 0)
    {
        AdminQ->Phase ^= 1; // Toggle Phase bit (0 <-> 1)
    }
    AdminQ->CompletionHead = Head;

    // Ring CQ Doorbell
    *AdminQ->CompletionDoorbell = Head;

    // Cek Status (bit 1-15)
    USHORT Status = (CqEntry->StatusAndPhase >> 1) & 0x7FFF;
    if (Status != 0)
    {
        DPRINT("NVMe: Admin command 0x%02X failed with status 0x%04X\n", Command->Opcode, Status);
        return STATUS_IO_DEVICE_ERROR;
    }

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
NVMeIdentifyDevice(IN PNVME_DEVICE_EXTENSION DevExt)
{
    VEASTATUS Status;
    PHYSICAL_ADDRESS HighestPhysAddr;
    PVOID IdentifyBufferVa;
    PHYSICAL_ADDRESS IdentifyBufferPhys;
    NVME_COMMAND Cmd;
    PUCHAR Buffer;
    ULONGLONG Nsze;
    UCHAR Flbas, LbaDs;

    HighestPhysAddr.QuadPart = 0xFFFFFFFFULL;
    IdentifyBufferVa = MmAllocateContiguousMemory(4096, HighestPhysAddr);
    if (!IdentifyBufferVa)
    {
        return STATUS_INSUFFICIENT_MEMORY;
    }
    RtlZeroMemory(IdentifyBufferVa, 4096);
    IdentifyBufferPhys = MmGetPhysicalAddress(IdentifyBufferVa);

    RtlZeroMemory(&Cmd, sizeof(NVME_COMMAND));
    Cmd.Opcode = NVME_ADMIN_IDENTIFY;
    Cmd.CommandId = DevExt->NextCommandId++;
    Cmd.Cdw10 = NVME_CNS_CONTROLLER;
    Cmd.Prp1 = IdentifyBufferPhys.QuadPart;

    Status = NVMeSubmitAdminCommand(DevExt, &Cmd, NULL);
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("NVMe: Identify Controller failed (0x%X)\n", Status);
        MmFreeContiguousMemory(IdentifyBufferVa);
        return Status;
    }

    RtlZeroMemory(IdentifyBufferVa, 4096);
    RtlZeroMemory(&Cmd, sizeof(NVME_COMMAND));
    Cmd.Opcode = NVME_ADMIN_IDENTIFY;
    Cmd.CommandId = DevExt->NextCommandId++;
    Cmd.NamespaceId = 1;
    Cmd.Cdw10 = NVME_CNS_NAMESPACE;
    Cmd.Prp1 = IdentifyBufferPhys.QuadPart;

    Status = NVMeSubmitAdminCommand(DevExt, &Cmd, NULL);
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("NVMe: Identify Namespace failed (0x%X)\n", Status);
        MmFreeContiguousMemory(IdentifyBufferVa);
        return Status;
    }

    Buffer = (PUCHAR)IdentifyBufferVa;
    Nsze = *(PULONGLONG)(Buffer + 0);               // Offset 0: Total Sectors/Blocks
    Flbas = Buffer[26] & 0x0F;                      // Offset 26: Formatted LBA Size Index
    LbaDs = Buffer[128 + (Flbas * 4) + 2];          // LBA Data Size (2^LbaDs, misal 9 = 512B, 12 = 4096B)

    if (LbaDs >= 32)
    {
        DPRINT("NVMe: Unsupported LBA data size exponent %u\n", LbaDs);
        MmFreeContiguousMemory(IdentifyBufferVa);
        return STATUS_NOT_SUPPORTED;
    }

    DevExt->NamespaceId = 1;
    DevExt->NamespaceBlocks = Nsze;
    DevExt->BlockSizeShift = LbaDs;
    DevExt->BlockSize = (1UL << LbaDs);

    DPRINT("NVMe: Namespace 1 Total Blocks = 0x%08X%08X, Block Size = %u Bytes\n",
           (ULONG)(DevExt->NamespaceBlocks >> 32),
           (ULONG)DevExt->NamespaceBlocks,
           DevExt->BlockSize);

    MmFreeContiguousMemory(IdentifyBufferVa);
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
NVMeCreateIoQueue(IN PNVME_DEVICE_EXTENSION DevExt)
{
    VEASTATUS Status;
    PHYSICAL_ADDRESS HighestPhysAddr;
    NVME_COMMAND Cmd;
    PUCHAR BarBase;

    HighestPhysAddr.QuadPart = 0xFFFFFFFFULL;

    DevExt->IoQueue.SubmissionVa = MmAllocateContiguousMemory(
        NVME_IO_QUEUE_DEPTH * NVME_SQ_ENTRY_SIZE, HighestPhysAddr);
    DevExt->IoQueue.CompletionVa = MmAllocateContiguousMemory(
        NVME_IO_QUEUE_DEPTH * NVME_CQ_ENTRY_SIZE, HighestPhysAddr);

    if (!DevExt->IoQueue.SubmissionVa || !DevExt->IoQueue.CompletionVa)
    {
        return STATUS_INSUFFICIENT_MEMORY;
    }

    RtlZeroMemory(DevExt->IoQueue.SubmissionVa, NVME_IO_QUEUE_DEPTH * NVME_SQ_ENTRY_SIZE);
    RtlZeroMemory(DevExt->IoQueue.CompletionVa, NVME_IO_QUEUE_DEPTH * NVME_CQ_ENTRY_SIZE);

    DevExt->IoQueue.SubmissionPhys = MmGetPhysicalAddress(DevExt->IoQueue.SubmissionVa).LowPart;
    DevExt->IoQueue.CompletionPhys = MmGetPhysicalAddress(DevExt->IoQueue.CompletionVa).LowPart;

    DevExt->IoQueue.QueueId = NVME_IO_QUEUE_ID;
    DevExt->IoQueue.Depth = NVME_IO_QUEUE_DEPTH;
    DevExt->IoQueue.Phase = 1;
    KsInitializeSpinlock(&DevExt->IoQueue.Lock);

    BarBase = (PUCHAR)DevExt->Registers;
    DevExt->IoQueue.SubmissionDoorbell = (volatile ULONG *)(BarBase + NVME_DOORBELL_BASE + (2 * DevExt->DoorbellStride));
    DevExt->IoQueue.CompletionDoorbell = (volatile ULONG *)(BarBase + NVME_DOORBELL_BASE + (3 * DevExt->DoorbellStride));

    RtlZeroMemory(&Cmd, sizeof(NVME_COMMAND));
    Cmd.Opcode = NVME_ADMIN_CREATE_IO_CQ;
    Cmd.CommandId = DevExt->NextCommandId++;
    Cmd.Prp1 = DevExt->IoQueue.CompletionPhys;
    Cmd.Cdw10 = ((ULONG)(NVME_IO_QUEUE_DEPTH - 1) << 16) | NVME_IO_QUEUE_ID;
    Cmd.Cdw11 = 0x0003 | (0 << 16);

    Status = NVMeSubmitAdminCommand(DevExt, &Cmd, NULL);
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("NVMe: Create IO CQ failed (0x%X)\n", Status);
        return Status;
    }

    RtlZeroMemory(&Cmd, sizeof(NVME_COMMAND));
    Cmd.Opcode = NVME_ADMIN_CREATE_IO_SQ;
    Cmd.CommandId = DevExt->NextCommandId++;
    Cmd.Prp1 = DevExt->IoQueue.SubmissionPhys;
    Cmd.Cdw10 = ((ULONG)(NVME_IO_QUEUE_DEPTH - 1) << 16) | NVME_IO_QUEUE_ID;
    Cmd.Cdw11 = (NVME_IO_QUEUE_ID << 16) | 0x0001; // CQID = 1, Contiguous = 1

    Status = NVMeSubmitAdminCommand(DevExt, &Cmd, NULL);
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("NVMe: Create IO SQ failed (0x%X)\n", Status);
        return Status;
    }

    DPRINT("NVMe: IO Queue 1 created successfully!\n");
    return STATUS_SUCCESS;
}

BOOLEAN
VEAPI
NVMeResetStateController(IN PNVME_DEVICE_EXTENSION DevExt)
{
    ULONG CapLow, CapHigh;
    ULONG Timeout = 0;

    CapLow = DevExt->Registers->CapLow;
    CapHigh = DevExt->Registers->CapHigh;

    DevExt->MaxQueueEntries = NVME_CAP_MQES(CapLow) + 1;
    DevExt->DoorbellStride = 4 << NVME_CAP_DSTRD(CapHigh);
    DevExt->ReadyTimeoutMs = NVME_CAP_TIMEOUT(CapLow) * 500;

    NVME_DISABLE_CONTROLLER(DevExt->Registers->Cc);

    Timeout = DevExt->ReadyTimeoutMs;
    while (DevExt->Registers->Csts & NVME_CSTS_READY)
    {
        if (Timeout-- == 0)
        {
            DPRINT("NVMe: Reset state controller stuck\n");
            return FALSE;
        }

        KsStallExecutionProcessor(1000);
    }

    return TRUE;
}

BOOLEAN
VEAPI
NVMeEnableController(IN PNVME_DEVICE_EXTENSION DevExt)
{
    ULONG Cc;
    ULONG Timeout;

    // Setup Register CC:
    // CC.EN     = 1
    // CC.CSS    = NVM Command Set (0)
    // CC.MPS    = Host Page Size (0 = 4KB)
    // CC.IOSQES = log2(64) = 6
    // CC.IOCQES = log2(16) = 4
    Cc = NVME_CC_ENABLE |
         NVME_CC_CSS_NVM |
         NVME_CC_MPS(0) |
         NVME_CC_IOSQES(6) |
         NVME_CC_IOCQES(4);

    DevExt->Registers->Cc = Cc;

    Timeout = DevExt->ReadyTimeoutMs;
    while (!(DevExt->Registers->Csts & NVME_CSTS_READY))
    {
        if (Timeout-- == 0)
        {
            DPRINT("NVMe: Enable controller timeout\n");
            return FALSE;
        }

        KsStallExecutionProcessor(1000);
    }

    DevExt->State = NvmeStateStarted;
    DPRINT("NVMe: Controller ENABLED and READY!\n");
    return TRUE;
}

VEASTATUS
VEAPI
NVMeSetupAdminQueue(IN PNVME_DEVICE_EXTENSION DevExt)
{
    PUCHAR BarBase;
    PHYSICAL_ADDRESS HighestPhysAddr, SubmissionPhysAddr, CompletionPhysAddr;

    HighestPhysAddr.QuadPart = 0xFFFFFFFFULL;

    DevExt->AdminQueue.SubmissionVa = MmAllocateContiguousMemory(
        NVME_ADMIN_QUEUE_DEPTH * NVME_SQ_ENTRY_SIZE,
        HighestPhysAddr
    );
    if (!DevExt->AdminQueue.SubmissionVa)
    {
        DPRINT("NVMe: Failed to allocate Admin SQ memory\n");
        return STATUS_INSUFFICIENT_MEMORY;
    }
    RtlZeroMemory(
        DevExt->AdminQueue.SubmissionVa,
        NVME_ADMIN_QUEUE_DEPTH * NVME_SQ_ENTRY_SIZE
    );
    SubmissionPhysAddr = MmGetPhysicalAddress(DevExt->AdminQueue.SubmissionVa);
    DevExt->AdminQueue.SubmissionPhys = SubmissionPhysAddr.LowPart;

    DevExt->AdminQueue.CompletionVa = MmAllocateContiguousMemory(
        NVME_ADMIN_QUEUE_DEPTH * NVME_CQ_ENTRY_SIZE,
        HighestPhysAddr
    );
    if (!DevExt->AdminQueue.CompletionVa)
    {
        DPRINT("NVMe: Failed to allocate Admin CQ memory\n");
        MmFreeContiguousMemory(DevExt->AdminQueue.SubmissionVa);
        DevExt->AdminQueue.SubmissionVa = NULL;
        return STATUS_INSUFFICIENT_MEMORY;
    }
    RtlZeroMemory(
        DevExt->AdminQueue.CompletionVa,
        NVME_ADMIN_QUEUE_DEPTH * NVME_CQ_ENTRY_SIZE
    );
    CompletionPhysAddr = MmGetPhysicalAddress(DevExt->AdminQueue.CompletionVa);
    DevExt->AdminQueue.CompletionPhys = CompletionPhysAddr.LowPart;

    DevExt->AdminQueue.QueueId = NVME_ADMIN_QUEUE_ID;
    DevExt->AdminQueue.Depth = NVME_ADMIN_QUEUE_DEPTH;
    DevExt->AdminQueue.SubmissionTail = 0;
    DevExt->AdminQueue.CompletionHead = 0;
    DevExt->AdminQueue.Phase = 1;
    KsInitializeSpinlock(&DevExt->AdminQueue.Lock);

    BarBase = (PUCHAR)DevExt->Registers;
    DevExt->AdminQueue.SubmissionDoorbell = (volatile ULONG *)(BarBase + NVME_DOORBELL_BASE);
    DevExt->AdminQueue.CompletionDoorbell = (volatile ULONG *)(BarBase + NVME_DOORBELL_BASE + DevExt->DoorbellStride);
    DevExt->Registers->Aqa = ((NVME_ADMIN_QUEUE_DEPTH - 1) << 16) | (NVME_ADMIN_QUEUE_DEPTH - 1);
    DevExt->Registers->AsqLow = DevExt->AdminQueue.SubmissionPhys;
    DevExt->Registers->AsqHigh = 0;
    DevExt->Registers->AcqLow = DevExt->AdminQueue.CompletionPhys;
    DevExt->Registers->AcqHigh = 0;

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
NVMeStartDevice(
    IN PDEVICE_OBJECT Do,
    IN POIP Oip,
    IN POIP_STACK OipStack
)
{
    PNVME_DEVICE_EXTENSION DeviceExtension;
    VEASTATUS Status;
    POBIO_RESOURCE_LIST ResourceList;
    POBIO_RESOURCE_DESCRIPTOR Resource;
    OIP_INFORMATION Information;
    PVOID LowerObject;
    ULONG NVMeBAR, Index;
    BOOLEAN HasMemoryBar;

    DeviceExtension = (PNVME_DEVICE_EXTENSION)Do->DeviceExtension;
    LowerObject = DeviceExtension->NextLowerDevice;
    NVMeBAR = 0;
    HasMemoryBar = FALSE;

    Status = NVMeCallLowerPnp(
        (PDEVICE_OBJECT)LowerObject,
        OIP_MN_QUERY_RESOURCES,
        &Information
    );
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("ObioCallSpecificPnpOip failed: 0x%x\n", Status);
        Oip->StatusInformation.Status = Status;
        return Status;
    }

    ResourceList = (POBIO_RESOURCE_LIST)Information.Information;
    if (!ResourceList)
    {
        DPRINT("NVMe: Lower device returned no resources\n");
        Oip->StatusInformation.Status = STATUS_NOT_FOUND;
        return STATUS_NOT_FOUND;
    }

    for (Index = 0; Index < ResourceList->Count; Index++)
    {
        Resource = &ResourceList->Descriptors[Index];

        if (Resource->Type == PnpResourceTypeMemory)
        {
            DPRINT("NVMe: Memory BAR resource[%u] base=0x%08X%08X length=0x%08X\n",
                   Index,
                   Resource->Memory.Base.HighPart,
                   Resource->Memory.Base.LowPart,
                   Resource->Memory.Length);

            if (!HasMemoryBar)
            {
                NVMeBAR = Resource->Memory.Base.LowPart;
                DeviceExtension->BarLength = Resource->Memory.Length;
                HasMemoryBar = TRUE;
            }
        }
        else if (Resource->Type == PnpResourceTypeInterrupt &&
                 !DeviceExtension->HasInterruptVector)
        {
            DeviceExtension->InterruptVector = Resource->Interrupt.Vector;
            DeviceExtension->HasInterruptVector = TRUE;
        }
    }

    UlFreePoolWithTag(ResourceList, TAG_OBIO);

    if (!HasMemoryBar)
    {
        DPRINT("NVMe: No memory BAR resource was returned\n");
        Oip->StatusInformation.Status = STATUS_NOT_FOUND;
        return STATUS_NOT_FOUND;
    }

    DeviceExtension->BarPhysical = NVMeBAR;
    DeviceExtension->Registers = MmMapIoSpace(NVMeBAR, DeviceExtension->BarLength, MmNonCached);
    if (!DeviceExtension->Registers)
    {
        DPRINT("NVMe: Failed to map regsiters\n");
        return STATUS_INSUFFICIENT_MEMORY;
    }

    Status = NVMeSetupInterrupt(DeviceExtension);
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("NVMe: No Interrupt were supported\n");
        return STATUS_DEVICE_INTERNAL_FAILED;
    }

    if (!NVMeResetStateController(DeviceExtension))
    {
        return STATUS_IO_TIMEOUT;
    }

    Status = NVMeSetupAdminQueue(DeviceExtension);
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    if (!NVMeEnableController(DeviceExtension))
    {
        return STATUS_IO_TIMEOUT;
    }

    Status = NVMeIdentifyDevice(DeviceExtension);
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("Failed to Identify Device: 0x%x\n", Status);
        return Status;
    }

    Status = NVMeCreateIoQueue(DeviceExtension);
    if (!VEA_SUCCESS(Status)) return Status;

    Status = NVMeCreateNamespacePdo(DeviceExtension, 1);
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("Failed to create NVMe PDO: 0x%x\n", Status);
        return Status;
    }

    RtlInitAnsiString(&DeviceExtension->DeviceSymbolic, NULL);

    Status = ObioRegisterDeviceInterface(
        DeviceExtension->NextLowerDevice,
        &VEA_GUID_DEVINTERFACE_NVME,
        NULL,
        &DeviceExtension->DeviceSymbolic
    );
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("NVMe: Failed to register device interface on PDO (0x%X)\n",
               Status);
        Oip->StatusInformation.Status = Status;
        return Status;
    }

    Status = ObioSetDeviceInterfaceState(
        &DeviceExtension->DeviceSymbolic,
        TRUE
    );
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("NVMe: Failed to enable device interface (0x%X)\n", Status);
        Oip->StatusInformation.Status = Status;
        return Status;
    }

    Do->Flags |= DO_DIRECT_IO;
    Do->Flags &= ~DO_DEVICE_INITIALIZING;

    DPRINT("NVMe: selected BAR=0x%08X\n", NVMeBAR);
    Oip->StatusInformation.Status = STATUS_SUCCESS;
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
NVMeMjReadWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN POIP Oip,
    IN POIP_STACK OipStack
)
{
    PNVME_DEVICE_EXTENSION DevExt;
    PNVME_DEVICE_EXTENSION ControllerExt;
    PNVME_QUEUE IoQ;
    NVME_COMMAND Cmd;
    PNVME_COMMAND SqEntry;
    ULONGLONG ByteOffset;
    ULONG Length, BlockCount;
    ULONGLONG StartLba;
    USHORT CommandId, Tail, SearchIndex, Index;
    PVOID Buffer;
    PMDL Mdl;
    PVOID PrpListVa = NULL;
    VEASTATUS Status;
    KIRQL OldIrql;
    BOOLEAN FoundCommandId;

    DevExt = (PNVME_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
    ControllerExt = DevExt->PdoDisk ? DevExt->ControllerExtension : DevExt;
    if (!ControllerExt || ControllerExt->State != NvmeStateStarted)
    {
        Oip->StatusInformation.Status = STATUS_DEVICE_NOT_READY;
        return STATUS_DEVICE_NOT_READY;
    }

    IoQ = &ControllerExt->IoQueue;

    ByteOffset = OipStack->Parameters.ReadWrite.Offset.QuadPart;
    Length = OipStack->Parameters.ReadWrite.Length;
    Mdl = NULL;
    if (Oip->Flags & OIP_BUFFERED_IO)
    {
        Buffer = Oip->AssociatedOip.SystemBuffer;
    }
    else if (Oip->MdlAddress != NULL)
    {
        Mdl = Oip->MdlAddress;
        Buffer = Oip->UserBuffer;
    }
    else
    {
        Buffer = Oip->UserBuffer;
    }

    if (!Buffer ||
        DevExt->BlockSize == 0 ||
        OipStack->Parameters.ReadWrite.Offset.QuadPart < 0 ||
        Length == 0 ||
        (Length % DevExt->BlockSize) != 0 ||
        (ByteOffset & (DevExt->BlockSize - 1)) != 0)
    {
        Oip->StatusInformation.Status = STATUS_INVALID_PARAMETER;
        return STATUS_INVALID_PARAMETER;
    }

    StartLba = ByteOffset >> DevExt->BlockSizeShift;
    BlockCount = Length / DevExt->BlockSize;
    if (BlockCount > 0x10000 ||
        StartLba >= DevExt->NamespaceBlocks ||
        BlockCount > DevExt->NamespaceBlocks - StartLba)
    {
        Oip->StatusInformation.Status = STATUS_INVALID_PARAMETER;
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(&Cmd, sizeof(NVME_COMMAND));

    if (OipStack->MajorFunction == OIP_MJ_READ)
    {
        Cmd.Opcode = NVME_IO_READ;
    }
    else
    {
        Cmd.Opcode = NVME_IO_WRITE;
    }

    Status = NVMeBuildPrp(
        ControllerExt,
        Buffer,
        Mdl,
        Length,
        &Cmd.Prp1,
        &Cmd.Prp2,
        &PrpListVa
    );
    if (!VEA_SUCCESS(Status))
    {
        Oip->StatusInformation.Status = Status;
        return Status;
    }

    Oip->StatusInformation.Status = STATUS_PENDING;
    Oip->StatusInformation.Information = 0;
    KsAcquireSpinLock(&IoQ->Lock, &OldIrql);

    if (ControllerExt->ActiveIoCommands >= NVME_IO_QUEUE_USABLE_ENTRIES)
    {
        KsReleaseSpinLock(&IoQ->Lock, OldIrql);
        if (PrpListVa)
        {
            MmFreeContiguousMemory(PrpListVa);
        }
        Oip->StatusInformation.Status = STATUS_DEVICE_BUSY;
        Oip->StatusInformation.Information = 0;
        return STATUS_DEVICE_BUSY;
    }

    FoundCommandId = FALSE;
    CommandId = ControllerExt->NextCommandId;
    for (Index = 0; Index < NVME_IO_QUEUE_DEPTH; Index++)
    {
        SearchIndex = (CommandId + Index) % NVME_IO_QUEUE_DEPTH;
        if (!ControllerExt->IoRequests[SearchIndex].InUse)
        {
            CommandId = SearchIndex;
            FoundCommandId = TRUE;
            break;
        }
    }

    if (!FoundCommandId)
    {
        KsReleaseSpinLock(&IoQ->Lock, OldIrql);
        if (PrpListVa)
        {
            MmFreeContiguousMemory(PrpListVa);
        }
        Oip->StatusInformation.Status = STATUS_DEVICE_BUSY;
        Oip->StatusInformation.Information = 0;
        return STATUS_DEVICE_BUSY;
    }

    ControllerExt->NextCommandId = (CommandId + 1) % NVME_IO_QUEUE_DEPTH;
    Cmd.CommandId = CommandId;
    Cmd.NamespaceId = DevExt->NamespaceId;

    // CDW10 & CDW11 = Start LBA (64-bit)
    Cmd.Cdw10 = (ULONG)(StartLba & 0xFFFFFFFF);
    Cmd.Cdw11 = (ULONG)(StartLba >> 32);

    // CDW12 = Number of Blocks - 1 (0-based)
    Cmd.Cdw12 = (BlockCount - 1) & 0xFFFF;

    ControllerExt->IoRequests[CommandId].Oip = Oip;
    ControllerExt->IoRequests[CommandId].PrpListVa = PrpListVa;
    ControllerExt->IoRequests[CommandId].Length = Length;
    ControllerExt->IoRequests[CommandId].InUse = TRUE;
    ControllerExt->ActiveIoCommands++;

    Tail = IoQ->SubmissionTail;
    SqEntry = &((PNVME_COMMAND)IoQ->SubmissionVa)[Tail];
    RtlCopyMemory(SqEntry, &Cmd, sizeof(NVME_COMMAND));

    Tail = (Tail + 1) % IoQ->Depth;
    IoQ->SubmissionTail = Tail;

    *IoQ->SubmissionDoorbell = Tail;

    KsReleaseSpinLock(&IoQ->Lock, OldIrql);

    return STATUS_PENDING;
}

/* EOF */