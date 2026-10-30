/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : engine.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : PURPOSE
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include "ahciheader.h"

/* Revision History ------------------------------------------------------
 * DATE       : 26-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file engine.c
 * --------------------------------------------------------------------- */

static
VEASTATUS
AhciStopPortEngine(
    IN PAHCI_PORT_REGISTERS PortRegs
)
{
    ULONG Timeout = 100000;

    // Clear ST (Start) dan FRE (FIS Receive Enable)
    PortRegs->Cmd &= ~AHCI_PxCMD_ST;
    PortRegs->Cmd &= ~AHCI_PxCMD_FRE;

    // Tunggu sampai CR dan FR mati
    while (Timeout > 0)
    {
        if (!(PortRegs->Cmd & AHCI_PxCMD_CR) && !(PortRegs->Cmd & AHCI_PxCMD_FR))
        {
            return STATUS_SUCCESS;
        }
        KsStallExecutionProcessor(10);
        Timeout--;
    }

    return STATUS_TIMEOUT;
}

VEASTATUS
VEAPI
AhciInitializePorts(IN PAHCI_DEVICE_EXTENSION DevExt)
{
    VEASTATUS Status;
    PAHCI_HBA_REGISTERS Hba = DevExt->HbaRegisters;
    ULONG PortNum;
    PHYSICAL_ADDRESS MaxPhysicalAddress;

    DPRINT("AHCI: Initializing Ports (MaxPorts: %u, PortsImplemented: 0x%08X)\n",
           DevExt->MaxPortsCount, DevExt->PortsImplementedMask);

    RtlZeroMemory(DevExt->Ports, sizeof(DevExt->Ports));
    
    /* Determine MaxPhysicalAddress */
    if (DevExt->Supports64BitDma)
    {
        MaxPhysicalAddress.QuadPart = 0xFFFFFFFFFFFFFFFFULL;
    }
    else
    {
        MaxPhysicalAddress.QuadPart = 0xFFFFFFFFULL;
    }

    /* Loop every available MaxPortsCount */
    for (PortNum = 0; PortNum < DevExt->MaxPortsCount; PortNum++)
    {
        PAHCI_PORT_REGISTERS PortRegs;
        PAHCI_PORT_CONTEXT PortCtx;
        ULONG Ssts, Det;
        ULONG TotalPortDmaSize;
        PVOID DmaVirtual;
        PHYSICAL_ADDRESS DmaPhysical;

        /* Is it implementes/even exist? */
        if (!(DevExt->PortsImplementedMask & (1 << PortNum)))
        {
            continue;
        }

        /* Get PortContext and Port Regsters*/
        PortCtx  = &DevExt->Ports[PortNum];
        PortRegs = &Hba->Ports[PortNum];

        PortCtx->PortNumber = PortNum;
        PortCtx->Registers  = PortRegs;
        PortCtx->DevExt     = DevExt;
        PortCtx->IsActive     = FALSE;
        PortCtx->TotalSectors = 0;
        PortCtx->SectorSize   = 512;
        PortCtx->SlotAllocBitmap = 0;
        RtlZeroMemory(PortCtx->ModelNumber, sizeof(PortCtx->ModelNumber));
        RtlZeroMemory(PortCtx->PendingOip, sizeof(PortCtx->PendingOip));

        /* Initialize Spinlock & Dpc for PortContext */
        KsInitializeSpinlock(&PortCtx->PortLock);
        KsInitializeDpc(&PortCtx->CompletionDpc, AhciDpcRoutine, PortCtx);

        /* Get Ssts and Det value */
        Ssts = PortRegs->Ssts;
        Det  = Ssts & AHCI_PxSSTS_DET_MASK;

        DPRINT("AHCI: Scanning Port %u - SSTS: 0x%08X (DET: %u), SIG: 0x%08X\n",
               PortNum, Ssts, Det, PortRegs->Sig);

        /* The port PHY should be established and ready to go */
        if (Det != AHCI_PxSSTS_DET_PRESENT_PHY)
        {
            continue;
        }

        DPRINT("AHCI: Port %u - Active SATA device detected!\n", PortNum);

        /* Stop first to reset the engine */
        Status = AhciStopPortEngine(PortRegs);
        if (!VEA_SUCCESS(Status))
        {
            DPRINT("AHCI: Port %u - Failed to stop port engine\n", PortNum);
            continue;
        }

        /* Make DMA Memory for the command */
        TotalPortDmaSize = 1024 + 256 + (AHCI_CMD_TABLE_SIZE * DevExt->CommandSlotsCount);
        DmaVirtual = MmAllocateContiguousMemory(TotalPortDmaSize, MaxPhysicalAddress);
        if (!DmaVirtual)
        {
            DPRINT("AHCI: Port %u - Failed to allocate DMA memory\n", PortNum);
            continue;
        }

        RtlZeroMemory(DmaVirtual, TotalPortDmaSize);

        DmaPhysical = MmGetPhysicalAddress(DmaVirtual);

        PortCtx->CommandListVirtual  = (PAHCI_COMMAND_HEADER)DmaVirtual;
        PortCtx->CommandListPhysical = DmaPhysical;
        PortCtx->ReceivedFisVirtual  = (PVOID)((ULONG_PTR)DmaVirtual + 1024);
        PortCtx->ReceivedFisPhysical.QuadPart = DmaPhysical.QuadPart + 1024;

        /* Initialize the port registers */
        PortRegs->Clb  = PortCtx->CommandListPhysical.LowPart;
        PortRegs->Clbu = PortCtx->CommandListPhysical.HighPart;
        PortRegs->Fb   = PortCtx->ReceivedFisPhysical.LowPart;
        PortRegs->Fbu  = PortCtx->ReceivedFisPhysical.HighPart;
        PortRegs->Serr = PortRegs->Serr;
        PortRegs->Is   = PortRegs->Is;
        PortRegs->Ie = AHCI_PxIS_DHRS | AHCI_PxIS_PSS | AHCI_PxIS_SDBS | 
                       AHCI_PxIS_PRCS | AHCI_PxIS_ERROR_MASK;
        PortRegs->Cmd |= AHCI_PxCMD_FRE;

        if (DevExt->HbaRegisters->Cap & AHCI_CAP_SSS)
        {
            PortRegs->Cmd |= AHCI_PxCMD_SUD;
        }

        PortRegs->Cmd |= AHCI_PxCMD_ST;
        PortCtx->IsActive = TRUE;

        PortCtx->BounceBufferVirtual = MmAllocateContiguousMemory(64 * 1024, MaxPhysicalAddress);
        if (PortCtx->BounceBufferVirtual)
        {
            RtlZeroMemory(PortCtx->BounceBufferVirtual, 64 * 1024);
            PortCtx->BounceBufferPhysical = MmGetPhysicalAddress(PortCtx->BounceBufferVirtual);
            DPRINT("AHCI: Port %u - Allocated 64KB Bounce Buffer at Phys: 0x%llx\n",
                PortNum, PortCtx->BounceBufferPhysical.QuadPart);
        }
        else
        {
            DPRINT("AHCI: WARN - Port %u failed to allocate Bounce Buffer!\n", PortNum);
        }
    }

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
AhciStartHbaEngine(
    IN PAHCI_DEVICE_EXTENSION DevExt
)
{
    VEASTATUS Status;
    ULONG PortNum;

    if (!DevExt || !DevExt->HbaRegisters)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (DevExt->HbaRegisters->Cap2 & AHCI_CAP2_BOHC)
    {
        DevExt->HbaRegisters->Bohc |= AHCI_BOHC_OOS;
        
        ULONG Timeout = 10000;
        while ((DevExt->HbaRegisters->Bohc & AHCI_BOHC_BOS) && Timeout > 0)
        {
            KsStallExecutionProcessor(100);
            Timeout--;
        }
    }

    DevExt->HbaRegisters->Ghc |= AHCI_GHC_AE;

    DevExt->PortsImplementedMask = DevExt->HbaRegisters->Pi;
    DevExt->MaxPortsCount        = (DevExt->HbaRegisters->Cap & AHCI_CAP_NP_MASK) + 1;
    DevExt->CommandSlotsCount    = ((DevExt->HbaRegisters->Cap & AHCI_CAP_NCS_MASK) >> 8) + 1;
    DevExt->Supports64BitDma     = (DevExt->HbaRegisters->Cap & AHCI_CAP_S64A) != 0;

    DPRINT("AHCI: HBA Capable - PortsMask: 0x%08X, MaxPorts: %u, CmdSlots: %u, 64BitDMA: %d\n",
           DevExt->PortsImplementedMask,
           DevExt->MaxPortsCount,
           DevExt->CommandSlotsCount,
           DevExt->Supports64BitDma);

    DevExt->HbaRegisters->Is = DevExt->HbaRegisters->Is;

    Status = AhciInitializePorts(DevExt);
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("AHCI: Failed to initialize HBA ports: 0x%x\n", Status);
        return Status;
    }

    DevExt->HbaRegisters->Ghc |= AHCI_GHC_IE;

    for (PortNum = 0; PortNum < DevExt->MaxPortsCount; PortNum++)
    {
        PAHCI_PORT_CONTEXT PortCtx = &DevExt->Ports[PortNum];

        if (PortCtx->IsActive)
        {
            AhciCreatePdoByPort(DevExt, PortCtx);
        }
    }

    return STATUS_SUCCESS;
}

/* EOF */