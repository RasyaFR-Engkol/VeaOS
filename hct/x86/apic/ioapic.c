/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : ioapic.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : IOAPIC and GSI Routing
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>
#include "apic.h"

#pragma pack(push, 1)
typedef struct _HCT_MADT_HEADER
{
    ACPI_HEADER Header;
    ULONG LocalApicAddress;
    ULONG Flags;
    UCHAR Entries[];
} HCT_MADT_HEADER, *PHCT_MADT_HEADER;

typedef struct _HCT_MADT_IO_APIC
{
    UCHAR Type;
    UCHAR Length;
    UCHAR Id;
    UCHAR Reserved;
    ULONG Address;
    ULONG GsiBase;
} HCT_MADT_IO_APIC, *PHCT_MADT_IO_APIC;

typedef struct _HCT_MADT_ISO
{
    UCHAR Type;
    UCHAR Length;
    UCHAR Bus;
    UCHAR Source;
    ULONG Gsi;
    USHORT Flags;
} HCT_MADT_ISO, *PHCT_MADT_ISO;
#pragma pack(pop)

typedef struct _HCT_IOAPIC_CONTROLLER
{
    volatile ULONG *Registers;
    ULONG GsiBase;
    ULONG RedirectionCount;
    UCHAR Id;
} HCT_IOAPIC_CONTROLLER, *PHCT_IOAPIC_CONTROLLER;

typedef struct _HCT_GSI_ENTRY
{
    ULONG Gsi;
    ULONG ControllerIndex;
    ULONG Pin;
    BOOLEAN Valid;
} HCT_GSI_ENTRY, *PHCT_GSI_ENTRY;

typedef struct _HCT_ISA_OVERRIDE
{
    ULONG Gsi;
    USHORT Flags;
    BOOLEAN Valid;
} HCT_ISA_OVERRIDE, *PHCT_ISA_OVERRIDE;

static HCT_IOAPIC_CONTROLLER HctIoApics[HCT_MAX_IOAPICS];
static ULONG HctIoApicCount;
static HCT_GSI_ENTRY HctGsiTable[HCT_MAX_GSI_COUNT];
static ULONG HctGsiCount;
static HCT_ISA_OVERRIDE HctIsaOverrides[HCT_MAX_ISA_IRQS];

static
ULONG
HctpIoApicRead(
    IN PHCT_IOAPIC_CONTROLLER Controller,
    IN ULONG Register
)
{
    Controller->Registers[IOAPIC_REGSEL / sizeof(ULONG)] = Register;
    return Controller->Registers[IOAPIC_IOWIN / sizeof(ULONG)];
}

static
VOID
HctpIoApicWrite(
    IN PHCT_IOAPIC_CONTROLLER Controller,
    IN ULONG Register,
    IN ULONG Value
)
{
    Controller->Registers[IOAPIC_REGSEL / sizeof(ULONG)] = Register;
    Controller->Registers[IOAPIC_IOWIN / sizeof(ULONG)] = Value;
}

static
PHCT_GSI_ENTRY
HctpFindGsi(IN ULONG Gsi)
{
    ULONG Index;

    for (Index = 0; Index < HctGsiCount; Index++)
    {
        if (HctGsiTable[Index].Valid && HctGsiTable[Index].Gsi == Gsi)
        {
            return &HctGsiTable[Index];
        }
    }

    return NULL;
}

