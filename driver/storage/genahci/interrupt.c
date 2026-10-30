/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : interrupt.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Interrupt AHCI
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include "ahciheader.h"

/* Revision History ------------------------------------------------------
 * DATE       : 26-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file interrupt.c
 * --------------------------------------------------------------------- */

VOID
VEAPI
AhciDpcRoutine(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
)
{
    PAHCI_PORT_CONTEXT PortCtx = (PAHCI_PORT_CONTEXT)DeferredContext;
    PAHCI_PORT_REGISTERS PortRegs;
    ULONG ActiveSlots;
    ULONG PortIs, PxSerr, PxTfd;
    BOOLEAN IsFatalError = FALSE;
    
    if (!PortCtx)
    {
        return;
    }

    PortRegs = PortCtx->Registers;
    PortIs = PortCtx->LastPortIs;
    PxSerr = PortRegs->Serr;
    PxTfd  = PortRegs->Tfd;

    if ((PxTfd & AHCI_PxTFD_STS_ERR) || 
        (PortIs & (AHCI_PxIS_TFES | AHCI_PxIS_HBFS | AHCI_PxIS_HBDS | AHCI_PxIS_IFS)))
    {
        IsFatalError = TRUE;
    }

    if (PortCtx->CompletionEvent)
    {
        // Bangunin thread synchronous internal (Identify / Flush)
        KsSetEvent(PortCtx->CompletionEvent, FALSE);
    }

    ActiveSlots = (PortCtx->DevExt->HbaRegisters->Cap & AHCI_CAP_SNCQ) ? PortRegs->Sact : PortRegs->Ci;

    for (ULONG Slot = 0; Slot < 32; Slot++)
    {
        // Jika slot dialokasikan di software TAPI bit hardware-nya sudah CLEAR (Selesai)
        if ((PortCtx->SlotAllocBitmap & (1 << Slot)) && !(ActiveSlots & (1 << Slot)))
        {
            POIP Oip = (POIP)PortCtx->PendingOip[Slot];
            PortCtx->PendingOip[Slot] = NULL;

            if (Oip)
            {
                POIP_STACK Stack = ObioGetCurrentOipStackLocation(Oip);
                PAHCI_IO_CHUNK_CONTEXT Chunk = &PortCtx->ChunkContext[Slot];

                if (IsFatalError)
                {
                    UCHAR AtaStatus = (UCHAR)(PxTfd & 0xFF);
                    UCHAR AtaError  = (UCHAR)((PxTfd >> 8) & 0xFF);

                    DPRINT("====================================================\n");
                    DPRINT("AHCI FATAL ERROR ON PORT %u (SLOT %u)\n", PortCtx->PortNumber, Slot);
                    DPRINT("  -> PxIS   : 0x%08X\n", PortIs);
                    DPRINT("  -> PxSERR : 0x%08X\n", PxSerr);
                    DPRINT("  -> PxTFD  : 0x%08X (Status: 0x%02X, Error: 0x%02X)\n",
                           PxTfd, AtaStatus, AtaError);
                    DPRINT("====================================================\n");

                    Oip->StatusInformation.Status = STATUS_IO_DEVICE_ERROR;
                    Oip->StatusInformation.Information = 0;
                }
                else
                {
                    ULONG MaxChunkSize = AHCI_COMMAND_TABLE_PRDT_COUNT * 4096;
                    ULONG JustTransferred = (Chunk->RemainingBytes >MaxChunkSize) ? MaxChunkSize : Chunk->RemainingBytes;

                    if (!Chunk->IsWrite && Chunk->UsesBounceBuffer && PortCtx->BounceBufferVirtual)
                    {
                        PVOID UserCurrentBuffer = NULL;
                        PMDL Mdl = Oip->MdlAddress;

                        if (Mdl)
                        {
                            UserCurrentBuffer = (PVOID)((ULONG_PTR)Mdl->MappedSystemVa + Chunk->BufferByteOffset);
                        }
                        else if (Oip->UserBuffer)
                        {
                            UserCurrentBuffer = (PVOID)((ULONG_PTR)Oip->UserBuffer + Chunk->BufferByteOffset);
                        }

                        if (UserCurrentBuffer)
                        {
                            RtlCopyMemory(UserCurrentBuffer, PortCtx->BounceBufferVirtual, JustTransferred);
                        }
                    }

                    Chunk->TransferredBytes += JustTransferred;
                    Chunk->RemainingBytes -= JustTransferred;

                    if (Chunk->RemainingBytes > 0)
                    {
                        Chunk->CurrentStartLba  += (JustTransferred / PortCtx->SectorSize);
                        Chunk->BufferByteOffset += JustTransferred;

                        AhciDispatchSubChunk(PortCtx, Slot);
                        continue;
                    }

                    Oip->StatusInformation.Status = STATUS_SUCCESS;
                    Oip->StatusInformation.Information = Chunk->TransferredBytes;

                    ObioCompleteRequest(Oip);
                    AhciFreeCommandSlot(PortCtx, Slot);
                }
            }
            else
            {
                AhciFreeCommandSlot(PortCtx, Slot);
            }
        }
    }
}

