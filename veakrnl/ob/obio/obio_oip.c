#include <veakrnl.h>

POIP
VEAPI
ObioAllocateOip(
    ULONG StackCount
)
{
    ULONG TotalSize = sizeof(OIP) + (StackCount * sizeof(OIP_STACK));

    POIP Oip = (POIP)UlAllocatePoolWithTag(NonPagedPool, TotalSize, OB_OIP_TAG);
    if(!Oip) return NULL;

    RtlZeroMemory(Oip, TotalSize);

    Oip->Size = TotalSize;
    Oip->StackCount = StackCount;
    Oip->CurrentLocation = StackCount + 1;
    Oip->CurrentStackLocation = (POIP_STACK)(Oip + 1) + StackCount;

    return Oip;
}

VEASTATUS
VEAFAST
ObioCallObject(
    PVOID TargetObject,
    POIP Oip
)
{
    POBJECT_HEADER Header = OBJECT_TO_OBJECT_HEADER(TargetObject);

    Oip->CurrentLocation--;
    if (Oip->CurrentLocation <= 0) {
        DPRINT("ERROR: Stack TOO DEPTH\n");
        return STATUS_INVALID_PARAMETER; 
    }

    Oip->CurrentStackLocation--;

    POIP_STACK Stack = Oip->CurrentStackLocation;
    Stack->TargetObject = TargetObject;

    if (Header->Type->InterruptHandler != NULL) {
        return Header->Type->InterruptHandler(TargetObject, Oip);
    }

    Oip->StatusInformation.Status = STATUS_NOT_SUPPORTED;
    return STATUS_NOT_SUPPORTED;
}

VOID
VEAPI
ObioCompleteRequest(
    POIP Oip
)
{
    POIP_STACK Stack;
    VEASTATUS Status;

    if (!Oip) {
        KsBugCheckEx(OIP_STACK_CORRUPTION, 0, 0, 0, 0);
    }

    if (Oip->CurrentLocation > Oip->StackCount + 1) {
        KsBugCheckEx(OIP_MULTIPLE_COMPLETE_REQUESTS, (ULONG_PTR)Oip, Oip->CurrentLocation, 0, 0);
    }

    while(Oip->CurrentLocation <= Oip->StackCount)
    {
        Stack = Oip->CurrentStackLocation;

        ULONG_PTR MaxStackAddress = (ULONG_PTR)Oip + Oip->Size;
        if ((ULONG_PTR)Stack >= MaxStackAddress) {
            KsBugCheckEx(OIP_STACK_CORRUPTION, (ULONG_PTR)Oip, (ULONG_PTR)Stack, 0, 0);
        }

        if(Stack->CompletionRoutine != NULL)
        {
            POIP_COMPLETION_ROUTINE Routine = Stack->CompletionRoutine;
            Status = Routine(Stack->TargetObject, Oip, Stack->Context);

            if (Status == STATUS_MORE_PROCESSING_REQUIRED) {
                // STOP UNWINDING! Biarkan OIP tertahan di layer ini.
                // Driver tersebut bertanggung jawab memanggil ObioCompleteRequest 
                // lagi nanti kalau tugas lanjutannya udah beneran beres.
                return; 
            }
        }

        Oip->CurrentLocation++;
        Oip->CurrentStackLocation++;
    }

    if (Oip->Flags & OIP_DEALLOCATE_ON_COMPLETION) {
        if ((Oip->Flags & OIP_DEALLOCATE_BUFFER) &&
            Oip->AssociatedOip.SystemBuffer != NULL) {
            UlFreePoolWithTag(Oip->AssociatedOip.SystemBuffer, Oip->SystemTag);
            Oip->AssociatedOip.SystemBuffer = NULL;
        }

        if (Oip->MdlAddress != NULL) {
            ObioFreeMdl(Oip->MdlAddress);
            Oip->MdlAddress = NULL;
        }
    }

    if (Oip->UserInfoBlock != NULL) {
        *(Oip->UserInfoBlock) = Oip->StatusInformation;
    }

    if (Oip->UserEvent != NULL) {
        PKEVENT Event = (PKEVENT)Oip->UserEvent;
        KsSetEvent(Event, FALSE);
    }

    if (Oip->Flags & OIP_DEALLOCATE_ON_COMPLETION) {
        DPRINT("[OBIO] Oip will be deallocated on Completion\n");
        ObioFreeOip(Oip);
    }
}

