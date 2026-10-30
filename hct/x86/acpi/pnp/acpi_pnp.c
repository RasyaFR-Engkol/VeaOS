/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : acpi_pnp.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : ACPI_PNP
 ----- Effective since 08-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>
#include "acpi_pnp.h"

/* Revision History ------------------------------------------------------
 * DATE       : 06-08-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file acpi_pnp.c
 * --------------------------------------------------------------------- */

static
PCHAR
AcpiPciBuildDeviceId(
    IN USHORT VendorId,
    IN USHORT DeviceId
)
{
    SIZE_T Size = 22;
    PCHAR String = (PCHAR)UlAllocatePoolZero(
        NonPagedPool,
        Size,
        TAG_OBIO
    );
    if (!String) return NULL;

    RtlStringCbPrintfA(String, Size, "PCI/VEN_%04X&DEV_%04X", VendorId, DeviceId);
    return String;
}

static
PCHAR
AcpiPciBuildInstanceId(
    IN USHORT Segment,
    IN UCHAR Bus,
    IN UCHAR Device,
    IN UCHAR Function
)
{
    SIZE_T Size = 32;
    PCHAR String = (PCHAR)UlAllocatePoolZero(NonPagedPool, Size, TAG_OBIO);
    if (!String) {
        return NULL;
    }

    RtlStringCbPrintfA(String, Size, "BUS_%02X_DEV_%02X_FUNC_%X", Bus, Device, Function);
    return String;
}

VEASTATUS
VEAPI
AcpiPnpProcessMnQueryId(
    IN PVOID Object,
    IN POIP OIP,
    IN POIP_STACK OipStack
)
{
    PDEVICE_OBJECT DeviceObject;
    ULONG_PTR FirstDword;

    /* Check DeviceObject */
    DeviceObject = (PDEVICE_OBJECT)Object;
    if (!DeviceObject || !DeviceObject->DeviceExtension) {
            OIP->StatusInformation.Status = STATUS_INVALID_PARAMETER;
            OIP->StatusInformation.Information = 0;
        return STATUS_INVALID_PARAMETER;
    }

    /* This is more like HACK */
    FirstDword = *(PULONG_PTR)DeviceObject->DeviceExtension;

    /* Is this ACPI PDO? */
    if (FirstDword == ACPI_PCI_PDO_SIGNATURE)
    {
        PACPI_PCI_PDO_EXTENSION PciExt = (PACPI_PCI_PDO_EXTENSION)DeviceObject->DeviceExtension;

        if (OipStack->Parameters.QueryId.IdType == BusQueryDeviceID)
        {
            PCHAR String = AcpiPciBuildDeviceId(PciExt->VendorId, PciExt->DeviceId);
            if (!String) {
                OIP->StatusInformation.Status = STATUS_INSUFFICIENT_MEMORY;
                OIP->StatusInformation.Information = 0;
                return STATUS_INSUFFICIENT_MEMORY;
            }

            OIP->StatusInformation.Status = STATUS_SUCCESS;
            OIP->StatusInformation.Information = (ULONG_PTR)String;
            return STATUS_SUCCESS;
        }
        else if (OipStack->Parameters.QueryId.IdType == BusQueryInstanceID)
        {
            PCHAR String = AcpiPciBuildInstanceId(
                PciExt->Segment,
                PciExt->Bus,
                PciExt->Device,
                PciExt->Function
            );
            if (!String) {
                OIP->StatusInformation.Status = STATUS_INSUFFICIENT_MEMORY;
                OIP->StatusInformation.Information = 0;
                return STATUS_INSUFFICIENT_MEMORY;
            }

            OIP->StatusInformation.Status = STATUS_SUCCESS;
            OIP->StatusInformation.Information = (ULONG_PTR)String;
            return STATUS_SUCCESS;
        }
    }
    else if (FirstDword == ACPI_HCT_PDO_SIGNATURE)
    {
        PHCT_PDO_EXTENSION RootExt = (PHCT_PDO_EXTENSION)DeviceObject->DeviceExtension;

        if (RootExt->PdoType == HctDeviceRootPdo)
        {
            if (OipStack->Parameters.QueryId.IdType == BusQueryDeviceID)
            {
                PCSTR HwId = RootExt->HardwareID ? RootExt->HardwareID : "ACPI_HCT/PNP0A08";
                SIZE_T Len = RtlRawStringLength(HwId) + 1;
                PCHAR String = (PCHAR)UlAllocatePoolZero(NonPagedPool, Len, TAG_OBIO);
                if (!String) {
                    OIP->StatusInformation.Status = STATUS_INSUFFICIENT_MEMORY;
                    OIP->StatusInformation.Information = 0;
                    return STATUS_INSUFFICIENT_MEMORY;
                }

                RtlCopyRawString(String, HwId);
                OIP->StatusInformation.Status = STATUS_SUCCESS;
                OIP->StatusInformation.Information = (ULONG_PTR)String;
                return STATUS_SUCCESS;
            }
            else if (OipStack->Parameters.QueryId.IdType == BusQueryInstanceID)
            {
                // Root PDO kasih aja InstanceID "0"
                PCHAR String = (PCHAR)UlAllocatePoolZero(NonPagedPool, 2, TAG_OBIO);
                if (!String) {
                    OIP->StatusInformation.Status = STATUS_INSUFFICIENT_MEMORY;
                    OIP->StatusInformation.Information = 0;
                    return STATUS_INSUFFICIENT_MEMORY;
                }

                RtlCopyRawString(String, "0");
                OIP->StatusInformation.Status = STATUS_SUCCESS;
                OIP->StatusInformation.Information = (ULONG_PTR)String;
                return STATUS_SUCCESS;
            }
        }
    }

    OIP->StatusInformation.Status = STATUS_NOT_SUPPORTED;
    OIP->StatusInformation.Information = 0;
    return STATUS_NOT_SUPPORTED;
}