VEASTATUS
VEAPI
HctInitializeIoApic(IN PACPI_HEADER MadtTable)
{
    PHCT_MADT_HEADER Madt;
    ULONG Offset;
    ULONG Index;
    ULONG Destination;

    if (!MadtTable ||
        MadtTable->Signature[0] != 'A' ||
        MadtTable->Signature[1] != 'P' ||
        MadtTable->Signature[2] != 'I' ||
        MadtTable->Signature[3] != 'C' ||
        MadtTable->Length < sizeof(HCT_MADT_HEADER))
    {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(HctIoApics, sizeof(HctIoApics));
    RtlZeroMemory(HctGsiTable, sizeof(HctGsiTable));
    RtlZeroMemory(HctIsaOverrides, sizeof(HctIsaOverrides));
    HctIoApicCount = 0;
    HctGsiCount = 0;
    Madt = (PHCT_MADT_HEADER)MadtTable;
    Offset = sizeof(HCT_MADT_HEADER);

    while (Offset < MadtTable->Length)
    {
        UCHAR Type;
        UCHAR Length;

        if (MadtTable->Length - Offset < 2)
        {
            return STATUS_INVALID_PARAMETER;
        }

        Type = Madt->Entries[Offset - sizeof(HCT_MADT_HEADER)];
        Length = Madt->Entries[Offset - sizeof(HCT_MADT_HEADER) + 1];
        if (Length < 2 || Length > MadtTable->Length - Offset)
        {
            return STATUS_INVALID_PARAMETER;
        }

        if (Type == MADT_ENTRY_IO_APIC)
        {
            PHCT_MADT_IO_APIC Entry;

            if (Length < sizeof(HCT_MADT_IO_APIC) ||
                HctIoApicCount == HCT_MAX_IOAPICS)
            {
                return STATUS_NOT_SUPPORTED;
            }

            Entry = (PHCT_MADT_IO_APIC)&Madt->Entries[
                Offset - sizeof(HCT_MADT_HEADER)];
            if (!Entry->Address || (Entry->Address & 0xFFF))
            {
                return STATUS_INVALID_PARAMETER;
            }

            HctIoApics[HctIoApicCount].Registers =
                (volatile ULONG *)(ULONG_PTR)Entry->Address;
            HctIoApics[HctIoApicCount].GsiBase = Entry->GsiBase;
            HctIoApics[HctIoApicCount].Id = Entry->Id;
            HctIoApicCount++;
        }
        else if (Type == MADT_ENTRY_ISO)
        {
            PHCT_MADT_ISO Entry;

            if (Length < sizeof(HCT_MADT_ISO))
            {
                return STATUS_INVALID_PARAMETER;
            }

            Entry = (PHCT_MADT_ISO)&Madt->Entries[
                Offset - sizeof(HCT_MADT_HEADER)];
            if (Entry->Bus == 0 && Entry->Source < HCT_MAX_ISA_IRQS)
            {
                HctIsaOverrides[Entry->Source].Gsi = Entry->Gsi;
                HctIsaOverrides[Entry->Source].Flags = Entry->Flags;
                HctIsaOverrides[Entry->Source].Valid = TRUE;
            }
        }

        Offset += Length;
    }

    if (!HctIoApicCount)
    {
        return STATUS_NOT_FOUND;
    }

    Destination = (ApicRead(APIC_REG_ID) >> 24) & 0xFF;
    for (Index = 0; Index < HctIoApicCount; Index++)
    {
        PHCT_IOAPIC_CONTROLLER Controller = &HctIoApics[Index];
        ULONG Version = HctpIoApicRead(Controller, IOAPIC_REG_VERSION);
        ULONG RedirectionCount = ((Version >> 16) & 0xFF) + 1;
        ULONG Pin;

        if (RedirectionCount > HCT_MAX_GSI_COUNT - HctGsiCount)
        {
            return STATUS_NOT_SUPPORTED;
        }

        Controller->RedirectionCount = RedirectionCount;
        for (Pin = 0; Pin < RedirectionCount; Pin++)
        {
            ULONG Gsi = Controller->GsiBase + Pin;
            PHCT_GSI_ENTRY GsiEntry;
            ULONG RedirectionRegister = IOAPIC_REG_REDIR_BASE + (Pin * 2);

            if (HctpFindGsi(Gsi))
            {
                return STATUS_INVALID_PARAMETER;
            }

            GsiEntry = &HctGsiTable[HctGsiCount++];
            GsiEntry->Gsi = Gsi;
            GsiEntry->ControllerIndex = Index;
            GsiEntry->Pin = Pin;
            GsiEntry->Valid = TRUE;

            HctpIoApicWrite(
                Controller,
                RedirectionRegister + 1,
                Destination << 24);
            HctpIoApicWrite(
                Controller,
                RedirectionRegister,
                0x20 | IOAPIC_REDIR_MASKED);

            DPRINT("IOAPIC %u: GSI %u -> pin %u masked\n",
                   Controller->Id, Gsi, Pin);
        }
    }

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
HctTranslateIrqToGsi(
    IN ULONG Irq,
    OUT PULONG Gsi,
    OUT PUSHORT Flags
)
{
    ULONG ResolvedGsi = Irq;
    USHORT ResolvedFlags = 0;

    if (!Gsi || !Flags)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (Irq < HCT_MAX_ISA_IRQS && HctIsaOverrides[Irq].Valid)
    {
        ResolvedGsi = HctIsaOverrides[Irq].Gsi;
        ResolvedFlags = HctIsaOverrides[Irq].Flags;
    }

    if (!HctpFindGsi(ResolvedGsi))
    {
        return STATUS_NOT_FOUND;
    }

    *Gsi = ResolvedGsi;
    *Flags = ResolvedFlags;
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
HctSetGsiRedirection(
    IN ULONG Gsi,
    IN ULONG Vector,
    IN USHORT Flags,
    IN BOOLEAN Masked
)
{
    PHCT_GSI_ENTRY GsiEntry;
    PHCT_IOAPIC_CONTROLLER Controller;
    ULONG RedirectionRegister;
    ULONG Low;
    ULONG Polarity;
    ULONG Trigger;
    ULONG Destination;

    if (Vector < 0x20 || Vector >= 0xFF)
    {
        return STATUS_INVALID_PARAMETER;
    }

    GsiEntry = HctpFindGsi(Gsi);
    if (!GsiEntry)
    {
        return STATUS_NOT_FOUND;
    }

    Polarity = Flags & 0x3;
    Trigger = (Flags >> 2) & 0x3;
    if (Polarity == 2 || Trigger == 2)
    {
        return STATUS_INVALID_PARAMETER;
    }

    Controller = &HctIoApics[GsiEntry->ControllerIndex];
    RedirectionRegister = IOAPIC_REG_REDIR_BASE + (GsiEntry->Pin * 2);
    Destination = (ApicRead(APIC_REG_ID) >> 24) & 0xFF;
    Low = Vector;

    if (Polarity == 3)
    {
        Low |= IOAPIC_REDIR_POLARITY;
    }

    if (Trigger == 3)
    {
        Low |= IOAPIC_REDIR_TRIGGER;
    }

    if (Masked)
    {
        Low |= IOAPIC_REDIR_MASKED;
    }

    HctpIoApicWrite(
        Controller,
        RedirectionRegister,
        HctpIoApicRead(Controller, RedirectionRegister) | IOAPIC_REDIR_MASKED);
    HctpIoApicWrite(
        Controller,
        RedirectionRegister + 1,
        Destination << 24);
    HctpIoApicWrite(Controller, RedirectionRegister, Low);

    DPRINT("IOAPIC: GSI %u routed to vector 0x%X %s\n",
           Gsi, Vector, Masked ? "masked" : "unmasked");
    return STATUS_SUCCESS;
}