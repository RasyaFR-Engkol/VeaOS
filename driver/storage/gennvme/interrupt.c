/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : interrupt.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : PURPOSE
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include "nvmeheader.h"

/* Revision History ------------------------------------------------------
 * DATE       : 08-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file interrupt.c
 * --------------------------------------------------------------------- */

/* VAR */
#define PCI_STATUS_CAP_LIST    (1 << 4)
#define PCI_CAP_ID_MSI         0x05
#define PCI_CAP_ID_MSIX        0x11

/* PRIVATE */

VOID
VEAPI
NVMeProcessIoCompletion(IN PNVME_DEVICE_EXTENSION DevExt)
{
    PNVME_QUEUE IoQ = &DevExt->IoQueue;
    volatile NVME_COMPLETION *CqEntry;
    USHORT Head, CommandId, StatusBits;
    UCHAR Phase;
    NVME_REQUEST_CONTEXT CompletedRequests[NVME_IO_QUEUE_DEPTH];
    USHORT CompletedStatuses[NVME_IO_QUEUE_DEPTH];
    USHORT CompletedCount, Index;
    KIRQL OldIrql;

    CompletedCount = 0;
    KsAcquireSpinLock(&IoQ->Lock, &OldIrql);

    Head = IoQ->CompletionHead;
    Phase = IoQ->Phase;

    while (TRUE)
    {
        if (!IoQ->CompletionVa || IoQ->Depth == 0)
        {
            break;
        }

        CqEntry = &((volatile NVME_COMPLETION *)IoQ->CompletionVa)[Head];

        // Cek apakah ada entry baru (Phase bit cocok dengan Expected Phase)
        if ((CqEntry->StatusAndPhase & 0x0001) != Phase)
        {
            break; // Belum ada CQ entry baru
        }

        CommandId = CqEntry->CommandId;
        StatusBits = (CqEntry->StatusAndPhase >> 1) & 0x7FFF;

        if (CommandId < NVME_IO_QUEUE_DEPTH &&
            DevExt->IoRequests[CommandId].InUse)
        {
            CompletedRequests[CompletedCount] =
                DevExt->IoRequests[CommandId];
            CompletedStatuses[CompletedCount] = StatusBits;
            CompletedCount++;

            RtlZeroMemory(
                &DevExt->IoRequests[CommandId],
                sizeof(DevExt->IoRequests[CommandId]));
            if (DevExt->ActiveIoCommands != 0)
            {
                DevExt->ActiveIoCommands--;
            }
        }
        else
        {
            DPRINT("NVMe: Ignoring completion with invalid or inactive command ID %u\n",
                   CommandId);
        }

        // Advance Head
        Head = (Head + 1) % IoQ->Depth;
        if (Head == 0)
        {
            IoQ->Phase ^= 1; // Toggle phase
            Phase = IoQ->Phase;
        }

        IoQ->CompletionHead = Head;

        // Ring Doorbell IO CQ
        *IoQ->CompletionDoorbell = Head;

    }

    KsReleaseSpinLock(&IoQ->Lock, OldIrql);

    for (Index = 0; Index < CompletedCount; Index++)
    {
        POIP Oip = CompletedRequests[Index].Oip;

        if (CompletedRequests[Index].PrpListVa)
        {
            MmFreeContiguousMemory(CompletedRequests[Index].PrpListVa);
        }

        if (CompletedStatuses[Index] == 0)
        {
            Oip->StatusInformation.Status = STATUS_SUCCESS;
            Oip->StatusInformation.Information =
                CompletedRequests[Index].Length;

            DPRINT("NVMe Complete: Flags=0x%x, UserBuf=%p, SysBuf=%p, Len=%u\n",
                    Oip->Flags,
                    Oip->UserBuffer,
                    Oip->AssociatedOip.SystemBuffer,
                    (ULONG)Oip->StatusInformation.Information);

            if ((Oip->Flags & (OIP_BUFFERED_IO | OIP_INPUT_OPERATION)) ==
                    (OIP_BUFFERED_IO | OIP_INPUT_OPERATION) &&
                Oip->UserBuffer != NULL &&
                Oip->AssociatedOip.SystemBuffer != NULL)
            {
                RtlCopyMemory(
                    Oip->UserBuffer,
                    Oip->AssociatedOip.SystemBuffer,
                    Oip->StatusInformation.Information
                );
            }
        }
        else
        {
            Oip->StatusInformation.Status = STATUS_IO_DEVICE_ERROR;
            Oip->StatusInformation.Information = 0;
        }

        ObioCompleteRequest(Oip);
    }
}

