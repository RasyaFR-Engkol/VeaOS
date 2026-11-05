/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : obio_file.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : File Object handling
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 14-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file obio_file.c
 * --------------------------------------------------------------------- */

BOOLEAN
ObioIsOpenPacketValid(IN POPEN_PACKET OpenPacket)
{
    if (!(OpenPacket) ||
        (OpenPacket->Type != IO_TYPE_OPEN_PACKET) ||
        (OpenPacket->Size != sizeof(OPEN_PACKET)))
    {
        return FALSE;
    }

    return TRUE;
}

VEASTATUS
VEAPI
ObioParseFile(
    IN PVOID ParseObject,
    IN POB_LOOKUP_CONTEXT LookupContext,
    IN PVOID Context,
    OUT PVOID* ResolvedObject
)
{
    PVOID DeviceObject;
    POPEN_PACKET OpenPacket = (POPEN_PACKET)Context;

    /* Validate first */
    if (!ObioIsOpenPacketValid(OpenPacket)) return STATUS_OBJECT_TYPE_MISMATCH;

    DeviceObject = ObioGetRelatedDeviceObject(ParseObject);
    OpenPacket->RelatedFileObject = ParseObject;

    return ObioParseDevice(
        DeviceObject,
        LookupContext,
        OpenPacket,
        ResolvedObject
    );
}

