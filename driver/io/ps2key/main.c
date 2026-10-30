#include <veaddk.h>

#define PS2KEY_RING_BUFFER_SIZE 256
#define PS2KEY_IO_TIMEOUT       100000

typedef struct _PS2KEY_DEVICE_EXTENSION {
    PVOID Pdo;              
    PVOID LowerDevice;      
    ULONG Flags;
    ULONG DataPort;
    ULONG CommandPort;
    ULONG IrqVector;
    PKINTERRUPT Interrupt;
    KSPIN_LOCK RingLock;
    UCHAR RingBuffer[PS2KEY_RING_BUFFER_SIZE];
    ULONG RingReadIndex;
    ULONG RingWriteIndex;
    ULONG RingCount;
} PS2KEY_DEVICE_EXTENSION, *PPS2KEY_DEVICE_EXTENSION;

static
UCHAR
PS2KeyReadPort(IN USHORT Port)
{
    UCHAR Value;

    __asm__ volatile("inb %1, %0" : "=a"(Value) : "Nd"(Port));
    return Value;
}

static
VOID
PS2KeyWritePort(IN USHORT Port, IN UCHAR Value)
{
    __asm__ volatile("outb %0, %1" : : "a"(Value), "Nd"(Port));
}

static
BOOLEAN
PS2KeyWaitForInputBufferEmpty(IN ULONG CommandPort)
{
    ULONG Retry;

    for (Retry = 0; Retry < PS2KEY_IO_TIMEOUT; Retry++)
    {
        if (!(PS2KeyReadPort((USHORT)CommandPort) & 0x02))
        {
            return TRUE;
        }
    }

    return FALSE;
}

static
BOOLEAN
PS2KeyWaitForOutputBufferFull(IN ULONG CommandPort)
{
    ULONG Retry;

    for (Retry = 0; Retry < PS2KEY_IO_TIMEOUT; Retry++)
    {
        if (PS2KeyReadPort((USHORT)CommandPort) & 0x01)
        {
            return TRUE;
        }
    }

    return FALSE;
}

static
BOOLEAN
PS2KeyInitializeController(
    IN ULONG DataPort,
    IN ULONG CommandPort
)
{
    ULONG Retry;
    UCHAR Status;

    for (Retry = 0; Retry < PS2KEY_IO_TIMEOUT; Retry++)
    {
        Status = PS2KeyReadPort((USHORT)CommandPort);
        if (!(Status & 0x01))
        {
            break;
        }

        (VOID)PS2KeyReadPort((USHORT)DataPort);
    }

    if (!PS2KeyWaitForInputBufferEmpty(CommandPort))
    {
        return FALSE;
    }

    PS2KeyWritePort((USHORT)CommandPort, 0xAE);
    if (!PS2KeyWaitForInputBufferEmpty(CommandPort))
    {
        return FALSE;
    }

    PS2KeyWritePort((USHORT)DataPort, 0xF4);
    if (!PS2KeyWaitForInputBufferEmpty(CommandPort) ||
        !PS2KeyWaitForOutputBufferFull(CommandPort))
    {
        return FALSE;
    }

    return (PS2KeyReadPort((USHORT)DataPort) == 0xFA);
}

static
VOID
PS2KeyDisableController(IN ULONG CommandPort)
{
    if (PS2KeyWaitForInputBufferEmpty(CommandPort))
    {
        PS2KeyWritePort((USHORT)CommandPort, 0xAD);
    }
}

VEASTATUS 
VEAPI
PS2KeyObjectInterruptHandler(PVOID Object, POIP Oip);

VEASTATUS
VEAPI
PS2KeyHandleMnAddDevice(
    IN PVOID DriverObject,
    IN POIP Oip,
    IN POIP_STACK Stack
)
{
    /* Legacy */
    return STATUS_SUCCESS;
}