VOID
VEAPI
NVMeCompletionDpcRoutine(PKDPC Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
{
    PNVME_DEVICE_EXTENSION DevExt = (PNVME_DEVICE_EXTENSION)DeferredContext;

    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    NVMeProcessIoCompletion(DevExt);
}

static
BOOLEAN
VEAPI
NVMeInterruptServiceRoutine(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
)
{
    PNVME_DEVICE_EXTENSION DevExt = (PNVME_DEVICE_EXTENSION)ServiceContext;
    PNVME_QUEUE IoQ = &DevExt->IoQueue;
    volatile NVME_COMPLETION *CqEntry;
    USHORT Head;
    UCHAR ExpectedPhase;

    DPRINT("NVMeInterruptServiceRoutine()\n");

    UNREFERENCED_PARAMETER(Interrupt);

    if (IoQ->CompletionVa && IoQ->Depth != 0)
    {
        Head = IoQ->CompletionHead;
        ExpectedPhase = IoQ->Phase;
        CqEntry = &((volatile NVME_COMPLETION *)IoQ->CompletionVa)[Head];

        if ((CqEntry->StatusAndPhase & 0x0001) == ExpectedPhase)
        {
            KsInsertQueueDpc(&DevExt->CompletionDpc, NULL, NULL);
            return TRUE;
        }
    }

    PNVME_QUEUE AdminQ = &DevExt->AdminQueue;
    if (AdminQ->CompletionVa && AdminQ->Depth != 0)
    {
        Head = AdminQ->CompletionHead;
        ExpectedPhase = AdminQ->Phase;
        CqEntry = &((volatile NVME_COMPLETION *)AdminQ->CompletionVa)[Head];

        if ((CqEntry->StatusAndPhase & 0x0001) == ExpectedPhase)
        {
            return TRUE;
        }
    }

    return FALSE;
}

static
VEASTATUS
NVMeConnectMessageInterrupt(
    IN PNVME_DEVICE_EXTENSION DevExt
)
{
    KINTERRUPT_MESSAGE Message;
    VEASTATUS Status;

    Status = ObioConnectMessageInterrupt(
        &DevExt->InterruptObject,
        &Message,
        NVMeInterruptServiceRoutine,
        DevExt,
        &DevExt->InterruptLock,
        3,
        3,
        0
    );
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("NVMe: Failed to connect message interrupt (0x%X)\n", Status);
        return Status;
    }

    DevExt->AdminInterrupt = DevExt->InterruptObject;
    DevExt->MessageCount = 1;
    return STATUS_SUCCESS;
}

