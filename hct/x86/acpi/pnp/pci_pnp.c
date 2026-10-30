/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : pci_pnp.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : ACPI HCT PCI
 ----- Effective since 09-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>
#include "acpi_pnp.h"

/* Revision History ------------------------------------------------------
 * DATE       : 19-09-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file pci_pnp.c
 * --------------------------------------------------------------------- */

/* HACK: Save MmMapIoSpace memory pointer */
ULONG_PTR AcpiEcamVirtualAddress;

static
volatile UCHAR *
AcpiPciGetConfigurationAddress(
    IN PACPI_MCFG_ALLOCATION Allocation,
    IN UCHAR Bus,
    IN UCHAR Device,
    IN UCHAR Function
)
{
    if (!Allocation) {
        return NULL;
    }

    // Map sekali saja jika belum di-map
    if (AcpiEcamVirtualAddress == 0)
    {
        // JANGAN MAP 256 MB DILUAR BATAS KERNEL 32-BIT!
        // Batasi cukup 16 MB (menutupi Bus 00 sampai Bus 0F / 15)
        ULONG MaxBuses = 16;
        ULONG MapSize = MaxBuses << 20; // 16 MB (0x01000000)

        PVOID VirtBase = MmMapIoSpace((ULONG_PTR)Allocation->BaseAddress, MapSize, MmNonCached);
        if (!VirtBase)
        {
            DPRINT("[ACPI-PCI] ERROR: Failed to map ECAM physical 0x%p\n", Allocation->BaseAddress);
            return NULL;
        }

        AcpiEcamVirtualAddress = (ULONG_PTR)VirtBase;
        DPRINT("[ACPI-PCI] ECAM Mapped Safe: Phys 0x%p -> Virt 0x%p (%u MB)\n",
               (ULONG_PTR)Allocation->BaseAddress, AcpiEcamVirtualAddress, MaxBuses);
    }

    // Jika bus yang dicari di atas Bus 15, abaikan agar tidak Out-of-Bounds Read
    if (Bus >= 16) {
        return NULL;
    }

    ULONG Offset = ((ULONG)(Bus - Allocation->StartBus) << 20) |
                   ((ULONG)(Device & 0x1F) << 15) |
                   ((ULONG)(Function & 0x07) << 12);

    return (volatile UCHAR *)(AcpiEcamVirtualAddress + Offset);
}

static
BOOLEAN
AcpiPciReadIds(
    IN PACPI_MCFG_ALLOCATION Allocation,
    IN UCHAR Bus,
    IN UCHAR Device,
    IN UCHAR Function,
    OUT PUSHORT VendorId,
    OUT PUSHORT DeviceId
)
{
    volatile UCHAR *Configuration;
    USHORT ReadVendorId;

    if (!Allocation || Bus < Allocation->StartBus || Bus > Allocation->EndBus ||
        Device >= 32 || Function >= 8 || !VendorId || !DeviceId)
    {
        return FALSE;
    }

    Configuration = AcpiPciGetConfigurationAddress(
        Allocation, Bus, Device, Function);

    if (!Configuration) {
        return FALSE;
    }

    ReadVendorId = *(volatile USHORT *)(Configuration + 0x00);
    if (ReadVendorId == 0xFFFF) {
        return FALSE;
    }

    *VendorId = ReadVendorId;
    *DeviceId = *(volatile USHORT *)(Configuration + 0x02);
    return TRUE;
}