BOOLEAN
VEAPI
PS2KeyIrqHandler(IN PKINTERRUPT Interrupt, IN PVOID ServiceContext)
{
    PPS2KEY_DEVICE_EXTENSION Extension;
    KIRQL OldIrql;
    UCHAR Status;
    UCHAR ScanCode;
    ULONG RingCountBefore;
    ULONG RingCountAfter;
    ULONG Esp;
    ULONG Ebp;
    BOOLEAN Enqueued;

    UNREFERENCED_PARAMETER(Interrupt);

    DPRINT("PS2KeyIrqHandler()\n");
    

    Extension = (PPS2KEY_DEVICE_EXTENSION)ServiceContext;
    if (!Extension || !Extension->DataPort || !Extension->CommandPort)
    {
        DPRINT("PS2 IRQ invalid context: interrupt=%p context=%p extension=%p\n",
               Interrupt, ServiceContext, Extension);
        return FALSE;
    }

    Status = PS2KeyReadPort((USHORT)Extension->CommandPort);
    if (!(Status & 0x01))
    {
        DPRINT("PS2 IRQ has no output byte; returning FALSE\n");
        return FALSE;
    }

    ScanCode = PS2KeyReadPort((USHORT)Extension->DataPort);

    KsAcquireSpinLock(&Extension->RingLock, &OldIrql);
    RingCountBefore = Extension->RingCount;
    Enqueued = RingCountBefore < PS2KEY_RING_BUFFER_SIZE;
    if (Enqueued)
    {
        Extension->RingBuffer[Extension->RingWriteIndex] = ScanCode;
        Extension->RingWriteIndex =
            (Extension->RingWriteIndex + 1) % PS2KEY_RING_BUFFER_SIZE;
        Extension->RingCount++;
    }
    RingCountAfter = Extension->RingCount;
    KsReleaseSpinLock(&Extension->RingLock, OldIrql);

    return TRUE;
}