VEASTATUS
VEAPI
ObioSendWriteRequest(
    PDEVICE_OBJECT DeviceObject,
    PVOID DataBuffer,
    ULONG DataLength
)
{
    if (!DeviceObject || !DataBuffer || DataLength == 0) 
    {
        return STATUS_INVALID_PARAMETER;
    }

    POIP Oip = ObioAllocateOip(1);
    if(!Oip)
    {
        return STATUS_INSUFFICIENT_MEMORY;
    }

    POIP_STACK Stack = ObioGetNextOipStackLocation(Oip);

    Oip->AssociatedOip.SystemBuffer = UlAllocatePoolWithTag(NonPagedPool, DataLength, 'OIPP');
    if(!Oip->AssociatedOip.SystemBuffer)
    {
        //ObioFreeOip(Oip);
        return STATUS_INSUFFICIENT_MEMORY;
    }
    Oip->SystemTag = 'OIPP';
    RtlCopyMemory(Oip->AssociatedOip.SystemBuffer, DataBuffer, DataLength);

    Stack->MajorFunction = OIP_MJ_WRITE; // Perintah Write
    Stack->Parameters.ReadWrite.Length = DataLength;
    Stack->TargetObject = DeviceObject;

    BOOLEAN Handled = ObioGoInterruptObject(DeviceObject, Oip);
    VEASTATUS FinalStatus = Oip->StatusInformation.Information;

    UlFreePoolWithTag(Oip->AssociatedOip.SystemBuffer, Oip->SystemTag);
    ObioFreeOip(Oip);

    if (!Handled) {
        return STATUS_UNSUCCESSFULL;
    }

    return FinalStatus;
}

VEASTATUS
VEAPI
ObioSendReadRequest(
    PDEVICE_OBJECT DeviceObject,
    PVOID DataBuffer,       // Buffer tujuan (di-alokasi oleh pemanggil)
    ULONG DataLength,       // Berapa byte yang mau dibaca
    PULONG BytesRead        // Output: Berapa byte yang aktual berhasil dibaca
)
{
    if (!DeviceObject || !DataBuffer || DataLength == 0 || !BytesRead) {
        return STATUS_INVALID_PARAMETER;
    }

    *BytesRead = 0; // Inisialisasi awal

    // 1. Alokasi OIP dengan 1 layer stack
    POIP Oip = ObioAllocateOip(1);
    if (!Oip) {
        return STATUS_INSUFFICIENT_MEMORY;
    }

    // 2. Ambil next stack
    POIP_STACK Stack = ObioGetNextOipStackLocation(Oip);

    // 3. Setup SystemBuffer untuk operasi Read (Buffered I/O)
    Oip->AssociatedOip.SystemBuffer = UlAllocatePoolWithTag(NonPagedPool, DataLength, 'OIPP');
    if (!Oip->AssociatedOip.SystemBuffer) {
        ObioFreeOip(Oip);
        return STATUS_INSUFFICIENT_MEMORY;
    }
    Oip->SystemTag = 'OIPP';

    // (Tidak perlu mengkopi DataBuffer ke SystemBuffer karena ini operasi READ)

    // 4. Setup parameter Stack
    Stack->MajorFunction = OIP_MJ_READ;
    Stack->Parameters.ReadWrite.Length = DataLength;
    Stack->TargetObject = DeviceObject;

    // 5. Eksekusi
    BOOLEAN Handled = ObioGoInterruptObject(DeviceObject, Oip);
    VEASTATUS FinalStatus = Oip->StatusInformation.Status;

    if (Handled && VEA_SUCCESS(FinalStatus)) {
        // Jika sukses, copy data dari Kernel Buffer ke User/Caller Buffer
        ULONG ReadAmount = (ULONG)Oip->StatusInformation.Information;
        if (ReadAmount > DataLength) {
            ReadAmount = DataLength; // Safety check
        }
        
        RtlCopyMemory(DataBuffer, Oip->AssociatedOip.SystemBuffer, ReadAmount);
        *BytesRead = ReadAmount;
    }

    UlFreePoolWithTag(Oip->AssociatedOip.SystemBuffer, Oip->SystemTag);
    ObioFreeOip(Oip);

    if (!Handled) {
        return STATUS_UNSUCCESSFULL;
    }

    return FinalStatus;
}