VEASTATUS
AcpiPciQueryResources(
    IN PDEVICE_OBJECT Pdo,
    OUT POBIO_RESOURCE_LIST *Resources
)
{
    PACPI_PCI_PDO_EXTENSION PciExtension;
    PHCT_PDO_EXTENSION ParentExtension;
    PACPI_MCFG_ALLOCATION Allocations;
    PACPI_MCFG_ALLOCATION Allocation = NULL;
    volatile UCHAR *Configuration;
    POBIO_RESOURCE_LIST ResourceList;
    ULONG AllocationCount;
    ULONG BarCount;
    ULONG ResourceCount = 0;
    ULONG ResourceSize;
    UCHAR HeaderType;
    ULONG MaxDescriptors;
    UCHAR InterruptLine, InterruptPin, MsiXCapOffset, MsiCapOffset;
    USHORT StatusRegister;

    if (!Pdo || !Pdo->DeviceExtension || !Resources)
    {
        return STATUS_INVALID_PARAMETER;
    }

    *Resources = NULL;
    PciExtension = (PACPI_PCI_PDO_EXTENSION)Pdo->DeviceExtension;
    if (PciExtension->Signature != ACPI_PCI_PDO_SIGNATURE ||
        !PciExtension->ParentBus ||
        !PciExtension->ParentBus->DeviceExtension)
    {
        return STATUS_NOT_SUPPORTED;
    }

    ParentExtension = (PHCT_PDO_EXTENSION)PciExtension->ParentBus->DeviceExtension;
    if (ParentExtension->MagicSignature != ACPI_HCT_PDO_SIGNATURE ||
        ParentExtension->PdoType != HctDeviceRootPdo ||
        !ParentExtension->McfgTable ||
        ParentExtension->McfgTable->Length < ACPI_MCFG_HEADER_SIZE)
    {
        return STATUS_NOT_SUPPORTED;
    }

    AllocationCount = (ParentExtension->McfgTable->Length -
                       ACPI_MCFG_HEADER_SIZE) / sizeof(ACPI_MCFG_ALLOCATION);

     DPRINT("[ACPI-PCI] MCFG: Length=%u allocations=%u\n",
           ParentExtension->McfgTable->Length, AllocationCount);
    
    Allocations = (PACPI_MCFG_ALLOCATION)(
        (PUCHAR)ParentExtension->McfgTable + ACPI_MCFG_HEADER_SIZE);

    for (ULONG Index = 0; Index < AllocationCount; Index++)
    {
        if (Allocations[Index].SegmentGroup == PciExtension->Segment &&
            PciExtension->Bus >= Allocations[Index].StartBus &&
            PciExtension->Bus <= Allocations[Index].EndBus)
        {
            Allocation = &Allocations[Index];
            break;
        }
    }

    if (!Allocation)
    {
        return STATUS_NOT_FOUND;
    }

    Configuration = AcpiPciGetConfigurationAddress(
        Allocation,
        PciExtension->Bus,
        PciExtension->Device,
        PciExtension->Function
    );
    HeaderType = Configuration[0x0E] & 0x7F;
    BarCount = HeaderType == 0 ? 6 : (HeaderType == 1 ? 2 : 0);
    MaxDescriptors = BarCount + 1;
    ResourceSize = sizeof(OBIO_RESOURCE_LIST) +
                   (MaxDescriptors > 1 ? (MaxDescriptors - 1) * sizeof(OBIO_RESOURCE_DESCRIPTOR) : 0);
    ResourceList = (POBIO_RESOURCE_LIST)UlAllocatePoolZero(
        NonPagedPool,
        ResourceSize,
        TAG_OBIO
    );
    if (!ResourceList)
    {
        return STATUS_INSUFFICIENT_MEMORY;
    }

    for (ULONG BarIndex = 0; BarIndex < BarCount; BarIndex++)
    {
        ULONG BarOffset = 0x10 + BarIndex * sizeof(ULONG);
        volatile ULONG *Bar = (volatile ULONG *)(Configuration + BarOffset);
        ULONG OriginalLow = *Bar;
        ULONG ProbeLow;

        if (OriginalLow == 0 || OriginalLow == 0xFFFFFFFF)
        {
            continue;
        }

        if (OriginalLow & 0x1)
        {
            *Bar = 0xFFFFFFFF;
            ProbeLow = *Bar;
            *Bar = OriginalLow;

            ULONG Mask = ProbeLow & ~0x3U;
            ULONG Length = ~Mask + 1;
            ULONG Base = OriginalLow & ~0x3U;
            if (Mask && Length)
            {
                ResourceList->Descriptors[ResourceCount].Type = PnpResourceTypePort;
                ResourceList->Descriptors[ResourceCount].Port.Base = Base;
                ResourceList->Descriptors[ResourceCount].Port.Length = Length;
                ResourceCount++;
            }
            continue;
        }

        ULONG MemoryType = (OriginalLow >> 1) & 0x3;
        if (MemoryType == 0)
        {
            *Bar = 0xFFFFFFFF;
            ProbeLow = *Bar;
            *Bar = OriginalLow;

            ULONG Mask = ProbeLow & ~0xFU;
            ULONG Length = ~Mask + 1;
            ULONG Base = OriginalLow & ~0xFU;
            DPRINT("[HCT PCI] BAR%u low=%08X probe=%08X mask=%08X length=%08X\n",
                   BarIndex, OriginalLow, ProbeLow, Mask, Length);
            if (Mask && Length)
            {
                ResourceList->Descriptors[ResourceCount].Type = PnpResourceTypeMemory;
                ResourceList->Descriptors[ResourceCount].Memory.Base.QuadPart = Base;
                ResourceList->Descriptors[ResourceCount].Memory.Length = Length;
                ResourceCount++;
            }
        }
        else if (MemoryType == 2 && BarIndex + 1 < BarCount)
        {
            volatile ULONG *BarHigh = Bar + 1;
            ULONG OriginalHigh = *BarHigh;
            ULONG ProbeHigh;
            ULONGLONG Mask;
            ULONGLONG Length;
            ULONGLONG Base;

            *Bar = 0xFFFFFFFF;
            *BarHigh = 0xFFFFFFFF;
            ProbeLow = *Bar;
            ProbeHigh = *BarHigh;
            *BarHigh = OriginalHigh;
            *Bar = OriginalLow;

            Mask = ((ULONGLONG)ProbeHigh << 32) | (ProbeLow & ~0xFULL);
            Length = ~Mask + 1;
            Base = ((ULONGLONG)OriginalHigh << 32) | (OriginalLow & ~0xFULL);
                     DPRINT("[HCT PCI] BAR%u:%u low=%08X high=%08X probe=%08X:%08X length=%08X%08X\n",
                     BarIndex, BarIndex + 1, OriginalLow, OriginalHigh,
                         ProbeHigh, ProbeLow, (ULONG)(Length >> 32), (ULONG)Length);
            if (Mask && Length && Length <= 0xFFFFFFFFULL)
            {
                ResourceList->Descriptors[ResourceCount].Type = PnpResourceTypeMemory;
                ResourceList->Descriptors[ResourceCount].Memory.Base.QuadPart = Base;
                ResourceList->Descriptors[ResourceCount].Memory.Length = (ULONG)Length;
                ResourceCount++;
            }
            BarIndex++;
        }
    }
    
    StatusRegister = *(volatile USHORT *)(Configuration + PCI_STATUS_REGISTER);
    MsiXCapOffset = 0;
    MsiCapOffset  = 0;

    if (StatusRegister & PCI_STATUS_CAPABILITIES_LIST)
    {
        UCHAR CapOffset = Configuration[PCI_CAPABILITIES_POINTER] & ~0x03U;

        while (CapOffset >= 0x40 && CapOffset <= 0xFC)
        {
            UCHAR CapId   = Configuration[CapOffset];
            UCHAR NextCap = Configuration[CapOffset + 1] & ~0x03U;

            if (CapId == PCI_CAP_ID_MSIX && MsiXCapOffset == 0)
            {
                MsiXCapOffset = CapOffset;
            }
            else if (CapId == PCI_CAP_ID_MSI && MsiCapOffset == 0)
            {
                MsiCapOffset = CapOffset;
            }

            if (NextCap == CapOffset || NextCap == 0)
            {
                break;
            }
            CapOffset = NextCap;
        }
    }

    if (MsiXCapOffset != 0)
    {
        USHORT MessageControl = *(volatile USHORT *)(Configuration + MsiXCapOffset + 0x02);
        ULONG TableInfo       = *(volatile ULONG *)(Configuration + MsiXCapOffset + 0x04);
        USHORT TableSize      = (MessageControl & 0x07FF) + 1; // Vector count (Bit 0..10)

        ResourceList->Descriptors[ResourceCount].Type = PnpResourceTypeMessageInterrupt; // Atau PnpResourceTypeMsiX
        ResourceList->Descriptors[ResourceCount].Interrupt.Vector = TableSize; // Menyimpan kuota vector MSI-X
        ResourceList->Descriptors[ResourceCount].Interrupt.Level  = (ULONG)MsiXCapOffset; // Offset Capability

        DPRINT("[HCT PCI] Prioritizing MSI-X: CapOffset=0x%02X TableSize=%u TableInfo=0x%08X\n",
               MsiXCapOffset, TableSize, TableInfo);

        ResourceCount++;
    }
    else if (MsiCapOffset != 0)
    {
        USHORT MessageControl = *(volatile USHORT *)(Configuration + MsiCapOffset + 0x02);
        ULONG MultipleMsgCapable = (MessageControl >> 1) & 0x7;
        ULONG GrantedVectors = 1 << MultipleMsgCapable;

        ResourceList->Descriptors[ResourceCount].Type = PnpResourceTypeMessageInterrupt;
        ResourceList->Descriptors[ResourceCount].Interrupt.Vector = GrantedVectors;
        ResourceList->Descriptors[ResourceCount].Interrupt.Level  = (ULONG)MsiCapOffset;

        DPRINT("[HCT PCI] Prioritizing MSI: CapOffset=0x%02X Vectors=%u\n",
               MsiCapOffset, GrantedVectors);

        ResourceCount++;
    }
    else
    {
        InterruptLine = Configuration[0x3C];
        InterruptPin  = Configuration[0x3D];

        if (InterruptPin != 0 && InterruptLine != 0 && InterruptLine != 0xFF)
        {
            ResourceList->Descriptors[ResourceCount].Type = PnpResourceTypeInterrupt;
            ResourceList->Descriptors[ResourceCount].Interrupt.Vector = (ULONG)InterruptLine;
            ResourceList->Descriptors[ResourceCount].Interrupt.Level  = (ULONG)InterruptLine;

            DPRINT("[HCT PCI] Fallback to Legacy INTx: Line/IRQ=%u Pin=%u\n", 
                   InterruptLine, InterruptPin);

            ResourceCount++;
        }
    }

    ResourceList->Count = ResourceCount;
    DPRINT("[HCT PCI] Returning %u resources for %02X:%02X.%u\n",
           ResourceCount, PciExtension->Bus, PciExtension->Device,
           PciExtension->Function);
    *Resources = ResourceList;
    return STATUS_SUCCESS;
}