static
VEASTATUS
NVMeSetupMsi(
    IN PNVME_DEVICE_EXTENSION DevExt,
    IN UCHAR CapabilityOffset
)
{
    VEASTATUS Status;
    USHORT MessageControl, OriginalMessageControl;
    ULONG AddressLow, AddressHigh;
    BOOLEAN Is64Bit;

    if (CapabilityOffset > 0xF0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    Status = NVMeReadPciConfigWord(
        DevExt->NextLowerDevice,
        CapabilityOffset + 2,
        &MessageControl
    );
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }
    OriginalMessageControl = MessageControl;
    Is64Bit = (MessageControl & (1U << 7)) != 0;

    MessageControl &= ~0x0071U; /* Disable MSI and request one message. */
    Status = NVMeWritePciConfigWord(
        DevExt->NextLowerDevice,
        CapabilityOffset + 2,
        MessageControl
    );
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    Status = NVMeConnectMessageInterrupt(DevExt);
    if (!VEA_SUCCESS(Status))
    {
        goto RestoreControl;
    }

    AddressLow = DevExt->InterruptObject->Message.AddressLow;
    AddressHigh = DevExt->InterruptObject->Message.AddressHigh;
    if (!Is64Bit && AddressHigh != 0)
    {
        Status = STATUS_NOT_SUPPORTED;
        goto Disconnect;
    }

    Status = NVMeWritePciConfigDword(
        DevExt->NextLowerDevice,
        CapabilityOffset + 4,
        AddressLow
    );
    if (!VEA_SUCCESS(Status))
    {
        goto Disconnect;
    }

    if (Is64Bit)
    {
        Status = NVMeWritePciConfigDword(
            DevExt->NextLowerDevice,
            CapabilityOffset + 8,
            AddressHigh
        );
        if (!VEA_SUCCESS(Status))
        {
            goto Disconnect;
        }
    }

    Status = NVMeWritePciConfigWord(
        DevExt->NextLowerDevice,
        CapabilityOffset + (Is64Bit ? 12 : 8),
        (USHORT)DevExt->InterruptObject->Message.Data
    );
    if (!VEA_SUCCESS(Status))
    {
        goto Disconnect;
    }

    MessageControl &= ~0x0070U;
    MessageControl |= 0x0001U;
    Status = NVMeWritePciConfigWord(
        DevExt->NextLowerDevice,
        CapabilityOffset + 2,
        MessageControl
    );
    if (!VEA_SUCCESS(Status))
    {
        goto Disconnect;
    }

    DevExt->InterruptType = InterruptMessageBased;
    DPRINT("NVMe: MSI enabled with one message vector %u\n",
           DevExt->InterruptObject->Message.Vector);
    return STATUS_SUCCESS;

Disconnect:
    ObioDisconnectInterrupt(DevExt->InterruptObject);
    DevExt->InterruptObject = NULL;
    DevExt->AdminInterrupt = NULL;
    DevExt->MessageCount = 0;

RestoreControl:
    OriginalMessageControl &= ~0x0071U;
    if (!VEA_SUCCESS(NVMeWritePciConfigWord(
            DevExt->NextLowerDevice,
            CapabilityOffset + 2,
            OriginalMessageControl)))
    {
        DPRINT("NVMe: Failed to leave MSI disabled after setup failure\n");
    }
    return Status;
}

static
VEASTATUS
NVMeDisableMessageInterrupts(
    IN PNVME_DEVICE_EXTENSION DevExt,
    IN UCHAR MsiOffset,
    IN UCHAR MsixOffset
)
{
    VEASTATUS Status;
    USHORT Control;

    if (MsiOffset != 0)
    {
        Status = NVMeReadPciConfigWord(
            DevExt->NextLowerDevice,
            MsiOffset + 2,
            &Control
        );
        if (!VEA_SUCCESS(Status))
        {
            return Status;
        }

        Control &= ~0x0001U;
        Status = NVMeWritePciConfigWord(
            DevExt->NextLowerDevice,
            MsiOffset + 2,
            Control
        );
        if (!VEA_SUCCESS(Status))
        {
            return Status;
        }
    }

    if (MsixOffset != 0)
    {
        Status = NVMeReadPciConfigWord(
            DevExt->NextLowerDevice,
            MsixOffset + 2,
            &Control
        );
        if (!VEA_SUCCESS(Status))
        {
            return Status;
        }

        Control &= ~(1U << 15);
        Control |= (1U << 14);
        Status = NVMeWritePciConfigWord(
            DevExt->NextLowerDevice,
            MsixOffset + 2,
            Control
        );
        if (!VEA_SUCCESS(Status))
        {
            return Status;
        }
    }

    return STATUS_SUCCESS;
}