POIP
VEAPI
ObioBuildAsynchronousFsdRequest(
    ULONG MajorFunction,
    PVOID Object,
    PVOID Buffer,
    ULONG Length,
    PLARGE_INTEGER StartingOffset,
    POIP_INFORMATION StatusInformation
)
{
    POBJECT_HEADER Header = OBJECT_TO_OBJECT_HEADER(Object);
    CHAR StackSize = Header->StackDepth;

    POIP Oip = ObioAllocateOip(StackSize);
    if (!Oip) {
        return NULL;
    }

    Oip->UserEvent = NULL;
    Oip->UserInfoBlock = StatusInformation;
    Oip->Flags |= OIP_DEALLOCATE_ON_COMPLETION;

    POIP_STACK Stack = ObioGetNextOipStackLocation(Oip);
    Stack->MajorFunction = (UCHAR)MajorFunction;
    Stack->TargetObject = Object;
    Stack->Parameters.ReadWrite.Length = Length;

    if (StartingOffset) {
        Stack->Parameters.ReadWrite.Offset = *StartingOffset;
    }

    if ((MajorFunction != OIP_MJ_FLUSH_BUFFERS) &&
        (MajorFunction != OIP_MJ_SHUTDOWN) &&
        (MajorFunction != OIP_MJ_PNP) &&
        (MajorFunction != OIP_MJ_POWER))
    {
        if (Length != 0 && Buffer == NULL) {
            ObioFreeOip(Oip);
            return NULL;
        }

        Oip->UserBuffer = Buffer;
        PDEVICE_OBJECT DeviceObject = (PDEVICE_OBJECT)Object;
        if (DeviceObject->Flags & DO_BUFFERED_IO) {
            if (Length != 0) {
                Oip->AssociatedOip.SystemBuffer =
                    UlAllocatePoolZero(NonPagedPool, Length, TAG_OB_OIP);
                if (Oip->AssociatedOip.SystemBuffer == NULL) {
                    ObioFreeOip(Oip);
                    return NULL;
                }

                Oip->SystemTag = TAG_OB_OIP;
                Oip->Flags |= OIP_BUFFERED_IO | OIP_DEALLOCATE_BUFFER;

                if (MajorFunction == OIP_MJ_WRITE) {
                    RtlCopyMemory(Oip->AssociatedOip.SystemBuffer, Buffer, Length);
                } else {
                    Oip->Flags |= OIP_INPUT_OPERATION;
                }
            }
        } else if (DeviceObject->Flags & DO_DIRECT_IO) {
            if (Length != 0) {
                Oip->MdlAddress = ObioAllocateMdl(
                    Buffer,
                    Length,
                    FALSE,
                    FALSE,
                    Oip
                );
                if (Oip->MdlAddress == NULL) {
                    ObioFreeOip(Oip);
                    return NULL;
                }

                if (MajorFunction == OIP_MJ_READ) {
                    Oip->MdlAddress->MdlFlags |= MDL_WRITE_OPERATION;
                }
            }
        }
        else {
            if (Length != 0) {
                Oip->AssociatedOip.SystemBuffer =
                    UlAllocatePoolZero(NonPagedPool, Length, TAG_OB_OIP);
                if (Oip->AssociatedOip.SystemBuffer == NULL) {
                    ObioFreeOip(Oip);
                    return NULL;
                }

                Oip->SystemTag = TAG_OB_OIP;
                Oip->Flags |= OIP_BUFFERED_IO | OIP_DEALLOCATE_BUFFER;

                if (MajorFunction == OIP_MJ_WRITE) {
                    RtlCopyMemory(Oip->AssociatedOip.SystemBuffer, Buffer, Length);
                } else {
                    Oip->Flags |= OIP_INPUT_OPERATION;
                }
            }
        }

        if (MajorFunction == OIP_MJ_READ ||
            MajorFunction == OIP_MJ_WRITE)
        {
            Stack->Parameters.ReadWrite.Length = Length;
            if (StartingOffset) {
                Stack->Parameters.ReadWrite.Offset = *StartingOffset;
            }
        }
    }

    return Oip;
}

POIP
VEAPI
ObioBuildSynchronusFsdRequest(
    ULONG MajorFunction,
    PVOID Object,
    PVOID Buffer,
    ULONG Length,
    PLARGE_INTEGER StartingOffset,
    PKEVENT Event,
    POIP_INFORMATION StatusInformation
)
{
    POIP Oip = ObioBuildAsynchronousFsdRequest(
        MajorFunction,
        Object,
        Buffer,
        Length,
        StartingOffset,
        StatusInformation
    );

    if (Oip) {
        Oip->UserEvent = Event;
    }

    return Oip;
}