static
PACPI_PCI_PDO_EXTENSION
AcpiPciFindPdo(
    IN USHORT Segment,
    IN UCHAR Bus,
    IN UCHAR Device,
    IN UCHAR Function
)
{
    PDEVICE_OBJECT CurrentDevice;

    if (!HctPnpDriverObject) {
        return NULL;
    }

    CurrentDevice = HctPnpDriverObject->DeviceObject;
    while (CurrentDevice) {
        PACPI_PCI_PDO_EXTENSION Extension =
            (PACPI_PCI_PDO_EXTENSION)CurrentDevice->DeviceExtension;

        if (Extension && Extension->Signature == ACPI_PCI_PDO_SIGNATURE &&
            Extension->Segment == Segment && Extension->Bus == Bus &&
            Extension->Device == Device && Extension->Function == Function)
        {
            return Extension;
        }

        CurrentDevice = CurrentDevice->NextDevice;
    }

    return NULL;
}

static
VEASTATUS
AcpiPciGetOrCreatePdo(
    IN PDEVICE_OBJECT ParentBus,
    IN USHORT Segment,
    IN UCHAR Bus,
    IN UCHAR Device,
    IN UCHAR Function,
    IN USHORT VendorId,
    IN USHORT DeviceId,
    OUT PDEVICE_OBJECT *Pdo,
    OUT PBOOLEAN Created OPTIONAL
)
{
    PACPI_PCI_PDO_EXTENSION Extension;
    PDEVICE_OBJECT NewPdo;
    VEASTATUS Status;

    if (Created) *Created = FALSE;

    if (!Pdo) {
        return STATUS_INVALID_PARAMETER;
    }

    Extension = AcpiPciFindPdo(Segment, Bus, Device, Function);
    if (Extension) {
        DPRINT("[ACPI-PCI] %04x:%02x:%02x.%x ven=%04x dev=%04x -> REUSE PDO %p\n",
               Segment, Bus, Device, Function, VendorId, DeviceId, Extension->Self);
        *Pdo = Extension->Self;
        return STATUS_SUCCESS;
    }

    if (!HctPnpDriverObject) {
        DPRINT("[ACPI-PCI] %04x:%02x:%02x.%x -> FAIL: HctPnpDriverObject is NULL\n",
               Segment, Bus, Device, Function);
        return STATUS_UNSUCCESSFUL;
    }

    Status = ObioCreateNewDevice(
        HctPnpDriverObject,
        sizeof(ACPI_PCI_PDO_EXTENSION),
        NULL,
        FILE_DEVICE_BUS_EXTENDER,
        &NewPdo
    );
    if (!VEA_SUCCESS(Status)) {
        DPRINT("[ACPI-PCI] %04x:%02x:%02x.%x -> FAIL: ObioCreateNewDevice 0x%x\n",
               Segment, Bus, Device, Function, Status);
        return Status;
    }

    Extension = (PACPI_PCI_PDO_EXTENSION)NewPdo->DeviceExtension;
    RtlZeroMemory(Extension, sizeof(*Extension));
    Extension->Signature = ACPI_PCI_PDO_SIGNATURE;
    Extension->Self = NewPdo;
    Extension->ParentBus = ParentBus;
    Extension->Segment = Segment;
    Extension->Bus = Bus;
    Extension->Device = Device;
    Extension->Function = Function;
    Extension->VendorId = VendorId;
    Extension->DeviceId = DeviceId;

    DPRINT("[ACPI-PCI] %04x:%02x:%02x.%x ven=%04x dev=%04x -> CREATED PDO %p ext=%p\n",
           Segment, Bus, Device, Function, VendorId, DeviceId, NewPdo, Extension);

    if (Created) *Created = TRUE;
    *Pdo = NewPdo;
    return STATUS_SUCCESS;
}