VEASTATUS
VEAPI
ObioCreateFile(
    OUT PHANDLE FileHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes,
    OUT POIP_INFORMATION IoStatusBlock,
    IN PLARGE_INTEGER AllocationSize OPTIONAL,
    IN ULONG FileAttributes,
    IN ULONG ShareAccess,
    IN ULONG Disposition,
    IN ULONG CreateOptions,
    IN PVOID EaBuffer OPTIONAL,     // <--- TAMBAHAN EA BUFFER
    IN ULONG EaLength              // <--- TAMBAHAN EA LENGTH
)
{
    OPEN_PACKET OpenPacket;
    HANDLE LocalHandle = NULL;
    VEASTATUS Status;

    if (!FileHandle || !ObjectAttributes)
    {
        DPRINT("ObioCreateFile: INVALID PARAMETER: ObjectAttributes %p FileHandle %x\n", ObjectAttributes, FileHandle);
        return STATUS_INVALID_PARAMETER;
    }

    // Safetynet: Kalau ada EaLength tapi EaBuffer NULL (atau sebaliknya)
    if ((EaBuffer && EaLength == 0) || (!EaBuffer && EaLength > 0))
    {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(&OpenPacket, sizeof(OPEN_PACKET));
    OpenPacket.Type = IO_TYPE_OPEN_PACKET;
    OpenPacket.Size = sizeof(OPEN_PACKET);
    OpenPacket.CreateOptions = CreateOptions;
    OpenPacket.ShareAccess = (USHORT)ShareAccess;
    OpenPacket.Disposition = Disposition;
    OpenPacket.FileAttributes = (USHORT)FileAttributes;

    if (AllocationSize)
    {
        OpenPacket.AllocationSize = *AllocationSize;
    }

    OpenPacket.EaBuffer = EaBuffer;
    OpenPacket.EaLength = EaLength;

    Status = ObOpenObjectByName(
        ObjectAttributes,
        ObpFileObjectType,
        KernelMode,
        DesiredAccess,
        &OpenPacket,
        &LocalHandle
    );

    if (VEA_SUCCESS(Status))
    {
        if (VEA_SUCCESS(OpenPacket.FinalStatus))
        {
            *FileHandle = LocalHandle;
            if (IoStatusBlock)
            {
                IoStatusBlock->Status = OpenPacket.FinalStatus;
                IoStatusBlock->Information = OpenPacket.Information;
            }
            return OpenPacket.FinalStatus;
        }
        else
        {
            VeaClose(LocalHandle);
            return OpenPacket.FinalStatus;
        }
    }

    return Status;
}

VEASTATUS
VEAPI
ObioPerformSychronusRequest(IN PDEVICE_OBJECT DeviceObject,
                             IN POIP Oip,
                             IN PFILE_OBJECT FileObject,
                             IN BOOLEAN Deferred,
                             IN KPROCESSOR_MODE PreviousMode,
                             IN BOOLEAN SynchIo,
                             IN IOP_TRANSFER_TYPE TransferType)
{
    VEASTATUS Status;
    PKNORMAL_ROUTINE NormalRoutine;
    PVOID NormalContext = NULL;
    KIRQL OldIrql;

    Status = ObioCallObject(DeviceObject, Oip);

    if (Deferred)
    {
        if (Status != STATUS_PENDING)
        {
            NormalContext = NULL;
            NormalRoutine = NULL;
            OldIrql = KeRaiseIrql(APC_LEVEL);
            ObioCompleteRequest(Oip); // Todo: More ObioCompleteRequest2
            KeLowerIrql(OldIrql);
        }
    }

    if (SynchIo)
    {
        if (Status == STATUS_PENDING)
        {
            Status = KsWaitForSingleObject(&FileObject->Event);

            if ((Status == STATUS_ALERTED) || (Status == STATUS_USER_APC))
            {
                // ObioAbortInterruptIrp();
            }

            Status = FileObject->FinalStatus;
        }

        if (FileObject->Waiters)
        {
            KsSetEvent(&FileObject->Lock, FALSE);
        }
    }

    return Status;
}

VEASTATUS
VEAPI
VeaOpenFile(
    OUT PHANDLE FileHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes,
    OUT POIP_INFORMATION IoStatusBlock,
    IN ULONG ShareAccess,
    IN ULONG OpenOptions
)
{
    return ObioCreateFile(
        FileHandle,
        DesiredAccess,
        ObjectAttributes,
        IoStatusBlock,
        NULL,
        FILE_ATTRIBUTE_NORMAL, 
        ShareAccess,
        FILE_OPEN,
        OpenOptions,
        NULL, // No EA Buffer
        0     // No EA Length
    );
}

VEASTATUS
VEAPI
VeaCreateFile(
    OUT PHANDLE FileHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes,
    OUT POIP_INFORMATION IoStatusBlock,
    IN PLARGE_INTEGER AllocationSize OPTIONAL,
    IN ULONG FileAttributes,
    IN ULONG ShareAccess,
    IN ULONG CreateDisposition,
    IN ULONG CreateOptions,
    IN PVOID EaBuffer OPTIONAL,
    IN ULONG EaLength
)
{
    return ObioCreateFile(
        FileHandle,
        DesiredAccess,
        ObjectAttributes,
        IoStatusBlock,
        AllocationSize,
        FileAttributes,
        ShareAccess,
        CreateDisposition,
        CreateOptions,
        EaBuffer,
        EaLength
    );
}

VEASTATUS
VEAPI
VeaReadFile(
    IN HANDLE FileHandle,
    IN HANDLE Event OPTIONAL,
    IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
    IN PVOID ApcContext OPTIONAL,
    OUT POIP_INFORMATION IoStatusBlock,
    OUT PVOID Buffer,
    IN ULONG Length,
    IN PLARGE_INTEGER ByteOffset OPTIONAL,
    IN PULONG Key OPTIONAL)
{
    PFILE_OBJECT FileObject = NULL;
    PDEVICE_OBJECT DeviceObject = NULL;
    PKEVENT EventObject = NULL;
    POIP Oip = NULL;
    POIP_STACK StackPtr = NULL;
    VEASTATUS Status;
    KPROCESSOR_MODE AccessMode = KeGetPreviousMode();
    LARGE_INTEGER LocalByteOffset;

    if (!IoStatusBlock || !Buffer)
    {
        return STATUS_INVALID_PARAMETER;
    }

    Status = ObReferenceObjectByHandle(
        FileHandle,
        FILE_READ_DATA,
        ObpFileObjectType,
        AccessMode,
        (PVOID*)&FileObject
    );

    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    if (Event)
    {
        Status = ObReferenceObjectByHandle(
            Event,
            EVENT_MODIFY_STATE,
            UlEventObjectType,
            AccessMode,
            (PVOID*)&EventObject
        );

        if (!VEA_SUCCESS(Status))
        {
            ObDereferenceObject(FileObject);
            return Status;
        }

        KsClearEvent(EventObject);
    }

    if (ByteOffset)
    {
        LocalByteOffset = *ByteOffset;
    }
    else
    {
        // Jika ByteOffset NULL, gunakan offset posisi file saat ini (Synchronous)
        LocalByteOffset = FileObject->CurrentByteOffset;
    }

    DeviceObject = ObioGetRelatedDeviceObject(FileObject);

    Oip = ObioBuildSynchronusFsdRequest(
        OIP_MJ_READ,
        DeviceObject,
        Buffer,
        Length,
        &LocalByteOffset,
        EventObject ? EventObject : &FileObject->Event,
        IoStatusBlock
    );

    if (!Oip)
    {
        if (EventObject) ObDereferenceObject(EventObject);
        ObDereferenceObject(FileObject);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    /* Setup Stack Location IRP untuk Driver */
    StackPtr = ObioGetNextOipStackLocation(Oip);
    StackPtr->FileObject = FileObject;
    if (Key) StackPtr->Parameters.ReadWrite.Key = *Key;

    Status = ObioCallObject(DeviceObject, Oip);

    if (Status == STATUS_PENDING)
    {
        if (FileObject->Flags & FO_SYNCHRONOUS_IO)
        {
            KsWaitForSingleObject(
                &FileObject->Event
            );
            Status = IoStatusBlock->Status;
        }
    }

    if (EventObject)
    {
        ObDereferenceObject(EventObject);
    }

    ObDereferenceObject(FileObject);

    return Status;
}

VEASTATUS
VEAPI
VeaFsControlFile(
    IN HANDLE DeviceHandle,
    IN HANDLE Event OPTIONAL,
    IN PIO_APC_ROUTINE UserApcRoutine OPTIONAL,
    IN PVOID UserApcContext OPTIONAL,
    OUT POIP_INFORMATION IoStatusBlock,
    IN ULONG IoControlCode,
    IN PVOID InputBuffer,
    IN ULONG InputBufferLength OPTIONAL,
    OUT PVOID OutputBuffer,
    IN ULONG OutputBufferLength OPTIONAL,
    IN BOOLEAN IsDevIoCtl)
//
// Routine Description:
// TODO
// 
// Parameter:
// TODO
// 
{
    VEASTATUS Status;
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();
    PFILE_OBJECT FileObject;
    ULONG DesiredAccess;
    PDEVICE_OBJECT DeviceObject;
    PKEVENT IoEvent = NULL;
    POIP Oip;
    POIP_STACK StackLocation;
    ULONG BufferLength;
    ULONG AccessType;
    BOOLEAN LockedForSynch = FALSE;

    AccessType = IO_METHOD_FROM_CTL_CODE(IoControlCode);

    if (PreviousMode != KernelMode)
    {
        _VEA_TRY
        {
            if (AccessType == METHOD_BUFFERED)
            {
                if (OutputBuffer)
                {
                    ProbeForWrite(
                        OutputBuffer,
                        OutputBufferLength,
                        sizeof(CHAR)
                    );
                }
                else
                {
                    OutputBufferLength = 0;
                }
            }

            if (AccessType != METHOD_NEITHER)
            {
                if (InputBuffer)
                {
                    ProbeForRead(
                        InputBuffer,
                        InputBufferLength,
                        sizeof(CHAR)
                    );
                }
                else
                {
                    InputBufferLength = 0;
                }
            }
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            _VEA_YIELD(return GetExceptionCode());
        }
        _VEA_END
    }
    
    Status = ObReferenceObjectByHandle(
        DeviceHandle,
        0,
        ObpFileObjectType,
        PreviousMode,
        (PVOID*)&FileObject
    );
    if (!VEA_SUCCESS(Status)) return Status;
    
    if (PreviousMode != KernelMode)
    {
        // TODO
    }

    if (Event)
    {
        Status = ObReferenceObjectByHandle(
            Event,
            EVENT_MODIFY_STATE,
            UlEventObjectType,
            PreviousMode,
            (PVOID*)&IoEvent
        );

        if (!VEA_SUCCESS(Status))
        {
            ObDereferenceObject(FileObject);
            return Status;
        }
    }

    if (FileObject->Flags & FO_SYNCHRONOUS_IO)
    {
        // TODO
    }

    if (FileObject->Flags & FO_DIRECT_DEVICE_OPEN)
    {
        DeviceObject = ObioGetTopDeviceObject(FileObject->DeviceObject);
    }
    else
    {
        DeviceObject = ObioGetRelatedDeviceObject(FileObject);
    }

    if (IsDevIoCtl)
    {
        // TODO: Fast IO isn't implemented
    }

    KsClearEvent(&FileObject->Event);

    Oip = ObioAllocateOip((OBJECT_TO_OBJECT_HEADER(DeviceObject)->StackDepth));
    if (!Oip)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Oip->UserInfoBlock = IoStatusBlock;
    Oip->UserEvent = IoEvent;
    // TODO:
    // Irp->Overlay.AsynchronousParameters.UserApcRoutine = UserApcRoutine;
    // Irp->Overlay.AsynchronousParameters.UserApcContext = UserApcContext;
    Oip->Cancel = FALSE;
    Oip->CancelRoutine = NULL;
    Oip->RequestorMode = PreviousMode;
    Oip->MdlAddress = NULL;
    Oip->AssociatedOip.SystemBuffer = NULL;
    Oip->Flags = 0;

    StackLocation = ObioGetNextOipStackLocation(Oip);
    StackLocation->FileObject = FileObject;
    StackLocation->MajorFunction = 
        IsDevIoCtl ? OIP_MJ_DEVICE_CONTROL : OIP_MJ_FILE_SYSTEM_CONTROL;
    StackLocation->MinorFunction = 0;
    StackLocation->Control = 0;
    StackLocation->Flags = 0;
    StackLocation->Parameters.DeviceControl.Type3InputBuffer = NULL;

    StackLocation->Parameters.DeviceControl.IoControlCode = IoControlCode;
    StackLocation->Parameters.DeviceControl.InputBufferLength = InputBufferLength;
    StackLocation->Parameters.DeviceControl.OutputBufferLength = OutputBufferLength;

    switch (AccessType)
    {
        case METHOD_BUFFERED:
            _VEA_TRY
            {
                BufferLength = (InputBufferLength > OutputBufferLength) ?
                                InputBufferLength : OutputBufferLength;

                if (BufferLength)
                {
                    Oip->AssociatedOip.SystemBuffer = 
                        UlAllocatePoolWithTag(NonPagedPool, BufferLength, TAG_OBIO);

                    if (InputBuffer)
                    {
                        RtlCopyMemory(Oip->AssociatedOip.SystemBuffer, InputBuffer, InputBufferLength);
                    }

                    Oip->Flags = OIP_BUFFERED_IO | OIP_DEALLOCATE_BUFFER;
                    if (OutputBuffer) Oip->Flags |= OIP_INPUT_OPERATION;

                    Oip->UserBuffer = OutputBuffer;
                }
                else
                {
                    Oip->UserBuffer = FALSE;
                }
            }
            _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
            {
                // ObioCleanupAfterException
                _VEA_YIELD(return GetExceptionCode());
            }
            _VEA_END;
            break;

        case METHOD_IN_DIRECT:
        case METHOD_OUT_DIRECT:

            _VEA_TRY
            {
                if ((InputBufferLength) && (InputBuffer))
                {
                    Oip->AssociatedOip.SystemBuffer = 
                        UlAllocatePoolZero(NonPagedPool, InputBufferLength, TAG_OBIO);

                    RtlCopyMemory(Oip->AssociatedOip.SystemBuffer, InputBuffer, InputBufferLength);

                    Oip->Flags = OIP_BUFFERED_IO | OIP_DEALLOCATE_BUFFER;
                }

                if (OutputBufferLength)
                {
                    Oip->MdlAddress = ObioAllocateMdl(
                        OutputBuffer,
                        OutputBufferLength,
                        FALSE,
                        FALSE,
                        Oip
                    );

                    if (!Oip->MdlAddress)
                    {
                        UlRaiseStatus(STATUS_INSUFFICIENT_RESOURCES);
                    }

                    MmProbeAndLockPages(
                        Oip->MdlAddress,
                        PreviousMode,
                        (AccessType == METHOD_IN_DIRECT) ?
                        IoReadAccess : IoWriteAccess
                    );
                }
            }
            _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
            {
                // ObioCleanupAfterException
                _VEA_YIELD(return GetExceptionCode());
            }
            _VEA_END;
            break;

        case METHOD_NEITHER:
            Oip->UserBuffer = OutputBuffer;
            StackLocation->Parameters.DeviceControl.Type3InputBuffer = InputBuffer;
    }

    if (!IsDevIoCtl)
    {
        Oip->Flags |= OIP_DEFER_IO_COMPLETION;
    }

    return ObioPerformSychronusRequest(
        DeviceObject,
        Oip,
        FileObject,
        !IsDevIoCtl,
        PreviousMode,
        LockedForSynch,
        IopOtherTransfer
    );
}

/* EOF */