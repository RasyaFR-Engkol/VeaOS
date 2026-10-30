/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : pdo.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : PDO Handling
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include "ahciheader.h"

/* Revision History ------------------------------------------------------
 * DATE       : 26-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file pdo.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
AhciCreateAtaPdo(
    IN PAHCI_DEVICE_EXTENSION DevExt,
    IN PAHCI_PORT_CONTEXT PortCtx,
    OUT PDEVICE_OBJECT *OutPdo OPTIONAL
)
{
    PDEVICE_OBJECT LocalPdo = NULL;
    VEASTATUS Status;
    CHAR NameBuffer[64];

    if (!DevExt || !PortCtx)
    {
        return STATUS_INVALID_PARAMETER;
    }

    RtlStringCbPrintfA(NameBuffer, sizeof(NameBuffer), "AhciDiskPort%u", PortCtx->PortNumber);
    
    Status = ObioCreateNewDevice(
        DevExt->FunctionalDeviceObject->DriverObject,
        sizeof(PAHCI_PORT_CONTEXT),
        NameBuffer,
        FILE_DEVICE_DISK,
        &LocalPdo
    );

    if (VEA_SUCCESS(Status) && LocalPdo)
    {
        ATA_IDENTIFY_DATA IdentifyData;
        VEASTATUS IdentifyStatus;
        KIRQL OldIrql;

        *(PAHCI_PORT_CONTEXT*)LocalPdo->DeviceExtension = PortCtx;
        PortCtx->DiskPdo = LocalPdo;

        LocalPdo->Flags |= DO_DIRECT_IO;
        LocalPdo->Flags &= ~DO_DEVICE_INITIALIZING;

        if (OutPdo)
        {
            *OutPdo = LocalPdo;
        }

        DPRINT("AHCI: Successfully spawned Disk PDO %p for Port %u (%s)\n",
               LocalPdo, PortCtx->PortNumber, NameBuffer);

        RtlZeroMemory(&IdentifyData, sizeof(ATA_IDENTIFY_DATA));

        Status = ObioInitializeIoTimer(
            LocalPdo,
            AhciPortTimerRoutine,
            PortCtx
        );

        if (!VEA_SUCCESS(Status))
        {
            DPRINT("AHCI: Failed to Initialize IO Timer: 0x%x\n", Status);
        }
        
        ObioStartTimer(LocalPdo);

        IdentifyStatus = AhciSendIdentifyDevice(DevExt, PortCtx, &IdentifyData);

        if (VEA_SUCCESS(IdentifyStatus))
        {
            AhciParseDeviceCapabilities(PortCtx, &IdentifyData);
            
            // Simpan data kapasitas & sektor ke PortCtx
            PortCtx->TotalSectors = IdentifyData.Max48BitLbaSectors;
            PortCtx->SectorSize = 512; // Default ATA sector size

            ULONGLONG CapacityMB = (PortCtx->TotalSectors * 512) / (1024 * 1024);
            DPRINT("AHCI: Port %u - Disk Capacity: %lu MB (%lu Sectors)\n",
                   PortCtx->PortNumber, (ULONG)CapacityMB, (ULONG)PortCtx->TotalSectors);
        }
        else
        {
            DPRINT("AHCI: WARN - Failed to IDENTIFY device on Port %u (Status: 0x%x)\n",
                   PortCtx->PortNumber, IdentifyStatus);
        }

        KsAcquireSpinLock(&DevExt->ChildPdoListLock, &OldIrql);
        
        InsertTailList(&DevExt->ChildPdoListHead, &PortCtx->ChildListEntry);
        DevExt->ChildPdoCount++;

        KsReleaseSpinLock(&DevExt->ChildPdoListLock, OldIrql);

        DPRINT("AHCI: Added Port %u PDO to FDO Child List (Total Children: %u)\n",
               PortCtx->PortNumber, DevExt->ChildPdoCount);
    }
    else
    {
        DPRINT("AHCI: Failed to create Disk PDO for Port %u (Status: 0x%x)\n",
               PortCtx->PortNumber, Status);
    }

    return Status;
}

VOID
VEAPI
AhciCreatePdoByPort(
    IN PAHCI_DEVICE_EXTENSION DevExt,
    IN PAHCI_PORT_CONTEXT PortCtx
)
{
    AHCI_DEVICE_TYPE DevType;

    if (!DevExt || !PortCtx)
    {
        return;
    }

    /* Deteksi & simpan tipe device ke PortCtx */
    DevType = AhciCheckForSignature(PortCtx);

    switch (DevType)
    {
        case AhciDeviceTypeSata:
        {
            PDEVICE_OBJECT DiskPdo = NULL;
            AhciCreateAtaPdo(DevExt, PortCtx, &DiskPdo);
            break;
        }

        case AhciDeviceTypeSatapi:
        {
            DPRINT("AHCI: Port %u is SATAPI Optical Drive (ATAPI driver required)\n", PortCtx->PortNumber);
            break;
        }

        case AhciDeviceTypeEnclosure:
        {
            DPRINT("AHCI: Port %u is Enclosure Management Bridge\n", PortCtx->PortNumber);
            break;
        }

        case AhciDeviceTypePortMultiplier:
        {
            DPRINT("AHCI: Port %u is Port Multiplier\n", PortCtx->PortNumber);
            break;
        }

        case AhciDeviceTypeUnknown:
        default:
        {
            DPRINT("AHCI: Port %u has unknown device type\n", PortCtx->PortNumber);
            break;
        }
    }
}

/* EOF */