VEASTATUS
AcpiPciQueryBusRelations(
    IN PDEVICE_OBJECT ParentBus,
    IN PHCT_PDO_EXTENSION ParentExtension,
    OUT PDEVICE_RELATIONS *Relations
)
{
    PACPI_MCFG_ALLOCATION Allocation;
    PACPI_MCFG_ALLOCATION Allocations;
    PDEVICE_RELATIONS DeviceRelations;
    ULONG AllocationCount;
    ULONG DeviceCount = 0;
    ULONG RelationIndex = 0;
    ULONG Size;

    if (!ParentBus || !ParentExtension || !ParentExtension->McfgTable || !Relations) {
        DPRINT("[ACPI-PCI] FAIL: Invalid parameters or McfgTable is NULL!\n");
        return STATUS_INVALID_PARAMETER;
    }

    DPRINT("[ACPI-PCI] McfgTable at %p, Length: %u bytes\n",
           ParentExtension->McfgTable, ParentExtension->McfgTable->Length);

    if (ParentExtension->McfgTable->Length < ACPI_MCFG_HEADER_SIZE) {
        DPRINT("[ACPI-PCI] FAIL: McfgTable Length (%u) < HeaderSize (%u)\n",
               ParentExtension->McfgTable->Length, ACPI_MCFG_HEADER_SIZE);
        return STATUS_INVALID_PARAMETER;
    }

    AllocationCount = (ParentExtension->McfgTable->Length -
                       ACPI_MCFG_HEADER_SIZE) / sizeof(ACPI_MCFG_ALLOCATION);
                       
    DPRINT("[ACPI-PCI] Calculated AllocationCount: %u\n", AllocationCount);

    if (AllocationCount == 0) {
        DPRINT("[ACPI-PCI] WARN: MCFG Table contains 0 allocation structures!\n");
        DPRINT("[ACPI-PCI] HINT: If running QEMU i440fx, use legacy PCI Config Space (Port 0xCF8/0xCFC) instead of MCFG.\n");
    }

    Allocations = (PACPI_MCFG_ALLOCATION)(
        (PUCHAR)ParentExtension->McfgTable + ACPI_MCFG_HEADER_SIZE);

    /* Pass 1: Count valid devices */
    for (ULONG AllocationIndex = 0;
         AllocationIndex < AllocationCount;
         AllocationIndex++)
    {
        Allocation = &Allocations[AllocationIndex];

        DPRINT("[ACPI-PCI] alloc[%u]: base=0x%08x%08x seg=%u bus=%u..%u\n",
               AllocationIndex,
               (ULONG)(Allocation->BaseAddress >> 32),
               (ULONG)(Allocation->BaseAddress & 0xFFFFFFFF),
               Allocation->SegmentGroup, Allocation->StartBus, Allocation->EndBus);

        for (ULONG Bus = Allocation->StartBus;
             Bus <= Allocation->EndBus;
             Bus++)
        {
            for (ULONG Device = 0; Device < 32; Device++) {
                for (ULONG Function = 0; Function < 8; Function++) {
                    USHORT VendorId = 0xFFFF;
                    USHORT DeviceId = 0xFFFF;

                    if (AcpiPciReadIds(Allocation, (UCHAR)Bus,
                                       (UCHAR)Device, (UCHAR)Function,
                                       &VendorId, &DeviceId))
                    {
                        DPRINT("[ACPI-PCI] Found device at %04x:%02x:%02x.%x -> Ven: %04x, Dev: %04x\n",
                               Allocation->SegmentGroup, Bus, Device, Function, VendorId, DeviceId);
                        DeviceCount++;
                    }
                }
            }
        }
        DPRINT("[ACPI-PCI] Pass 1 scan found %u PCI function(s) in alloc[%u]\n", DeviceCount, AllocationIndex);
    }

    if (DeviceCount == 0) {
        DPRINT("[ACPI-PCI] WARN: No PCI functions responded to AcpiPciReadIds.\n");
        DPRINT("[ACPI-PCI] Check if Allocation->BaseAddress requires Virtual Memory Mapping (MmMapIoSpace) before read!\n");
    }

    Size = sizeof(DEVICE_RELATIONS);
    if (DeviceCount > 0) {
        Size += (DeviceCount - 1) * sizeof(PDEVICE_OBJECT);
    }

    DeviceRelations = (PDEVICE_RELATIONS)UlAllocatePoolZero(
        NonPagedPool,
        Size,
        TAG_OBIO
    );
    if (!DeviceRelations) {
        return STATUS_INSUFFICIENT_MEMORY;
    }

    /* Pass 2: Create PDOs */
    for (ULONG AllocationIndex = 0;
         AllocationIndex < AllocationCount;
         AllocationIndex++)
    {        
        Allocation = &Allocations[AllocationIndex];
        for (ULONG Bus = Allocation->StartBus;
             Bus <= Allocation->EndBus;
             Bus++)
        {
            for (ULONG Device = 0; Device < 32; Device++) {
                for (ULONG Function = 0; Function < 8; Function++) {
                    USHORT VendorId;
                    USHORT DeviceId;
                    PDEVICE_OBJECT ChildPdo;
                    VEASTATUS Status;

                    if (!AcpiPciReadIds(Allocation, (UCHAR)Bus,
                                        (UCHAR)Device, (UCHAR)Function,
                                        &VendorId, &DeviceId))
                    {
                        continue;
                    }

                    Status = AcpiPciGetOrCreatePdo(
                        ParentBus,
                        Allocation->SegmentGroup,
                        (UCHAR)Bus,
                        (UCHAR)Device,
                        (UCHAR)Function,
                        VendorId,
                        DeviceId,
                        &ChildPdo,
                        0
                    );
                    
                    if (!VEA_SUCCESS(Status)) {
                        DPRINT("[ACPI-PCI] ERROR: AcpiPciGetOrCreatePdo failed for %04x:%02x:%02x.%x (Status: 0x%X)\n",
                               Allocation->SegmentGroup, Bus, Device, Function, Status);
                        continue;
                    }

                    ObReferenceObject(ChildPdo);
                    DeviceRelations->Objects[RelationIndex] = ChildPdo;
                    
                    DPRINT("[ACPI-PCI] QueryRelations SUCCESS -> Added PDO %p at Relations[%u]\n",
                           ChildPdo, RelationIndex);
                           
                    RelationIndex++;
                }
            }
        }
    }

    DeviceRelations->Count = RelationIndex;
    DPRINT("[ACPI-PCI] QueryBusRelations completed. Total PDOs returned: %u\n", RelationIndex);
    
    *Relations = DeviceRelations;
    return STATUS_SUCCESS;
}