VEASTATUS
VEAPI
AcpiPnpProcessMnQueryDeviceRelations(
    IN PVOID Object,
    IN POIP OIP,
    IN POIP_STACK OipStack
)
{
    PDEVICE_OBJECT ParentBus;
    PHCT_PDO_EXTENSION PdoExtension;
    PDEVICE_RELATIONS PciRelations = NULL;
    PDEVICE_RELATIONS CombinedRelations = NULL;
    VEASTATUS Status;
    ULONG TotalCount = 0;
    ULONG_PTR FirstDword;

    DPRINT("AcpiPnpProcessMnQueryDeviceRelations()\n");

    /* Make sure it's bus */
    if (OipStack->Parameters.QueryDeviceRelation.Type != BusRelations)
    {
        DPRINT("AcpiPnpProcessMnQueryDeviceRelations: Other than BusRelations is unsupported\n");
        OIP->StatusInformation.Status = STATUS_NOT_SUPPORTED;
        OIP->StatusInformation.Information = 0;
        return STATUS_NOT_SUPPORTED;
    }

    ParentBus = (PDEVICE_OBJECT)Object;
    if (!ParentBus || !ParentBus->DeviceExtension)
    {
        OIP->StatusInformation.Status = STATUS_INVALID_PARAMETER;
        OIP->StatusInformation.Information = 0;
        return STATUS_INVALID_PARAMETER;
    }

    FirstDword = *(PULONG_PTR)ParentBus->DeviceExtension;

    // Jika ini adalah PCI PDO (Leaf Device seperti Intel HDA), dia TIDAK punya child device.
    if (FirstDword == ACPI_PCI_PDO_SIGNATURE)
    {
        OIP->StatusInformation.Status = STATUS_SUCCESS;
        OIP->StatusInformation.Information = 0; // 0 Child Devices
        return STATUS_SUCCESS;
    }
    else if (FirstDword == ACPI_HCT_PDO_SIGNATURE)
    {
        PdoExtension = (PHCT_PDO_EXTENSION)ParentBus->DeviceExtension;
        if (PdoExtension->PdoType != HctDeviceRootPdo ||
            !PdoExtension->McfgTable)
        {
            OIP->StatusInformation.Status = STATUS_NOT_SUPPORTED;
            OIP->StatusInformation.Information = 0;
            return STATUS_NOT_SUPPORTED;
        }

        if (PdoExtension->McfgTable)
        {   
            Status = AcpiPciQueryBusRelations(
                ParentBus,
                PdoExtension,
                &PciRelations
            );
            if (!VEA_SUCCESS(Status))
            {
                DPRINT("AcpiPciQueryBusRelations() FAILED: 0x%x\n", Status);
                OIP->StatusInformation.Status = Status;
                OIP->StatusInformation.Information = 0;
                return Status;
            }
            else
            {
                DPRINT("AcpiPciQueryBusRelations() success\n");
            }
        }
        else
        {
            DPRINT("AcpiPciQueryBusRelations() NO MCFG TABLE\n");
        }

        if (PciRelations) TotalCount += PciRelations->Count;

        if (TotalCount > 0)
        {
            SIZE_T AllocationSize = sizeof(DEVICE_RELATIONS) + (sizeof(PDEVICE_OBJECT) * (TotalCount - 1));
            CombinedRelations = (PDEVICE_RELATIONS)UlAllocatePoolZero(NonPagedPool, AllocationSize, TAG_OBIO);

            if (!CombinedRelations)
            {
                /* Cleanup bila memori habis */
                if (PciRelations) UlFreePoolWithTag(PciRelations, TAG_OBIO);
                
                OIP->StatusInformation.Status = STATUS_INSUFFICIENT_MEMORY;
                OIP->StatusInformation.Information = 0;
                return STATUS_INSUFFICIENT_MEMORY;
            }

            CombinedRelations->Count = TotalCount;
            ULONG DestIndex = 0;

            /* Salin PDO PCI */
            if (PciRelations)
            {
                for (ULONG i = 0; i < PciRelations->Count; i++)
                {
                    CombinedRelations->Objects[DestIndex++] = PciRelations->Objects[i];
                }
                UlFreePoolWithTag(PciRelations, TAG_OBIO);
            }

            OIP->StatusInformation.Status = STATUS_SUCCESS;
            OIP->StatusInformation.Information = (ULONG_PTR)CombinedRelations;
            return OIP->StatusInformation.Status;
        }

        OIP->StatusInformation.Status = STATUS_SUCCESS;
        OIP->StatusInformation.Information = 0;
        return STATUS_SUCCESS;
    }

    DPRINT("AcpiPnpProcessMnQueryDeviceRelations: Other PDO is unsupported\n");
    OIP->StatusInformation.Status = STATUS_NOT_SUPPORTED;
    OIP->StatusInformation.Information = 0;
    return STATUS_NOT_SUPPORTED;
}