static
VEASTATUS
NVMeSetupLineInterrupt(
    IN PNVME_DEVICE_EXTENSION DevExt
)
{
    VEASTATUS Status;
    USHORT Command;
    ULONG IsrVector;
    KIRQL Irql;

    if (!DevExt->HasInterruptVector)
    {
        return STATUS_NOT_FOUND;
    }

    Status = NVMeReadPciConfigWord(
        DevExt->NextLowerDevice,
        0x04,
        &Command
    );
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    Command &= ~(1U << 10);
    Status = NVMeWritePciConfigWord(
        DevExt->NextLowerDevice,
        0x04,
        Command
    );
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    IsrVector = HctIrqVectorToIsrVector(DevExt->InterruptVector);
    if (IsrVector > 0xBF)
    {
        return STATUS_INVALID_PARAMETER;
    }

    Irql = HctConvertVectorToIrql((UCHAR)IsrVector);
    Status = ObioConnectInterrupt(
        &DevExt->InterruptObject,
        NVMeInterruptServiceRoutine,
        DevExt,
        &DevExt->InterruptLock,
        DevExt->InterruptVector,
        Irql,
        Irql,
        LevelSensitive,
        0
    );
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("NVMe: Failed to connect line interrupt (0x%X)\n", Status);
        return Status;
    }

    DevExt->AdminInterrupt = DevExt->InterruptObject;
    DevExt->MessageCount = 1;
    DevExt->InterruptType = InterruptLineBased;
    DPRINT("NVMe: INTx enabled on IRQ %u\n", DevExt->InterruptVector);
    return STATUS_SUCCESS;
}

static
VEASTATUS
NVMeSetupMsix(
    IN PNVME_DEVICE_EXTENSION DevExt,
    IN UCHAR CapabilityOffset
)
{
    VEASTATUS Status;
    ULONG TableInfo, TableOffset, TableBytes, Bar0, BarHigh;
    USHORT MessageControl;
    ULONG TableBir;
    USHORT OriginalMessageControl;
    ULONG BarType;

    if (CapabilityOffset > 0xF8)
    {
        return STATUS_INVALID_PARAMETER;
    }

    Status = NVMeReadPciConfigDword(
        DevExt->NextLowerDevice,
        CapabilityOffset + 4,
        &TableInfo
    );
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }
    TableBir = TableInfo & 0x7;
    TableOffset = TableInfo & ~0x7UL;

    Status = NVMeReadPciConfigWord(
        DevExt->NextLowerDevice,
        CapabilityOffset + 2,
        &MessageControl
    );
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }
    OriginalMessageControl = MessageControl;

    DevExt->MsixTableSize = (MessageControl & 0x07FF) + 1;
    Status = NVMeReadPciConfigDword(
        DevExt->NextLowerDevice,
        0x10,
        &Bar0
    );
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    BarType = Bar0 & 0x6;
    if (BarType == 0x4)
    {
        Status = NVMeReadPciConfigDword(
            DevExt->NextLowerDevice,
            0x14,
            &BarHigh
        );
        if (!VEA_SUCCESS(Status))
        {
            return Status;
        }

        if (BarHigh != 0)
        {
            DPRINT("NVMe: 64-bit BAR0 is above the addressable MMIO range\n");
            return STATUS_NOT_SUPPORTED;
        }
    }

    TableBytes = (ULONG)DevExt->MsixTableSize * 16;
    if (DevExt->MsixTableSize == 0 ||
        TableBir != 0 ||
        (Bar0 & 0x1) != 0 ||
        (BarType != 0 && BarType != 0x4) ||
        (Bar0 & ~0xFUL) != DevExt->BarPhysical ||
        TableOffset > DevExt->BarLength ||
        TableBytes > DevExt->BarLength - TableOffset)
    {
        DPRINT("NVMe: MSI-X table/BAR mismatch (BIR=%u offset=0x%X tableBytes=0x%X rawBAR0=0x%X mappedBAR0=0x%X length=0x%X)\n",
               TableBir, TableOffset, TableBytes, Bar0,
               DevExt->BarPhysical, DevExt->BarLength);
        return STATUS_NOT_SUPPORTED;
    }

    MessageControl |= (1U << 14); /* Mask all vectors while programming. */
    MessageControl &= ~(1U << 15);
    Status = NVMeWritePciConfigWord(
        DevExt->NextLowerDevice,
        CapabilityOffset + 2,
        MessageControl
    );
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    Status = NVMeConnectMessageInterrupt(DevExt);
    if (!VEA_SUCCESS(Status))
    {
        NVMeWritePciConfigWord(
            DevExt->NextLowerDevice,
            CapabilityOffset + 2,
            OriginalMessageControl
        );
        return Status;
    }

    DevExt->MsixTable = (volatile ULONG *)
        ((PUCHAR)DevExt->Registers + TableOffset);

    DevExt->MsixTable[3] = 1;
    DevExt->MsixTable[0] = DevExt->InterruptObject->Message.AddressLow;
    DevExt->MsixTable[1] = DevExt->InterruptObject->Message.AddressHigh;
    DevExt->MsixTable[2] = DevExt->InterruptObject->Message.Data;
    DevExt->MsixTable[3] = 0;

    MessageControl &= ~(1U << 14);
    MessageControl |= (1U << 15);
    Status = NVMeWritePciConfigWord(
        DevExt->NextLowerDevice,
        CapabilityOffset + 2,
        MessageControl
    );
    if (!VEA_SUCCESS(Status))
    {
        goto Failure;
    }

    DevExt->InterruptType = InterruptExtendedMessageBased;
    DPRINT("NVMe: MSI-X enabled with one message vector %u\n",
           DevExt->InterruptObject->Message.Vector);
    return STATUS_SUCCESS;

