/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : init.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Header file for ...
 ----- Effective since 09-2026 til FOREVER  ------------------------------- */

#pragma once

#include "procbind.h"
#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 19-09-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file init.c
 * --------------------------------------------------------------------- */

#define ACPI_MCFG_HEADER_SIZE       (sizeof(ACPI_HEADER) + sizeof(ULONGLONG))
#define ACPI_PCI_PDO_SIGNATURE      0x4F494350 /* "PCIO" */
#define ACPI_HCT_PDO_SIGNATURE      'TCH_'  /* _HCT */
#define ACPI_VOCPI_PDO_SIGNATURE    'IPCV' /* VCPI*/

#pragma pack(push, 1)
typedef struct _ACPI_MCFG_ALLOCATION {
    ULONGLONG BaseAddress;
    USHORT SegmentGroup;
    UCHAR StartBus;
    UCHAR EndBus;
    ULONG Reserved;
} ACPI_MCFG_ALLOCATION, *PACPI_MCFG_ALLOCATION;
#pragma pack(pop)

typedef struct _ACPI_PCI_PDO_EXTENSION {
    ULONG Signature;
    PDEVICE_OBJECT Self;
    PDEVICE_OBJECT ParentBus;
    USHORT Segment;
    UCHAR Bus;
    UCHAR Device;
    UCHAR Function;
    USHORT VendorId;
    USHORT DeviceId;
} ACPI_PCI_PDO_EXTENSION, *PACPI_PCI_PDO_EXTENSION;
extern PDRIVER_OBJECT HctPnpDriverObject;

VEASTATUS
AcpiPciQueryBusRelations(
    IN PDEVICE_OBJECT ParentBus,
    IN PHCT_PDO_EXTENSION ParentExtension,
    OUT PDEVICE_RELATIONS *Relations
);

VEASTATUS
AcpiPciQueryResources(
    IN PDEVICE_OBJECT Pdo,
    OUT POBIO_RESOURCE_LIST *Resources
);

VEASTATUS
VEAPI
VOCPIPnpQueryDeviceRelations(
    IN PDEVICE_OBJECT ParentBus,
    OUT PDEVICE_RELATIONS *OutRelations
);

#define PCI_STATUS_REGISTER             0x06
#define PCI_STATUS_CAPABILITIES_LIST    0x0010
#define PCI_CAPABILITIES_POINTER        0x34

#define PCI_CAP_ID_MSI                  0x05
#define PCI_CAP_ID_MSIX                 0x11

VOID
VEAPI
AcpiPciInterfaceDereference(
    IN PVOID Context
);

VOID
VEAPI
AcpiPciInterfaceReference(
    IN PVOID Context
);

ULONG
VEAPI
AcpiPciGetBusData(
    IN OUT PVOID Context,
    IN ULONG DataType,
    IN OUT PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
);

ULONG
VEAPI
AcpiPciSetBusData(
    IN OUT PVOID Context,
    IN ULONG DataType,
    IN OUT PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
);

/* EOF */