VOID
VEAPI
AcpiPciInterfaceReference(
    IN PVOID Context
)
{
    PACPI_PCI_PDO_EXTENSION PciExtension = (PACPI_PCI_PDO_EXTENSION)Context;
    if (PciExtension && PciExtension->Self)
    {
        ObReferenceObject(PciExtension->Self);
    }
}

VOID
VEAPI
AcpiPciInterfaceDereference(
    IN PVOID Context
)
{
    PACPI_PCI_PDO_EXTENSION PciExtension = (PACPI_PCI_PDO_EXTENSION)Context;
    if (PciExtension && PciExtension->Self)
    {
        ObDereferenceObject(PciExtension->Self);
    }
}

ULONG
VEAPI
AcpiPciGetBusData(
    IN OUT PVOID Context,
    IN ULONG DataType,
    IN OUT PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
)
{
    PACPI_PCI_PDO_EXTENSION PciExtension = (PACPI_PCI_PDO_EXTENSION)Context;
    PHCT_PDO_EXTENSION ParentExtension;
    PACPI_MCFG_ALLOCATION Allocations;
    PACPI_MCFG_ALLOCATION Allocation = NULL;
    ULONG AllocationCount;
    volatile UCHAR *Configuration;

    if (!PciExtension || !Buffer || Length == 0)
    {
        return 0;
    }

    // ECAM Config Space batas maksimalnya 4096 byte (0x1000)
    if (Offset + Length > 0x1000)
    {
        return 0;
    }

    ParentExtension = (PHCT_PDO_EXTENSION)PciExtension->ParentBus->DeviceExtension;
    AllocationCount = (ParentExtension->McfgTable->Length - ACPI_MCFG_HEADER_SIZE) / sizeof(ACPI_MCFG_ALLOCATION);
    Allocations = (PACPI_MCFG_ALLOCATION)((PUCHAR)ParentExtension->McfgTable + ACPI_MCFG_HEADER_SIZE);

    for (ULONG Index = 0; Index < AllocationCount; Index++)
    {
        if (Allocations[Index].SegmentGroup == PciExtension->Segment &&
            PciExtension->Bus >= Allocations[Index].StartBus &&
            PciExtension->Bus <= Allocations[Index].EndBus)
        {
            Allocation = &Allocations[Index];
            break;
        }
    }

    if (!Allocation)
    {
        return 0;
    }

    Configuration = AcpiPciGetConfigurationAddress(
        Allocation,
        PciExtension->Bus,
        PciExtension->Device,
        PciExtension->Function
    );

    if (!Configuration)
    {
        return 0;
    }

    // Read dari MMIO ECAM ke Buffer caller
    for (ULONG i = 0; i < Length; i++)
    {
        ((PUCHAR)Buffer)[i] = Configuration[Offset + i];
    }

    return Length;
}

