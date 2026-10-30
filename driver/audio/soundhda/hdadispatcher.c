/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : hdadispatcher.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : PURPOSE
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veaddk.h>
#include "hdaheader.h"
#include "hdaioctl.h"
#include "veaguid.h"

/* Revision History ------------------------------------------------------
 * DATE       : 02-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file hdadispatcher.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
HDAStartDevice(
    IN PVOID Object,
    IN POIP Oip,
    IN POIP_STACK OipStack
)
{
    PDEVICE_OBJECT Fdo = (PDEVICE_OBJECT)Object;
    PHDA_DEVICE_EXTENSION DevExt;
    POIP ResourceOip = NULL;
    POIP RequiredOip = NULL;
    POIP_STACK NextStack = NULL;
    POBIO_RESOURCE_LIST ResourceList = NULL;
    POBIO_REQUIRED_RESOURCES_LIST_EX RequiredList = NULL;
    POBIO_RESOURCE_DESCRIPTOR Desc = NULL;
    POBJECT_HEADER ObjectHeader;
    VEASTATUS Status;
    ULONG i, Timeout, Gctl;
    ULONG BusNumber, SlotNumber, DeviceNumber, FunctionNumber;
    ULONG ConfigAddress, Cmd;

    if (!Fdo || !Fdo->DeviceExtension) {
        Oip->StatusInformation.Status = STATUS_INVALID_PARAMETER;
        Oip->StatusInformation.Information = 0;
        return STATUS_INVALID_PARAMETER;
    }

    DevExt = (PHDA_DEVICE_EXTENSION)Fdo->DeviceExtension;

    ResourceOip = ObioAllocateOip(1);
    if (!ResourceOip) {
        DPRINT("HDA: Failed to allocate Resource OIP\n");
        Oip->StatusInformation.Status = STATUS_INSUFFICIENT_MEMORY;
        Oip->StatusInformation.Information = 0;
        return STATUS_INSUFFICIENT_MEMORY;
    }

    NextStack = ResourceOip->CurrentStackLocation - 1;
    NextStack->MajorFunction = OIP_MJ_PNP;
    NextStack->MinorFunction = OIP_MN_QUERY_RESOURCES;

    Status = ObioGoInterruptObject(DevExt->LowerDeviceObject, ResourceOip);

    if (!VEA_SUCCESS(Status) || !VEA_SUCCESS(ResourceOip->StatusInformation.Status)) {
        DPRINT("HDA: Lower device failed OIP_MN_QUERY_RESOURCE (Status: 0x%X)\n", Status);
        UlFreePoolWithTag(ResourceOip, TAG_OB_OIP);
        
        Oip->StatusInformation.Status = STATUS_UNSUCCESSFUL;
        Oip->StatusInformation.Information = 0;
        return STATUS_UNSUCCESSFUL;
    }

    ResourceList = (POBIO_RESOURCE_LIST)ResourceOip->StatusInformation.Information;
    if (!ResourceList) {
        DPRINT("HDA: Bus driver returned NULL resource list!\n");
        UlFreePoolWithTag(ResourceOip, OB_OIP_TAG);

        Oip->StatusInformation.Status = STATUS_UNSUCCESSFUL;
        Oip->StatusInformation.Information = 0;
        return STATUS_UNSUCCESSFUL;
    }

    DPRINT("HDA: Resource list %p, count %u\n", ResourceList, ResourceList->Count);
    for (i = 0; i < ResourceList->Count; i++)
    {
        Desc = &ResourceList->Descriptors[i];

        DPRINT("HDA: Resource[%u] type=%u raw length=%08X\n",
               i, Desc->Type, Desc->Memory.Length);
        if (Desc->Type == OBIO_RESOURCE_TYPE_MEMORY) // MMIO / BAR
        {
            PHYSICAL_ADDRESS Bar0Phy = Desc->Memory.Base;
            ULONG Bar0Length = Desc->Memory.Length;

            if (Bar0Length == 0)
            {
                DPRINT("HDA: Rejecting zero-length memory resource\n");
                continue;
            }

                 DPRINT("HDA: BAR0 Physical Base: 0x%08X, Length: 0x%08X\n",
                     Bar0Phy.LowPart, Bar0Length);

            // Map physical BAR0 ke Virtual Memory Kernel (Wajib NonCached!)
            DevExt->HdaMMIOBase = (PUCHAR)MmMapIoSpace(
                (ULONG)Bar0Phy.QuadPart,
                Bar0Length,
                MmNonCached
            );

            if (!DevExt->HdaMMIOBase) {
                DPRINT("HDA: Failed to map MMIO space for BAR0!\n");
                break;
            }

            DevExt->HdaMmioLength = Bar0Length;
        }
        else if (Desc->Type == OBIO_RESOURCE_TYPE_INTERRUPT)
        {
            DevExt->InterruptVector = Desc->Interrupt.Vector;
            DPRINT("HDA: Assigned Interrupt Vector: %u\n", DevExt->InterruptVector);
        }
    }

    UlFreePoolWithTag(ResourceList, TAG_OBIO);
    UlFreePoolWithTag(ResourceOip, OB_OIP_TAG);

    if (!DevExt->HdaMMIOBase) {
        DPRINT("HDA: BAR0 MMIO was not mapped properly!\n");
        Oip->StatusInformation.Status = STATUS_UNSUCCESSFUL;
        Oip->StatusInformation.Information = 0;
        return STATUS_UNSUCCESSFUL;
    }

    DPRINT("HDA: Resources Acquired\n");

    ObjectHeader = OBJECT_TO_OBJECT_HEADER(Fdo);
    RequiredOip = ObioAllocateOip(ObjectHeader->StackDepth);
    if (!RequiredOip) {
        DPRINT("HDA: Failed to allocate resource requirements OIP\n");
        Oip->StatusInformation.Status = STATUS_INSUFFICIENT_MEMORY;
        return STATUS_INSUFFICIENT_MEMORY;
    }

    NextStack = ObioGetNextOipStackLocation(RequiredOip);
    NextStack->MajorFunction = OIP_MJ_PNP;
    NextStack->MinorFunction = OIP_MN_QUERY_RESOURCE_REQUIREMENTS;

    Status = ObioGoInterruptObject(DevExt->LowerDeviceObject, RequiredOip);
    if (!VEA_SUCCESS(Status) ||
        !VEA_SUCCESS(RequiredOip->StatusInformation.Status)) {
        DPRINT("HDA: Lower device failed resource requirements query (0x%X)\n", Status);
        UlFreePoolWithTag(RequiredOip, OB_OIP_TAG);
        Oip->StatusInformation.Status = STATUS_UNSUCCESSFUL;
        return STATUS_UNSUCCESSFUL;
    }

    RequiredList = (POBIO_REQUIRED_RESOURCES_LIST_EX)
        RequiredOip->StatusInformation.Information;
    if (!RequiredList || !RequiredList->AlternativeLists) {
        DPRINT("HDA: HCT returned no PCI location requirements\n");
        if (RequiredList) UlFreePoolWithTag(RequiredList, TAG_OBIO);
        UlFreePoolWithTag(RequiredOip, OB_OIP_TAG);
        Oip->StatusInformation.Status = STATUS_NOT_FOUND;
        return STATUS_NOT_FOUND;
    }

    BusNumber = RequiredList->BusNumber;
    SlotNumber = RequiredList->SlotNumber;
    UlFreePoolWithTag(RequiredList, TAG_OBIO);
    UlFreePoolWithTag(RequiredOip, OB_OIP_TAG);

    DeviceNumber = SlotNumber & 0x1F;
    FunctionNumber = (SlotNumber >> 5) & 0x07;
    if (BusNumber > 0xFF || DeviceNumber >= 32 || FunctionNumber >= 8) {
        DPRINT("HDA: HCT returned invalid PCI location %u:%u.%u\n",
               BusNumber, DeviceNumber, FunctionNumber);
        Oip->StatusInformation.Status = STATUS_INVALID_PARAMETER;
        return STATUS_INVALID_PARAMETER;
    }

    ConfigAddress = 0x80000000UL | (BusNumber << 16) |
                    (DeviceNumber << 11) | (FunctionNumber << 8) | 0x04;

    WRITE_PORT_ULONG((PULONG)0xCF8, ConfigAddress);
    Cmd = READ_PORT_ULONG((PULONG)0xCFC);
    Cmd = (Cmd & 0x0000FFFF) | 0x0006;      /* Memory Space + Bus Master; jangan tulis balik bit Status */
    WRITE_PORT_ULONG((PULONG)0xCF8, ConfigAddress);
    WRITE_PORT_ULONG((PULONG)0xCFC, Cmd);

    WRITE_PORT_ULONG((PULONG)0xCF8, ConfigAddress);
    DPRINT("HDA: PCI CMD readback = %04X\n", READ_PORT_ULONG((PULONG)0xCFC) & 0xFFFF);

    if (DevExt->InterruptVector == 0) {
        ULONG Reg;
        WRITE_PORT_ULONG((PULONG)0xCF8,
                         (ConfigAddress & ~0xFCUL) | 0x3CUL);
        Reg = READ_PORT_ULONG((PULONG)0xCFC);
        DPRINT("HDA: PCI IntLine=%u IntPin=%u\n", Reg & 0xFF, (Reg >> 8) & 0xFF);
        if (((Reg >> 8) & 0xFF) != 0 && (Reg & 0xFF) != 0xFF) {
            DevExt->InterruptVector = Reg & 0xFF;
        }
    }

    Status = HDAResetController(DevExt);
    if (!VEA_SUCCESS(Status)) {
        DPRINT("HDA: Failed to reset HDA controller!\n");
        Oip->StatusInformation.Status = Status;
        return Status;
    }

    Status = HDAInitCorbRirb(DevExt);
    if (!VEA_SUCCESS(Status)) {
        DPRINT("HDA: Failed to initialize CORB/RIRB!\n");
        Oip->StatusInformation.Status = Status;
        return Status;
    }

    Status = HDAProbeCodecs(DevExt);
    if (!VEA_SUCCESS(Status)) {
        DPRINT("HDA: Codec probing failed!\n");
        Oip->StatusInformation.Status = Status;
        return Status;
    }

    Status = HDAEnumerateWidgets(DevExt);
    if (!VEA_SUCCESS(Status)) {
        DPRINT("HDA: Widget enumeration failed!\n");
        Oip->StatusInformation.Status = Status;
        return Status;
    }

    Status = HDAFindOutputPath(DevExt);
    if (!VEA_SUCCESS(Status)) {
        Oip->StatusInformation.Status = Status;
        return Status;
    }

    Status = HDASetupOutputPath(DevExt);
    if (!VEA_SUCCESS(Status)) {
        Oip->StatusInformation.Status = Status;
        return Status;
    }

    Status = HDASetupOutputStream(DevExt);
    if (!VEA_SUCCESS(Status)) {
        Oip->StatusInformation.Status = Status;
        return Status;
    }

    PUCHAR Sd1 = DevExt->HdaMMIOBase + DevExt->StreamDescriptorOffset;

    Status = HDAConnectInterrupt(DevExt);
    if (VEA_SUCCESS(Status)) {
        HDAEnableInterrupts(DevExt);
    } else {
        DPRINT("HDA: Interrupt not connected (0x%X), polling only\n", Status);
    }

    {
        PUCHAR Sd = DevExt->HdaMMIOBase + DevExt->StreamDescriptorOffset;
        ULONG k, j;

        for (k = 0; k < 5; k++) {
            for (j = 0; j < 20; j++) KsStallExecutionProcessor(1000);   /* ~20 ms */
            DPRINT("HDA: LPIB=%u STS=%02X CTL=%08X\n",
                   READ_REGISTER_ULONG((PULONG)(Sd + HDA_SD_LPIB)),
                   READ_REGISTER_UCHAR(Sd + HDA_SD_STS),
                   READ_REGISTER_ULONG((PULONG)(Sd + HDA_SD_CTL)));
                   
            DPRINT("HDA: LPIB=%u IRQ=%u INTSTS=%08X\n",
                   READ_REGISTER_ULONG((PULONG)(Sd + HDA_SD_LPIB)),
                   DevExt->IrqCount,
                   READ_REGISTER_ULONG((PULONG)(DevExt->HdaMMIOBase + HDA_REG_INTSTS)));
        }
    }

    RtlInitAnsiString(&DevExt->SymbolicLinkName, NULL);

    Status = ObioRegisterDeviceInterface(
        DevExt->PhysicalDeviceObject,
        &VEA_GUID_DEVINTERFACE_AUDIO,
        NULL,
        &DevExt->SymbolicLinkName
    );

    if (!VEA_SUCCESS(Status)) {
        DPRINT("HDA: Failed to register device interface! Status = 0x%08X\n", Status);
        // Kalau gagal registrasi interface, opsional mau dibatalkan atau lanjut
    } else {
        DPRINT("HDA: Device interface registered successfully: %s\n", 
               DevExt->SymbolicLinkName.Buffer);

        Status = ObioSetDeviceInterfaceState(&DevExt->SymbolicLinkName, TRUE);
        if (!VEA_SUCCESS(Status)) {
            DPRINT("HDA: Failed to enable device interface! Status = 0x%08X\n",
                   Status);
        } else {
            DPRINT("HDA: Device interface enabled\n");
        }
    }

    DevExt->IsStarted = TRUE;
    Oip->StatusInformation.Status = STATUS_SUCCESS;
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
HDAOipMjPnp(
    IN PVOID Object,
    IN POIP Oip,
    IN POIP_STACK OipStack
)
{
    switch (OipStack->MinorFunction)
    {
        case OIP_MN_START_DEVICE:
        {
            return HDAStartDevice(
                Object,
                Oip,
                OipStack
            );
        }
    }
    
    Oip->StatusInformation.Status = STATUS_NOT_SUPPORTED;
    return STATUS_NOT_SUPPORTED;
}