POIP
VEAPI
ObioBuildDeviceIoControlRequest(
    ULONG IoControlCode,
    PDEVICE_OBJECT DeviceObject,
    PVOID InputBuffer,
    ULONG InputBufferLength,
    PVOID OutputBuffer,
    ULONG OutputBufferLength,
    BOOLEAN InternalDeviceIoControl,
    PKEVENT Event,
    POIP_INFORMATION StatusInformation
)
{
    POBJECT_HEADER Header;
    ULONG StackCount;
    ULONG Method;
    ULONG BufferLength;
    POIP Oip;
    POIP_STACK Stack;

    if (DeviceObject == NULL ||
        (InputBufferLength != 0 && InputBuffer == NULL) ||
        (OutputBufferLength != 0 && OutputBuffer == NULL))
    {
        return NULL;
    }

    Header = OBJECT_TO_OBJECT_HEADER(DeviceObject);
    StackCount = Header->StackDepth;
    if (StackCount == 0)
    {
        return NULL;
    }

    Oip = ObioAllocateOip(StackCount);
    if (Oip == NULL)
    {
        return NULL;
    }

    Oip->StatusInformation.Status = STATUS_PENDING;
    Oip->UserEvent = Event;
    Oip->UserInfoBlock = StatusInformation;
    Oip->UserBuffer = OutputBuffer;
    Oip->Flags |= OIP_DEALLOCATE_ON_COMPLETION;

    Stack = ObioGetNextOipStackLocation(Oip);
    Stack->MajorFunction = InternalDeviceIoControl
        ? OIP_MJ_INTERNAL_DEVICE_CTL
        : OIP_MJ_DEVICE_CONTROL;
    Stack->TargetObject = DeviceObject;
    Stack->Parameters.DeviceControl.IoControlCode = IoControlCode;
    Stack->Parameters.DeviceControl.InputBufferLength = InputBufferLength;
    Stack->Parameters.DeviceControl.OutputBufferLength = OutputBufferLength;

    Method = IoControlCode & 0x3;
    switch (Method)
    {
        case OIP_METHOD_BUFFERED:
            BufferLength = InputBufferLength > OutputBufferLength
                ? InputBufferLength
                : OutputBufferLength;
            if (BufferLength != 0)
            {
                Oip->AssociatedOip.SystemBuffer =
                    UlAllocatePoolZero(NonPagedPool, BufferLength, OB_OIP_TAG);
                if (Oip->AssociatedOip.SystemBuffer == NULL)
                {
                    goto Failure;
                }

                Oip->SystemTag = OB_OIP_TAG;
                Oip->Flags |= OIP_BUFFERED_IO | OIP_DEALLOCATE_BUFFER;
                if (InputBufferLength != 0)
                {
                    RtlCopyMemory(Oip->AssociatedOip.SystemBuffer,
                                  InputBuffer,
                                  InputBufferLength);
                }
                if (OutputBuffer != NULL)
                {
                    Oip->Flags |= OIP_INPUT_OPERATION;
                }
            }
            break;

        case OIP_METHOD_IN_DIRECT:
        case OIP_METHOD_OUT_DIRECT:
            if (InputBufferLength != 0)
            {
                Oip->AssociatedOip.SystemBuffer =
                    UlAllocatePoolZero(NonPagedPool,
                                       InputBufferLength,
                                       OB_OIP_TAG);
                if (Oip->AssociatedOip.SystemBuffer == NULL)
                {
                    goto Failure;
                }

                Oip->SystemTag = OB_OIP_TAG;
                Oip->Flags |= OIP_BUFFERED_IO | OIP_DEALLOCATE_BUFFER;
                RtlCopyMemory(Oip->AssociatedOip.SystemBuffer,
                              InputBuffer,
                              InputBufferLength);
            }

            if (OutputBufferLength != 0)
            {
                Oip->MdlAddress = ObioAllocateMdl(
                    OutputBuffer,
                    OutputBufferLength,
                    FALSE,
                    FALSE,
                    Oip
                );
                if (Oip->MdlAddress == NULL)
                {
                    goto Failure;
                }
                if (Method == OIP_METHOD_OUT_DIRECT)
                {
                    Oip->MdlAddress->MdlFlags |= MDL_WRITE_OPERATION;
                }
            }
            break;

        case OIP_METHOD_NEITHER:
            Stack->Parameters.DeviceControl.Type3InputBuffer = InputBuffer;
            break;
    }

    return Oip;

Failure:
    if ((Oip->Flags & OIP_DEALLOCATE_BUFFER) &&
        Oip->AssociatedOip.SystemBuffer != NULL)
    {
        UlFreePoolWithTag(Oip->AssociatedOip.SystemBuffer, Oip->SystemTag);
        Oip->AssociatedOip.SystemBuffer = NULL;
    }
    Oip->Flags &= ~(OIP_DEALLOCATE_BUFFER | OIP_DEALLOCATE_ON_COMPLETION);
    ObioFreeOip(Oip);
    return NULL;
}