BOOLEAN
VEAPI
AhciIsrRoutine(
    IN struct _KINTERRUPT *Interrupt,
    IN PVOID Context
)
{
    PAHCI_DEVICE_EXTENSION DevExt = (PAHCI_DEVICE_EXTENSION)Context;
    PAHCI_HBA_REGISTERS Hba;
    ULONG PendingInterrupts;
    BOOLEAN Handled = FALSE;

    if (!DevExt || !DevExt->HbaRegisters)
    {
        return FALSE;
    }

    Hba = DevExt->HbaRegisters;

    PendingInterrupts = Hba->Is;
    if (!PendingInterrupts)
    {
        return FALSE;
    }

    for (ULONG i = 0; i < DevExt->MaxPortsCount; i++)
    {
        if (PendingInterrupts & (1 << i))
        {
            PAHCI_PORT_CONTEXT PortCtx = &DevExt->Ports[i];
            PAHCI_PORT_REGISTERS PortRegs = PortCtx->Registers;
            ULONG PortIs = PortRegs->Is;

            /* Clear PxIS and PxSERR (Write-1-to-Clear) */
            PortRegs->Is = PortIs;
            PortRegs->Serr = PortRegs->Serr;

            PortCtx->LastPortIs = PortIs;

            KsInsertQueueDpc(&PortCtx->CompletionDpc, NULL, NULL);

            Handled = TRUE;
        }
    }

    Hba->Is = PendingInterrupts;

    return Handled;
}

VEASTATUS
VEAPI
AhciEnableMsiCapability(
    IN PBUS_INTERFACE_STANDARD BusInterface,
    IN UCHAR CapOffset,
    IN PKINTERRUPT_MESSAGE MsgInfo
)
{
    USHORT MessageControl;
    BOOLEAN Is64BitCapable;

    if (!BusInterface || CapOffset == 0 || !MsgInfo)
    {
        return STATUS_INVALID_PARAMETER;
    }

    MessageControl = AhciReadConfigWord(BusInterface, CapOffset + 0x02);

    // Bit 7: 64-bit Address Capable (1 = 64-bit, 0 = 32-bit)
    Is64BitCapable = (MessageControl & (1 << 7)) != 0;

    AhciWriteConfigDword(BusInterface, CapOffset + 0x04, MsgInfo->AddressLow);

    if (Is64BitCapable)
    {
        // Write Message Address High (Offset Cap + 0x08)
        AhciWriteConfigDword(BusInterface, CapOffset + 0x08, MsgInfo->AddressHigh);

        // Write Message Data (Offset Cap + 0x0C)
        AhciWriteConfigWord(BusInterface, CapOffset + 0x0C, (USHORT)MsgInfo->Data);
    }
    else
    {
        // Write Message Data (Offset Cap + 0x08)
        AhciWriteConfigWord(BusInterface, CapOffset + 0x08, (USHORT)MsgInfo->Data);
    }

    MessageControl |= (1 << 0);
    AhciWriteConfigWord(BusInterface, CapOffset + 0x02, MessageControl);

    DPRINT("AHCI: MSI Enabled (Addr: 0x%08X:%08X, Data: 0x%04X, Vector: 0x%X, 64bit: %d)\n",
           MsgInfo->AddressHigh, MsgInfo->AddressLow, (USHORT)MsgInfo->Data, MsgInfo->Vector, Is64BitCapable);

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
AhciEnableLegacyInterrupt(
    IN PBUS_INTERFACE_STANDARD BusInterface
)
{
    USHORT CommandRegister;

    if (!BusInterface)
    {
        return STATUS_INVALID_PARAMETER;
    }

    CommandRegister = AhciReadConfigWord(BusInterface, PCI_COMMAND_REGISTER);

    if (CommandRegister & PCI_COMMAND_INTERRUPT_DISABLE)
    {
        CommandRegister &= ~PCI_COMMAND_INTERRUPT_DISABLE;
        AhciWriteConfigWord(BusInterface, PCI_COMMAND_REGISTER, CommandRegister);
    }

    DPRINT("AHCI: Legacy INTx Interrupt Enabled (PCI Command: 0x%04X)\n", CommandRegister);

    return STATUS_SUCCESS;
}

VOID
VEAPI
AhciPortTimerRoutine(
    IN PDEVICE_OBJECT DeviceObject,
    IN PVOID Context
)
{
    PAHCI_PORT_CONTEXT PortCtx = (PAHCI_PORT_CONTEXT)Context;
    BOOLEAN NeedReset = FALSE;

    if (!PortCtx->IsActive) return;

    for (ULONG Slot = 0; Slot < 32; Slot++)
    {
        if (PortCtx->SlotAllocBitmap & (1 << Slot))
        {
            PortCtx->SlotTimeoutCount[Slot]++;

            /* Jika request tertahan > 5 detik, tandai butuh recovery hardware */
            if (PortCtx->SlotTimeoutCount[Slot] >= 5)
            {
                NeedReset = TRUE;
            }
        }
        else
        {
            PortCtx->SlotTimeoutCount[Slot] = 0;
        }
    }

    if (NeedReset)
    {
        DPRINT("AHCI: Timeout detected on Port %u. Executing Link Recovery...\n", PortCtx->PortNumber);

        VEASTATUS ResetStatus = AhciResetPortEngine(PortCtx);

        /* Bersihkan seluruh antrean OIP yang menggantung di port ini */
        for (ULONG Slot = 0; Slot < 32; Slot++)
        {
            if (PortCtx->SlotAllocBitmap & (1 << Slot))
            {
                POIP Oip = (POIP)PortCtx->PendingOip[Slot];
                PortCtx->PendingOip[Slot] = NULL;

                if (Oip)
                {
                    if (VEA_SUCCESS(ResetStatus))
                    {
                        Oip->StatusInformation.Status = STATUS_RETRY;
                    }
                    else
                    {
                        Oip->StatusInformation.Status = STATUS_DEVICE_OFFLINE;
                    }

                    Oip->StatusInformation.Information = 0;
                    ObioCompleteRequest(Oip);
                }

                if (PortCtx->CompletionEvent) KsSetEvent(PortCtx->CompletionEvent, FALSE);

                AhciFreeCommandSlot(PortCtx, Slot);
                PortCtx->SlotTimeoutCount[Slot] = 0;
            }
        }
    }
}

/* EOF */