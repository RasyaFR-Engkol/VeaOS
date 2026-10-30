/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : pcicapability.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : PURPOSE
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include "nvmeheader.h"

/* Revision History ------------------------------------------------------
 * DATE       : 08-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file pcicapability.c
 * --------------------------------------------------------------------- */

static
VEASTATUS
NVMeGetPciLocation(
    IN PDEVICE_OBJECT LowerDevice,
    OUT PUCHAR BusNumber,
    OUT PUCHAR SlotNumber
)
{
    VEASTATUS Status;
    OIP_INFORMATION Information;
    POBIO_REQUIRED_RESOURCES_LIST_EX RequiredResources;

    Status = NVMeCallLowerPnp(
        LowerDevice,
        OIP_MN_QUERY_RESOURCE_REQUIREMENTS,
        &Information
    );
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("NVMe: Failed to query PCI location (0x%X)\n", Status);
        return Status;
    }

    RequiredResources = (POBIO_REQUIRED_RESOURCES_LIST_EX)
        (ULONG_PTR)Information.Information;
    if (!RequiredResources ||
        RequiredResources->ListSize < sizeof(OBIO_REQUIRED_RESOURCES_LIST_EX) ||
        RequiredResources->AlternativeLists == 0 ||
        RequiredResources->BusNumber > 0xFF)
    {
        DPRINT("NVMe: Invalid PCI location requirements\n");
        if (RequiredResources)
        {
            UlFreePoolWithTag(RequiredResources, TAG_OBIO);
        }
        return STATUS_INVALID_PARAMETER;
    }

    *BusNumber = (UCHAR)RequiredResources->BusNumber;
    *SlotNumber = (UCHAR)RequiredResources->SlotNumber;
    UlFreePoolWithTag(RequiredResources, TAG_OBIO);
    return STATUS_SUCCESS;
}