VEASTATUS
VEAPI
HDAOipMjWrite(
    IN PVOID Object,
    IN POIP Oip,
    IN POIP_STACK OipStack
)
{
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
HDAOipMjIOCTL(
    IN PVOID Object,
    IN POIP Oip,
    IN POIP_STACK OipStack
)
{
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
HDAOipMjDevCtl(
    IN PVOID Object,
    IN POIP Oip,
    IN POIP_STACK OipStack
)
{
    PDEVICE_OBJECT Fdo = (PDEVICE_OBJECT)Object;
    PHDA_DEVICE_EXTENSION DevExt = (PHDA_DEVICE_EXTENSION)Fdo->DeviceExtension;
    ULONG IoControlCode = OipStack->Parameters.DeviceControl.IoControlCode;
    VEASTATUS Status = STATUS_SUCCESS;
    ULONG Information = 0;

    if (!DevExt->IsStarted)
    {
        Oip->StatusInformation.Status = STATUS_DEVICE_NOT_READY;
        return STATUS_DEVICE_NOT_READY;
    }

    switch (IoControlCode)
    {
        case IOCTL_HDA_START_STREAM:
        {
            DevExt->UserModeActive = TRUE;
            if (!DevExt->StreamRunning)
            {
                Status = HDAStartOutputStream(DevExt);
            }
            break;
        }

        case IOCTL_HDA_STOP_STREAM:
        {
            if (DevExt->StreamRunning) {
                HDAStopOutputStream(DevExt);
            }
            break;
        }

        case IOCTL_HDA_GET_BUFFER:
        {
            if (OipStack->Parameters.DeviceControl.OutputBufferLength >= sizeof(PVOID))
            {
                PVOID *OutBuffer = (PVOID*)Oip->AssociatedOip.SystemBuffer;
                if (OutBuffer)
                {
                    *OutBuffer = DevExt->StreamBuffer;
                    Information = sizeof(PVOID);
                }
            }
            else 
            {
                Status = STATUS_BUFFER_TOO_SMALL;
            }
            break;
        }

        case IOCTL_HDA_REGISTER_EVENT:
        {
            HANDLE EventHandle;
            PKEVENT EventObject;

            if (OipStack->Parameters.DeviceControl.InputBufferLength >= sizeof(HANDLE))
            {
                EventHandle = *(PHANDLE)Oip->AssociatedOip.SystemBuffer;

                Status = ObReferenceObjectByHandle(
                    EventHandle,
                    EVENT_ALL_ACCESS,
                    NULL,
                    UserMode,
                    (PVOID*)&EventObject
                );

                if (VEA_SUCCESS(Status))
                {
                    if (DevExt->BufferedEvent)
                    {
                        ObDereferenceObject(DevExt->BufferedEvent);
                    }

                    DevExt->BufferedEvent = EventObject;
                    Information = 0;
                }
            }
            else
            {
                Status = STATUS_BUFFER_TOO_SMALL;
            }
        }

        default:
        {
            DPRINT("HDA: Unknown IOCTL code 0x%08X\n", IoControlCode);
            Status = STATUS_INVALID_DEVICE_REQUEST;
            break;
        }
    }

    Oip->StatusInformation.Status = Status;
    Oip->StatusInformation.Information = Information;
    return Status;
}

VEASTATUS
VEAPI
HDAOipMjPower(
    IN PVOID Object,
    IN POIP Oip,
    IN POIP_STACK OipStack
)
{
    return STATUS_SUCCESS;
}