VEASTATUS
VEAPI
PS2KeyStartDevice(
    IN PDEVICE_OBJECT Fdo,
    IN POIP Oip,
    IN POIP_STACK OipStack
)
{
    POBJECT_HEADER ObjectHeader;
    PPS2KEY_DEVICE_EXTENSION Extension;
    POIP RequiredOip = NULL;
    PVOID TargetObject;
    VEASTATUS Status;
    KIRQL InterruptIrql;
    ULONG IsrVector;
    POBIO_REQUIRED_RESOURCES_LIST_EX ListEx;
    POBIO_REQUIRED_RESOURCES_LIST CurrentList;
    ULONG Index;
    BOOLEAN HasDataPort = FALSE;
    BOOLEAN HasCommandPort = FALSE;
    BOOLEAN HasInterrupt = FALSE;

    if (!Fdo)
    {
        return STATUS_INVALID_PARAMETER;
    }

    Extension = (PPS2KEY_DEVICE_EXTENSION)Fdo->DeviceExtension;
    if (!Extension)
    {
        Status = STATUS_INVALID_PARAMETER;
        goto cleanup;
    }

    KsInitializeSpinlock(&Extension->RingLock);
    Extension->RingReadIndex = 0;
    Extension->RingWriteIndex = 0;
    Extension->RingCount = 0;
    Extension->Interrupt = NULL;

    ObjectHeader = OBJECT_TO_OBJECT_HEADER(Fdo);
    RequiredOip = ObioAllocateOip(ObjectHeader->StackDepth);
    if (!RequiredOip) {
        Status = STATUS_INSUFFICIENT_MEMORY;
        goto cleanup;
    }

    TargetObject = ObjectHeader->LowerAttachedObject;
    if (!TargetObject)
    {
        Status = STATUS_INVALID_DEVICE_REQUEST;
        goto cleanup;
    }

    POIP_STACK NextStack = ObioGetNextOipStackLocation(RequiredOip);
    NextStack->MajorFunction = OIP_MJ_PNP;
    NextStack->MinorFunction = OIP_MN_QUERY_RESOURCE_REQUIREMENTS;
    
    Status = ObioGoInterruptObject(
        TargetObject,
        RequiredOip
    );
    if (!VEA_SUCCESS(Status))
    {
        goto cleanup;
    }

    ListEx = (POBIO_REQUIRED_RESOURCES_LIST_EX)
        RequiredOip->StatusInformation.Information;
    if (!ListEx || !ListEx->AlternativeLists)
    {
        Status = STATUS_NOT_FOUND;
        goto cleanup;
    }

    CurrentList = &ListEx->List[0];
    for (Index = 0; Index < CurrentList->Count; Index++)
    {
        POBIO_REQUIRED_RESOURCES Desc = &CurrentList->Descriptors[Index];

        if (Desc->Type == PnpResourceTypePort)
        {
            ULONG PortStart = (ULONG)Desc->Port.MinimumAddress.QuadPart;
            ULONG PortEnd = (ULONG)Desc->Port.MaximumAddress.QuadPart;

            if (PortEnd == PortStart && Desc->Port.Length > 1)
            {
                PortEnd = PortStart + Desc->Port.Length - 1;
            }

            if (PortStart <= 0x60 && PortEnd >= 0x60)
            {
                Extension->DataPort = 0x60;
                HasDataPort = TRUE;
            }

            if (PortStart <= 0x64 && PortEnd >= 0x64)
            {
                Extension->CommandPort = 0x64;
                HasCommandPort = TRUE;
            }
        }
        else if (Desc->Type == PnpResourceTypeInterrupt && !HasInterrupt)
        {
            if (Desc->Interrupt.MinimumVector > Desc->Interrupt.MaximumVector)
            {
                Status = STATUS_INVALID_PARAMETER;
                goto cleanup;
            }

            Extension->IrqVector = Desc->Interrupt.MinimumVector;
            HasInterrupt = TRUE;
        }
    }

    if (!HasDataPort || !HasCommandPort || !HasInterrupt)
    {
        Status = STATUS_NOT_FOUND;
        goto cleanup;
    }

    if (!PS2KeyInitializeController(Extension->DataPort, Extension->CommandPort))
    {
        Status = STATUS_DEVICE_NOT_CONNECTED;
        PS2KeyDisableController(Extension->CommandPort);
        goto cleanup;
    }

    IsrVector = HctIrqVectorToIsrVector(Extension->IrqVector);
    if (IsrVector > 0xBF)
    {
        Status = STATUS_INVALID_PARAMETER;
        PS2KeyDisableController(Extension->CommandPort);
        goto cleanup;
    }

    InterruptIrql = HctConvertVectorToIrql((UCHAR)IsrVector);
    DPRINT("IRQL FOR PS2: %u\n", InterruptIrql);
    Status = ObioConnectInterrupt(
        &Extension->Interrupt,
        PS2KeyIrqHandler,
        Extension,
        NULL,
        Extension->IrqVector,
        InterruptIrql,
        InterruptIrql,
        Latched,
        0);
    if (!VEA_SUCCESS(Status))
    {
        Extension->Interrupt = NULL;
        PS2KeyDisableController(Extension->CommandPort);
        goto cleanup;
    }

    DPRINT("PS2KEY started: data=0x%x command=0x%x irq=%u\n",
           Extension->DataPort, Extension->CommandPort, Extension->IrqVector);
    Status = STATUS_SUCCESS;

cleanup:
    Fdo->Flags &= ~DO_DEVICE_INITIALIZING;

    if (RequiredOip)
    {
        ObioFreeOip(RequiredOip);
    }

    if (!VEA_SUCCESS(Status) && Extension && Extension->Interrupt)
    {
        ObioDisconnectInterrupt(Extension->Interrupt);
        Extension->Interrupt = NULL;
    }

    return Status;
}