ULONG
VEAPI
AcpiPciSetBusData(
    IN OUT PVOID Context,
    IN ULONG DataType,
    IN OUT PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
)
{
    PACPI_PCI_PDO_EXTENSION PciExtension = (PACPI_PCI_PDO_EXTENSION)Context;
    PHCT_PDO_EXTENSION ParentExtension;
    PACPI_MCFG_ALLOCATION Allocations;
    PACPI_MCFG_ALLOCATION Allocation = NULL;
    ULONG AllocationCount;
    volatile UCHAR *Configuration;

    if (!PciExtension || !Buffer || Length == 0)
    {
        return 0;
    }

    if (Offset + Length > 0x1000)
    {
        return 0;
    }

    ParentExtension = (PHCT_PDO_EXTENSION)PciExtension->ParentBus->DeviceExtension;
    AllocationCount = (ParentExtension->McfgTable->Length - ACPI_MCFG_HEADER_SIZE) / sizeof(ACPI_MCFG_ALLOCATION);
    Allocations = (PACPI_MCFG_ALLOCATION)((PUCHAR)ParentExtension->McfgTable + ACPI_MCFG_HEADER_SIZE);

    for (ULONG Index = 0; Index < AllocationCount; Index++)
    {
        if (Allocations[Index].SegmentGroup == PciExtension->Segment &&
            PciExtension->Bus >= Allocations[Index].StartBus &&
            PciExtension->Bus <= Allocations[Index].EndBus)
        {
            Allocation = &Allocations[Index];
            break;
        }
    }

    if (!Allocation)
    {
        return 0;
    }

    Configuration = AcpiPciGetConfigurationAddress(
        Allocation,
        PciExtension->Bus,
        PciExtension->Device,
        PciExtension->Function
    );

    if (!Configuration)
    {
        return 0;
    }

    // Write dari Buffer caller ke MMIO ECAM Configuration Space
    for (ULONG i = 0; i < Length; i++)
    {
        Configuration[Offset + i] = ((PUCHAR)Buffer)[i];
    }

    return Length;
}

/* EOF */