VOID
VEAPI
ObioFreeOip(
    POIP Oip
)
{
    if (!Oip) return;

    if (Oip->MdlAddress != NULL) {
        ObioFreeMdl(Oip->MdlAddress);
        Oip->MdlAddress = NULL;
    }

    if ((Oip->Flags & OIP_DEALLOCATE_BUFFER) &&
        Oip->AssociatedOip.SystemBuffer != NULL)
    {
        UlFreePoolWithTag(Oip->AssociatedOip.SystemBuffer, Oip->SystemTag);
        Oip->AssociatedOip.SystemBuffer = NULL;
    }

    DPRINT("[OBIO] Calling ObioFreeOip for Oip at %p\n", Oip);
    UlFreePoolWithTag(Oip, OB_OIP_TAG); 
}

VEASTATUS
VEAPI
ObioSynchronusCallByStack
(
    IN PDEVICE_OBJECT DeviceObject,
    IN POIP_STACK OipStack,
    IN OUT PVOID *Information
)
{
    POIP Oip;
    POBJECT_HEADER ObjectHeader;
    OIP_INFORMATION LocalStatusInformation;
    POIP_STACK LocalOipStack;
    KEVENT Event;
    PDEVICE_OBJECT TargetedObject;
    VEASTATUS Status;

    /* Get DeviceReference */
    ObReferenceObject(DeviceObject);

    /* Get the most top object */
    TargetedObject = ObioGetTopDeviceObject(DeviceObject);
    ObjectHeader = OBJECT_TO_OBJECT_HEADER(TargetedObject);

    /* Allocate Oip */
    Oip = ObioAllocateOip(ObjectHeader->StackDepth);
    if (!Oip)
    {
        ObDereferenceObject(DeviceObject);
        return STATUS_INSUFFICIENT_MEMORY;
    }

    /* Default parameter */
    Oip->StatusInformation.Status = STATUS_NOT_SUPPORTED;
    Oip->StatusInformation.Information = 0;
    LocalStatusInformation.Status = STATUS_NOT_SUPPORTED;
    LocalStatusInformation.Information = 0;

    /* Initialize event */
    KsInitializeEvent(&Event, SynchronizationEvent, FALSE);

    /* Setup */
    Oip->UserInfoBlock = &LocalStatusInformation;
    Oip->UserEvent = &Event;

    /* Just deep copy this parameter */
    LocalOipStack = ObioGetNextOipStackLocation(Oip);
    RtlCopyMemory(LocalOipStack, OipStack, sizeof(OIP_STACK));

    Status = ObioGoInterruptObject((PVOID)TargetedObject, Oip);
    
    if (Status == STATUS_PENDING)
    {
        KsWaitForSingleObject(&Event);
        Status = LocalStatusInformation.Status;
    }
    else
    {
        /* Synchronous handlers return their result directly. */
        LocalStatusInformation = Oip->StatusInformation;
    }

    /* Get out the reference */
    ObDereferenceObject(DeviceObject);

    /* Pass out the information */
    *Information = (PVOID)LocalStatusInformation.Information;
    return Status;
}

/* Pnp Specific */
VEASTATUS
VEAPI
ObioCallSpecificPnpOip
(
    IN PDEVICE_OBJECT DeviceObject,
    IN UCHAR MinorFunction,
    IN POIP_STACK Stack OPTIONAL,
    IN OUT POIP_INFORMATION Information
)
{
    OIP_STACK LocalStack;

    /* Zero it and fill out data */
    RtlZeroMemory(&LocalStack, sizeof(OIP_STACK));
    LocalStack.MajorFunction = OIP_MJ_PNP;
    LocalStack.MinorFunction = MinorFunction;
    if (Stack)
    {
        RtlCopyMemory(
            &LocalStack.Parameters,
            &Stack->Parameters,
            sizeof(Stack->Parameters)
        );
    }

    Information->Status = ObioSynchronusCallByStack(
        DeviceObject,
        &LocalStack,
        (PVOID)&Information->Information
    );

    return Information->Status;
}