VEASTATUS 
VEAPI
PS2KeyObjectInterruptHandler(PVOID Object, POIP Oip)
{
    POIP_STACK Stack = ObioGetCurrentOipStackLocation(Oip);

    switch (Stack->MajorFunction)
    {
        case OIP_MJ_READ:
        {
            PDEVICE_OBJECT DeviceObject;
            PPS2KEY_DEVICE_EXTENSION Extension;
            ULONG Length;
            ULONG BytesRead = 0;
            KIRQL OldIrql;

            if (!Object)
            {
                Oip->StatusInformation.Status = STATUS_INVALID_PARAMETER;
                Oip->StatusInformation.Information = 0;
                break;
            }

            DeviceObject = (PDEVICE_OBJECT)Object;
            Extension = (PPS2KEY_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
            if (!Extension)
            {
                Oip->StatusInformation.Status = STATUS_INVALID_PARAMETER;
                Oip->StatusInformation.Information = 0;
                break;
            }

            Length = Stack->Parameters.ReadWrite.Length;
            Oip->StatusInformation.Status = STATUS_SUCCESS;
            Oip->StatusInformation.Information = 0;

            if (Length && !Oip->AssociatedOip.SystemBuffer)
            {
                Oip->StatusInformation.Status = STATUS_INVALID_PARAMETER;
                break;
            }

            KsAcquireSpinLock(&Extension->RingLock, &OldIrql);
            while (BytesRead < Length && Extension->RingCount)
            {
                ((PUCHAR)Oip->AssociatedOip.SystemBuffer)[BytesRead++] =
                    Extension->RingBuffer[Extension->RingReadIndex];
                Extension->RingReadIndex =
                    (Extension->RingReadIndex + 1) % PS2KEY_RING_BUFFER_SIZE;
                Extension->RingCount--;
            }
            KsReleaseSpinLock(&Extension->RingLock, OldIrql);

            Oip->StatusInformation.Information = BytesRead;
            break;
        }

        case OIP_MJ_CREATE:
        {
            Oip->StatusInformation.Status = STATUS_SUCCESS;
            break;
        }

        case OIP_MJ_PNP:
        {
            PDEVICE_OBJECT DeviceObject;
            PPS2KEY_DEVICE_EXTENSION Extension;

            if (Stack->MinorFunction == OIP_MN_ADD_DEVICE)
            {
                return PS2KeyHandleMnAddDevice(Object, Oip, Stack);
            }
            else if (Stack->MinorFunction == OIP_MN_START_DEVICE)
            {
                return PS2KeyStartDevice(Object, Oip, Stack);
            }

            DeviceObject = (PDEVICE_OBJECT)Object;
            Extension = (PPS2KEY_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
            if (!Extension || !Extension->LowerDevice)
            {
                Oip->StatusInformation.Status = STATUS_DEVICE_NOT_READY;
                Oip->StatusInformation.Information = 0;
                return STATUS_DEVICE_NOT_READY;
            }

            ObioSkipCurrentOipStackLocation(Oip);
            return ObioGoInterruptObject(Extension->LowerDevice, Oip);
        }

        default:
        {
            Oip->StatusInformation.Status = STATUS_NOT_SUPPORTED;
            break;
        }
    }

    return Oip->StatusInformation.Status;
}

VEASTATUS
VEAPI
PS2KeyAddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT TargetDevice // Ini PDO dari Obio (ACPI/PNP0303/0)
)
{
    VEASTATUS Status;
    PDEVICE_OBJECT Fdo = NULL;
    PPS2KEY_DEVICE_EXTENSION Extension;
    PVOID TargetObject;

    if (!DriverObject || !TargetDevice)
    {
        return STATUS_INVALID_PARAMETER;
    }

    /* 1. Bikin Functional Device Object (FDO) buat ps2key */
    Status = ObioCreateNewDevice(
        DriverObject,
        sizeof(PS2KEY_DEVICE_EXTENSION),
        NULL, /* Nama device opsional, misal "\\Device\\KeyboardClass0" */
        FILE_DEVICE_KEYBOARD,
        &Fdo
    );

    if (!VEA_SUCCESS(Status)) return Status;

    Extension = (PPS2KEY_DEVICE_EXTENSION)Fdo->DeviceExtension;
    Extension->Pdo = TargetDevice;
    Extension->LowerDevice = ObioAttachObject(Fdo, TargetDevice);
    if (!Extension->LowerDevice)
    {
        ObioDeleteDevice(Fdo);
        return STATUS_UNSUCCESSFUL;
    }

    /* Attach Object Interrupt handler ke FDO */
    ObioAttachInterruptToThisObject(Fdo, PS2KeyObjectInterruptHandler);

    return STATUS_SUCCESS;
}

VEASTATUS 
VEAPI
DriverEntry(PVOID DriverObject)
{
    PDRIVER_OBJECT PDriverObject = (PDRIVER_OBJECT)DriverObject;
    DPRINT("PS2KEY Driver Loaded\n");

    PDriverObject->AddDeviceRoutine = PS2KeyAddDevice;
    
    return ObioAttachInterruptToThisObject(DriverObject, PS2KeyObjectInterruptHandler);
}