Failure:
    MessageControl |= (1U << 14);
    MessageControl &= ~(1U << 15);
    NVMeWritePciConfigWord(
        DevExt->NextLowerDevice,
        CapabilityOffset + 2,
        MessageControl
    );
    ObioDisconnectInterrupt(DevExt->InterruptObject);
    DevExt->InterruptObject = NULL;
    DevExt->AdminInterrupt = NULL;
    return Status;
}

/* PUBLIC */
VEASTATUS
VEAPI
NVMeSetupInterrupt(IN PNVME_DEVICE_EXTENSION DevExt)
{
    UCHAR MSIXOffset, MSIOffset;
    VEASTATUS Status;

    MSIXOffset = NVMeFindPciCapability(DevExt->NextLowerDevice, PCI_CAP_ID_MSIX);
    if (MSIXOffset != 0)
    {
        DPRINT("NVMe: Found MSI-X capability at offset 0x%02X\n", MSIXOffset);
        DevExt->MsixCapOffset = MSIXOffset;
        Status = NVMeSetupMsix(DevExt, MSIXOffset);
        if (VEA_SUCCESS(Status))
        {
            return Status;
        }
        DPRINT("NVMe: MSI-X setup failed (0x%X), trying MSI\n", Status);
    }

    MSIOffset = NVMeFindPciCapability(DevExt->NextLowerDevice, PCI_CAP_ID_MSI);
    if (MSIOffset != 0)
    {
        Status = NVMeDisableMessageInterrupts(DevExt, 0, MSIXOffset);
        if (VEA_SUCCESS(Status))
        {
            Status = NVMeSetupMsi(DevExt, MSIOffset);
            if (VEA_SUCCESS(Status))
            {
                return Status;
            }
        }
        DPRINT("NVMe: MSI setup failed (0x%X), trying INTx\n", Status);
    }

    Status = NVMeDisableMessageInterrupts(DevExt, MSIOffset, MSIXOffset);
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("NVMe: Failed to disable message interrupts (0x%X)\n", Status);
        return Status;
    }

    Status = NVMeSetupLineInterrupt(DevExt);
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("NVMe: No usable MSI-X, MSI, or INTx interrupt (0x%X)\n",
               Status);
    }
    return Status;
}

/* EOF */