VEASTATUS
VEAPI
AcpiPnpProcessMnQueryResources(
    IN PVOID Object,
    IN POIP OIP
)
{
    PDEVICE_OBJECT DeviceObject = (PDEVICE_OBJECT)Object;
    POBIO_RESOURCE_LIST ResourceList = NULL;
    ULONG_PTR Signature;
    VEASTATUS Status;

    if (!DeviceObject || !DeviceObject->DeviceExtension)
    {
        OIP->StatusInformation.Status = STATUS_INVALID_PARAMETER;
        OIP->StatusInformation.Information = 0;
        return STATUS_INVALID_PARAMETER;
    }

    Signature = *(PULONG_PTR)DeviceObject->DeviceExtension;
    if (Signature != ACPI_PCI_PDO_SIGNATURE)
    {
        OIP->StatusInformation.Status = STATUS_NOT_SUPPORTED;
        OIP->StatusInformation.Information = 0;
        return STATUS_NOT_SUPPORTED;
    }

    Status = AcpiPciQueryResources(DeviceObject, &ResourceList);
    if (!VEA_SUCCESS(Status))
    {
        OIP->StatusInformation.Status = Status;
        OIP->StatusInformation.Information = 0;
        return Status;
    }

    OIP->StatusInformation.Status = STATUS_SUCCESS;
    OIP->StatusInformation.Information = (ULONG_PTR)ResourceList;
    return STATUS_SUCCESS;
}