static
VEASTATUS
NVMeAccessPciConfigDword(
    IN PDEVICE_OBJECT LowerDevice,
    IN UCHAR Offset,
    IN OUT PULONG Value,
    IN BOOLEAN Write
)
{
    VEASTATUS Status;
    UCHAR BusNumber, SlotNumber;
    ULONG ConfigAddress;
    ULONG DeviceNumber, FunctionNumber;

    if (!Value || (Offset & 0x03) != 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    Status = NVMeGetPciLocation(LowerDevice, &BusNumber, &SlotNumber);
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    DeviceNumber = SlotNumber & 0x1F;
    FunctionNumber = (SlotNumber >> 5) & 0x07;
    ConfigAddress = 0x80000000UL |
                    ((ULONG)BusNumber << 16) |
                    (DeviceNumber << 11) |
                    (FunctionNumber << 8) |
                    Offset;

    WRITE_PORT_ULONG((PULONG)0xCF8, ConfigAddress);
    if (Write)
    {
        WRITE_PORT_ULONG((PULONG)0xCFC, *Value);
    }
    else
    {
        *Value = READ_PORT_ULONG((PULONG)0xCFC);
    }

    return STATUS_SUCCESS;
}

VEASTATUS
NVMeReadPciConfigDword(
    IN PDEVICE_OBJECT LowerDevice,
    IN UCHAR Offset,
    OUT PULONG Value
)
{
    return NVMeAccessPciConfigDword(LowerDevice, Offset, Value, FALSE);
}

VEASTATUS
NVMeWritePciConfigDword(
    IN PDEVICE_OBJECT LowerDevice,
    IN UCHAR Offset,
    IN ULONG Value
)
{
    return NVMeAccessPciConfigDword(LowerDevice, Offset, &Value, TRUE);
}

static
VEASTATUS
NVMeReadPciConfigByte(
    IN PDEVICE_OBJECT LowerDevice,
    IN UCHAR Offset,
    OUT PUCHAR Value
)
{
    VEASTATUS Status;
    ULONG ConfigValue;
    ULONG AlignedOffset = Offset & 0xFC;

    Status = NVMeReadPciConfigDword(
        LowerDevice,
        (UCHAR)AlignedOffset,
        &ConfigValue
    );
    if (VEA_SUCCESS(Status))
    {
        *Value = (UCHAR)(ConfigValue >> ((Offset & 0x03) * 8));
    }

    return Status;
}

VEASTATUS
NVMeReadPciConfigWord(
    IN PDEVICE_OBJECT LowerDevice,
    IN UCHAR Offset,
    OUT PUSHORT Value
)
{
    VEASTATUS Status;
    ULONG ConfigValue;
    ULONG AlignedOffset = Offset & 0xFC;

    if ((Offset & 0x03) == 0x03)
    {
        return STATUS_INVALID_PARAMETER;
    }

    Status = NVMeReadPciConfigDword(
        LowerDevice,
        (UCHAR)AlignedOffset,
        &ConfigValue
    );
    if (VEA_SUCCESS(Status))
    {
        *Value = (USHORT)(ConfigValue >> ((Offset & 0x02) * 8));
    }

    return Status;
}

VEASTATUS
NVMeWritePciConfigWord(
    IN PDEVICE_OBJECT LowerDevice,
    IN UCHAR Offset,
    IN USHORT Value
)
{
    VEASTATUS Status;
    ULONG ConfigValue;
    ULONG Shift;
    ULONG Mask;
    ULONG AlignedOffset = Offset & 0xFC;

    if ((Offset & 0x03) == 0x03)
    {
        return STATUS_INVALID_PARAMETER;
    }

    Status = NVMeReadPciConfigDword(
        LowerDevice,
        (UCHAR)AlignedOffset,
        &ConfigValue
    );
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    Shift = (Offset & 0x02) * 8;
    Mask = 0xFFFFUL << Shift;
    ConfigValue = (ConfigValue & ~Mask) | ((ULONG)Value << Shift);
    return NVMeWritePciConfigDword(
        LowerDevice,
        (UCHAR)AlignedOffset,
        ConfigValue
    );
}

UCHAR
NVMeFindPciCapability(
    IN PDEVICE_OBJECT LowerDevice,
    IN UCHAR TargetCapId
)
{
    VEASTATUS Status;
    UCHAR BusNumber, SlotNumber, Index;
    USHORT PciStatus;
    UCHAR CapOffset;
    UCHAR CapHeader[2];

    if (!LowerDevice)
    {
        return 0;
    }

    Status = NVMeGetPciLocation(LowerDevice, &BusNumber, &SlotNumber);
    if (!VEA_SUCCESS(Status))
    {
        return 0;
    }

    Status = NVMeReadPciConfigWord(LowerDevice, 0x06, &PciStatus);
    if (!VEA_SUCCESS(Status))
    {
        return 0;
    }
    if (!(PciStatus & 0x0010))
    {
        DPRINT("NVMe: PCI device has no capability list\n");
        return 0;
    }

    Status = NVMeReadPciConfigByte(LowerDevice, 0x34, &CapOffset);
    if (!VEA_SUCCESS(Status))
    {
        return 0;
    }
    CapOffset &= 0xFC;

    for (Index = 0; CapOffset != 0 && Index < 48; Index++)
    {
        if (CapOffset < 0x40 || CapOffset > 0xFC)
        {
            DPRINT("NVMe: Invalid PCI capability offset 0x%02X\n", CapOffset);
            return 0;
        }

        Status = NVMeReadPciConfigByte(
            LowerDevice, CapOffset, &CapHeader[0]);
        if (!VEA_SUCCESS(Status))
        {
            return 0;
        }
        Status = NVMeReadPciConfigByte(
            LowerDevice, CapOffset + 1, &CapHeader[1]);
        if (!VEA_SUCCESS(Status))
        {
            return 0;
        }

        if (CapHeader[0] == TargetCapId)
        {
            return CapOffset;
        }

        CapOffset = CapHeader[1] & 0xFC;
    }

    if (CapOffset != 0)
    {
        DPRINT("NVMe: PCI capability list is malformed or cyclic\n");
    }

    return 0;
}

/* EOF */