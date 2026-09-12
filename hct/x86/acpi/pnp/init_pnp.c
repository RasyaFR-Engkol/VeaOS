/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : init_pnp.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : ACPI_HCT PNP
 ----- Effective since 08-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 06-08-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file init_pnp.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
AcpiPnpDriverEntry(IN PDRIVER_OBJECT Object);

PDRIVER_OBJECT HctPnpDriverObject = NULL;
PDEVICE_OBJECT HctRootPdo         = NULL;

VOID
VEAPI
HctInitializePnp(VOID)
{
    VEASTATUS Status;
    PACPI_HEADER MADT, MCFG, FADT;

    DPRINT("Initializing ACPI PNP Subsystem\n");

    MADT = HctFindAcpiTable("APIC");
    MCFG = HctFindAcpiTable("MCFG");
    FADT = HctFindAcpiTable("FACP");

    if(MADT)
    {

    }

    if(MCFG)
    {

    }

    if(FADT)
    {

    }

    Status = ObioCreateDriver(
        "HctPnp", 
        AcpiPnpDriverEntry, 
        &HctPnpDriverObject
    );
    
    if(!VEA_SUCCESS(Status))
    {
        KsBugCheckEx(IO1_INITIALIZATION_FAILED, Status, 0x10, 0, 0);
    }

    Status = ObioCreateNewDevice(
        HctPnpDriverObject,
        sizeof(HCT_PDO_EXTENSION),
        "HctPnp",
        FILE_DEVICE_BUS_EXTENDER,
        &HctRootPdo
    );

    if (!VEA_SUCCESS(Status)) {
        KsBugCheckEx(IO1_INITIALIZATION_FAILED, Status, 0x11, 0, 0);
    }

    PHCT_PDO_EXTENSION PdoExt = (PHCT_PDO_EXTENSION)HctRootPdo->DeviceExtension;

    if (PdoExt != NULL) {
        PdoExt->Self       = HctRootPdo;
        PdoExt->PdoType    = HctDeviceRootPdo;
        PdoExt->HardwareID = "ACPI_HAL\\PNP0A08"; // PCI Express Host Bridge ID
        PdoExt->MadtTable  = MADT;
        PdoExt->McfgTable  = MCFG;
        PdoExt->FadtTable  = FADT;
        PdoExt->IsStarted  = TRUE;
    }

    HctRootPdo->Flags &= ~DEVICE_INITIALIZING;
    
    ObioRegisterRootDeviceNode(HctRootPdo, "ACPI_HAL/PNP0A08");
}