/* Specific Obio Helper OIP */

VEASTATUS
VEAPI
ObioQueryDeviceRelations(
    IN PDEVICE_NODE DeviceNode,
    IN DEVICE_RELATION_TYPE RelationType)
{
    OIP_STACK Stack;
    VEASTATUS Status;
    PDEVICE_OBJECT TopDeviceObject;

    /* Zero Mmeory */
    RtlZeroMemory(&Stack, sizeof(OIP_STACK));

    Stack.MajorFunction = OIP_MJ_PNP;
    Stack.MinorFunction = OIP_MN_QUERY_DEVICE_RELATIONS;
    Stack.Parameters.QueryDeviceRelation.Type = RelationType;

    TopDeviceObject = ObioGetTopDeviceObject(DeviceNode->PhysicalDeviceObject);
    DeviceNode->PendingDeviceRelations = NULL;

    Status = ObioSynchronusCallByStack(
        (PVOID)TopDeviceObject, // Target ditujukan ke FDO
        &Stack,
        (PVOID)&DeviceNode->PendingDeviceRelations
    );

    DeviceNode->LastStateCompletionStatus = Status;
    
    return Status;
}

VEASTATUS
VEAPI
ObioAddDeviceByDeviceObject(
    IN PDEVICE_NODE DeviceNode OPTIONAL,
    IN PDEVICE_OBJECT DeviceObject,
    IN PDRIVER_OBJECT TargetDriver,
    OUT POIP_INFORMATION Information
)
{
    VEASTATUS Status;
    PDEVICE_OBJECT Pdo = NULL;

    if (DeviceNode != NULL)
    {
        Pdo = (PDEVICE_OBJECT)DeviceNode->PhysicalDeviceObject;
    }
    else if (DeviceObject != NULL)
    {
        Pdo = DeviceObject;
    }
    else
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (!TargetDriver || !TargetDriver->AddDeviceRoutine)
    {
        return STATUS_NOT_SUPPORTED;
    }

    Status = TargetDriver->AddDeviceRoutine(TargetDriver, Pdo);

    if (DeviceNode != NULL)
    {
        DeviceNode->LastStateCompletionStatus = Status;
    }

    if (Information)
    {
        Information->Status = Status;
        Information->Information = 0;
    }

    return Status;
}

VEASTATUS
VEAPI
ObioStartDevice(IN PDEVICE_NODE DeviceNode)
{
    VEASTATUS Status;
    OIP_STACK Stack;
    OIP_INFORMATION StatusInformation;

    RtlZeroMemory(&StatusInformation, sizeof(OIP_INFORMATION));
    RtlZeroMemory(&Stack, sizeof(OIP_STACK));

    Stack.MajorFunction = OIP_MJ_PNP;
    Stack.MinorFunction = OIP_MN_START_DEVICE;
    
    Status = ObioSynchronusCallByStack(DeviceNode->PhysicalDeviceObject, &Stack, (PVOID)&StatusInformation.Information);
    DeviceNode->LastStateCompletionStatus = StatusInformation.Status;
    return Status;
}

PDEVICE_OBJECT
VEAPI
ObioGetTopDeviceObject(
    IN PDEVICE_OBJECT DeviceObject
)
{
    PVOID Object = (PVOID)DeviceObject;

    // Assume is 
    return (PDEVICE_OBJECT)ObGetTopAttachedObject(Object);
}

VOID
ObioSkipCurrentOipStackLocation(
    IN OUT POIP Oip
)
{
    Oip->CurrentLocation++;
    Oip->CurrentStackLocation++;
}

VOID
ObioCopyCurrentOipStackLocationToNext(
    IN OUT POIP Oip
)
{
    POIP_STACK OipSp;
    POIP_STACK OipNext;

    OipSp = ObioGetCurrentOipStackLocation(Oip);
    OipNext = ObioGetNextOipStackLocation(Oip);

    RtlCopyMemory(OipNext, OipSp, sizeof(OIP_STACK));

    OipNext->Control = 0;
}