static
VEASTATUS
AcpiPnpProcessMnQueryResourceRequirements(
    IN PVOID Object,
    IN POIP OIP
)
{
    PDEVICE_OBJECT DeviceObject = (PDEVICE_OBJECT)Object;
    PACPI_PCI_PDO_EXTENSION PciExtension;
    POBIO_REQUIRED_RESOURCES_LIST_EX ListEx;

    if (!DeviceObject || !DeviceObject->DeviceExtension || !OIP)
    {
        return STATUS_INVALID_PARAMETER;
    }

    PciExtension = (PACPI_PCI_PDO_EXTENSION)DeviceObject->DeviceExtension;
    if (PciExtension->Signature != ACPI_PCI_PDO_SIGNATURE)
    {
        OIP->StatusInformation.Status = STATUS_NOT_SUPPORTED;
        OIP->StatusInformation.Information = 0;
        return STATUS_NOT_SUPPORTED;
    }

    ListEx = (POBIO_REQUIRED_RESOURCES_LIST_EX)UlAllocatePoolZero(
        NonPagedPool,
        sizeof(OBIO_REQUIRED_RESOURCES_LIST_EX),
        TAG_OBIO
    );
    if (!ListEx)
    {
        OIP->StatusInformation.Status = STATUS_INSUFFICIENT_MEMORY;
        OIP->StatusInformation.Information = 0;
        return STATUS_INSUFFICIENT_MEMORY;
    }

    ListEx->ListSize = sizeof(OBIO_REQUIRED_RESOURCES_LIST_EX);
    ListEx->BusNumber = PciExtension->Bus;
    ListEx->SlotNumber = PciExtension->Device |
                         ((ULONG)PciExtension->Function << 5);
    ListEx->AlternativeLists = 1;
    ListEx->List[0].Version = 1;
    ListEx->List[0].Revision = 1;
    ListEx->List[0].Count = 0;

    OIP->StatusInformation.Status = STATUS_SUCCESS;
    OIP->StatusInformation.Information = (ULONG_PTR)ListEx;
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
AcpiPnpProcessMnQueryInterface(
    IN PVOID Object,
    IN POIP OIP,
    IN POIP_STACK OipStack
)
{
    PDEVICE_OBJECT DeviceObject = (PDEVICE_OBJECT)Object;
    const GUID *InterfaceGuid = OipStack->Parameters.QueryInterface.InterfaceType;

    if (IsEqualGUID(InterfaceGuid, &VEA_ACPI_INTERFACE_ID))
    {
        PVEA_ACPI_INTERFACE AcpiInterface = (PVEA_ACPI_INTERFACE)OipStack->Parameters.QueryInterface.InterfaceSpecificData;

        if (OipStack->Parameters.QueryInterface.Size >= sizeof(VEA_ACPI_INTERFACE))
        {
            DPRINT("HCT: Giving ACPI Interface\n");

            AcpiInterface->GetFadt = HctGetFadt;
            AcpiInterface->GetDsdt = HctGetDsdt;
            AcpiInterface->GetSsdt = HctGetSsdt;
            AcpiInterface->Context = DeviceObject->DeviceExtension;

            OIP->StatusInformation.Status = STATUS_SUCCESS;
            return STATUS_SUCCESS;
        }
        else
        {
            return STATUS_INVALID_PARAMETER;
        }
    }
    else if (IsEqualGUID(InterfaceGuid, &VEA_BUS_INTERFACE_STANDARD))
    {
        PBUS_INTERFACE_STANDARD InterfaceStandard = (PBUS_INTERFACE_STANDARD)OipStack->Parameters.QueryInterface.InterfaceSpecificData;

        if (OipStack->Parameters.QueryInterface.Size >= sizeof(BUS_INTERFACE_STANDARD))
        {
            DPRINT("HCT: Giving Standar Bus Interface\n");

            InterfaceStandard->Size = sizeof(BUS_INTERFACE_STANDARD);
            InterfaceStandard->Version = 1;
            InterfaceStandard->Context = DeviceObject->DeviceExtension; // PACPI_PCI_PDO_EXTENSION
            InterfaceStandard->InterfaceReference = AcpiPciInterfaceReference;
            InterfaceStandard->InterfaceDereference = AcpiPciInterfaceDereference;
            InterfaceStandard->GetBusData = AcpiPciGetBusData;
            InterfaceStandard->SetBusData = AcpiPciSetBusData;

            AcpiPciInterfaceReference(InterfaceStandard->Context);
            
            OIP->StatusInformation.Status = STATUS_SUCCESS;
            return STATUS_SUCCESS;
        }
    }

    return STATUS_NOT_SUPPORTED;
}

VEASTATUS
VEAPI
AcpiPnpProcessMnAddDevice(
    IN PVOID Object,
    IN POIP OIP,
    IN POIP_STACK OipStack
)
{
    PDEVICE_OBJECT DeviceObject = (PDEVICE_OBJECT)Object;
    VEASTATUS Status;

    return STATUS_NOT_SUPPORTED;
}

VEASTATUS
VEAPI
AcpiPnpInterruptHandler(
    IN PVOID Object,
    IN POIP OIP
)
{
    POIP_STACK OipStack = ObioGetCurrentOipStackLocation(OIP);

    DPRINT("AcpiPnpInterruptHandler()\n");
    
    switch(OipStack->MajorFunction)
    {
        case OIP_MJ_PNP:
        {
            switch (OipStack->MinorFunction)
            {
                case OIP_MN_QUERY_ID:
                {
                    return AcpiPnpProcessMnQueryId(Object, OIP, OipStack);
                }

                case OIP_MN_QUERY_DEVICE_RELATIONS:
                {
                    return AcpiPnpProcessMnQueryDeviceRelations(
                        Object,
                        OIP,
                        OipStack
                    );
                }

                case OIP_MN_QUERY_RESOURCES:
                {
                    return AcpiPnpProcessMnQueryResources(Object, OIP);
                }

                case OIP_MN_QUERY_RESOURCE_REQUIREMENTS:
                {
                    return AcpiPnpProcessMnQueryResourceRequirements(Object, OIP);
                }

                case OIP_MN_ADD_DEVICE:
                {
                    return AcpiPnpProcessMnAddDevice(
                        Object,
                        OIP,
                        OipStack
                    );
                }

                case OIP_MN_QUERY_INTERFACE:
                {
                    return AcpiPnpProcessMnQueryInterface(
                        Object,
                        OIP,
                        OipStack
                    );
                }
                
                case OIP_MN_START_DEVICE:
                {
                    /* Its already started */
                    OIP->StatusInformation.Status = STATUS_SUCCESS;
                    return STATUS_SUCCESS;
                }

                default:    
                    DPRINT("Unhandled OIP_MN_PNP: %d\n", OipStack->MinorFunction);
                    OIP->StatusInformation.Status = STATUS_NOT_SUPPORTED;
                    OIP->StatusInformation.Information = 0;
                    return STATUS_NOT_SUPPORTED;
            }
        }

        case OIP_MJ_POWER:
        {
            OIP->StatusInformation.Status = STATUS_SUCCESS;
            OIP->StatusInformation.Information = 0;
            return STATUS_SUCCESS;
        }

        default:
            DPRINT("Unhandled OIP_MJ: %d\n", OipStack->MajorFunction);
            OIP->StatusInformation.Status = STATUS_SUCCESS;
            OIP->StatusInformation.Information = 0;
            return STATUS_NOT_SUPPORTED;
    }

    OIP->StatusInformation.Status = STATUS_SUCCESS;
    return OIP->StatusInformation.Status;
}

VEASTATUS
VEAPI
AcpiPnpDriverEntry(IN PDRIVER_OBJECT Object)
{
    if(!Object) return STATUS_INVALID_PARAMETER;
    DPRINT("AcpiPnpDriverEntry\n");
    return ObioAttachInterruptToThisObject((PVOID)Object, AcpiPnpInterruptHandler);
}