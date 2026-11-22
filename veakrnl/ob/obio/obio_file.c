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

static
VEASTATUS
ObioReadWriteScatterGather(
    IN HANDLE FileHandle,
    IN HANDLE Event OPTIONAL,
    IN PIO_APC_ROUTINE UserApcRoutine OPTIONAL,
    IN PVOID UserApcContext OPTIONAL,
    OUT POIP_INFORMATION IoStatusBlock,
    IN FILE_SEGMENT_ELEMENT BufferDescription[],
    IN ULONG BufferLength,
    IN PLARGE_INTEGER ByteOffset,
    IN PULONG Key OPTIONAL,
    IN BOOLEAN IsWrite
)
{
    VEASTATUS Status;
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();
    PFILE_OBJECT FileObject = NULL;
    PDEVICE_OBJECT DeviceObject = NULL;
    PKEVENT IoEvent = NULL;
    POIP Oip = NULL;
    POIP_STACK StackLocation = NULL;
    LARGE_INTEGER LocalByteOffset;
    ULONG LocalKey = 0;
    ULONG PageCount;
    ULONG SectorSize;
    PMDL MasterMdl = NULL;
    PPFN_NUMBER MasterPfnArray;
    PVOID *SegmentList = NULL;
    BOOLEAN LockedForSynch = FALSE;

    if (!IoStatusBlock || !BufferDescription || !ByteOffset)
    {
        return STATUS_INVALID_PARAMETER;
    }

    /* Reference FileObject dengan hak akses Read atau Write */
    Status = ObReferenceObjectByHandle(
        FileHandle,
        IsWrite ? FILE_WRITE_DATA : FILE_READ_DATA,
        ObpFileObjectType,
        PreviousMode,
        (PVOID*)&FileObject
    );
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    /* Syarat mutlak NT: Scatter/Gather HANYA untuk No Intermediate Buffering */
    if (!(FileObject->Flags & FO_NO_INTERMEDIATE_BUFFERING))
    {
        ObDereferenceObject(FileObject);
        return STATUS_INVALID_PARAMETER;
    }

    DeviceObject = ObioGetRelatedDeviceObject(FileObject);
    SectorSize = 512; /* TODO */

    /* Validasi & salin parameter dari UserMode */
    if (PreviousMode != KernelMode)
    {
        _VEA_TRY
        {
            ProbeForWrite(IoStatusBlock, sizeof(OIP_INFORMATION), sizeof(ULONG));

            LocalByteOffset = ProbeForReadLargeInteger(ByteOffset);
            if (Key)
            {
                LocalKey = ProbeForReadUlong(Key);
            }

            if (BufferLength > 0)
            {
                PageCount = BYTES_TO_PAGES(BufferLength);
                ProbeForRead(
                    BufferDescription,
                    PageCount * sizeof(FILE_SEGMENT_ELEMENT),
                    sizeof(ULONGLONG)
                );
            }
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            ObDereferenceObject(FileObject);
            _VEA_YIELD(return GetExceptionCode());
        }
        _VEA_END;
    }
    else
    {
        LocalByteOffset = *ByteOffset;
        if (Key) LocalKey = *Key;
    }

    /* Cek alignment sector untuk ByteOffset dan BufferLength */
    if (SectorSize > 0)
    {
        if ((BufferLength & (SectorSize - 1)) != 0 ||
            (LocalByteOffset.LowPart & (SectorSize - 1)) != 0)
        {
            ObDereferenceObject(FileObject);
            return STATUS_INVALID_PARAMETER;
        }
    }

    /* Reference Event jika disediakan caller */
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
        KsClearEvent(IoEvent);
    }
    else
    {
        KsClearEvent(&FileObject->Event);
    }

    /* Handle kasus BufferLength == 0 (langsung sukses sesuai perilaku NT) */
    if (BufferLength == 0)
    {
        _VEA_TRY
        {
            IoStatusBlock->Status = STATUS_SUCCESS;
            IoStatusBlock->Information = 0;
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            if (IoEvent) ObDereferenceObject(IoEvent);
            ObDereferenceObject(FileObject);
            _VEA_YIELD(return GetExceptionCode());
        }
        _VEA_END;

        if (IoEvent)
        {
            KsSetEvent(IoEvent, FALSE);
            ObDereferenceObject(IoEvent);
        }
        else
        {
            KsSetEvent(&FileObject->Event, FALSE);
        }

        ObDereferenceObject(FileObject);
        return STATUS_SUCCESS;
    }

    PageCount = BYTES_TO_PAGES(BufferLength);

    /* Salin pointer segmen ke buffer kernel & pastikan semuanya PAGE_SIZE aligned */
    SegmentList = UlAllocatePoolWithTag(NonPagedPool, PageCount * sizeof(PVOID), TAG_OBIO);
    if (!SegmentList)
    {
        if (IoEvent) ObDereferenceObject(IoEvent);
        ObDereferenceObject(FileObject);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    _VEA_TRY
    {
        for (ULONG i = 0; i < PageCount; i++)
        {
            ULONGLONG RawAddr = BufferDescription[i].Alignment;

            if (!RawAddr ||
                (RawAddr & (PAGE_SIZE - 1)) != 0 ||
                (RawAddr & ~((ULONGLONG)(ULONG_PTR)-1)) != 0)
            {
                UlRaiseStatus(STATUS_INVALID_PARAMETER);
            }

            SegmentList[i] = (PVOID)(ULONG_PTR)RawAddr;
        }
    }
    _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
    {
        UlFreePoolWithTag(SegmentList, TAG_OBIO);
        if (IoEvent) ObDereferenceObject(IoEvent);
        ObDereferenceObject(FileObject);
        _VEA_YIELD(return GetExceptionCode());
    }
    _VEA_END;

    /* Alokasi OIP */
    Oip = ObioAllocateOip(OBJECT_TO_OBJECT_HEADER(DeviceObject)->StackDepth);
    if (!Oip)
    {
        UlFreePoolWithTag(SegmentList, TAG_OBIO);
        if (IoEvent) ObDereferenceObject(IoEvent);
        ObDereferenceObject(FileObject);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Oip->UserInfoBlock = IoStatusBlock;
    Oip->UserEvent = IoEvent;
    Oip->Cancel = FALSE;
    Oip->CancelRoutine = NULL;
    Oip->RequestorMode = PreviousMode;
    Oip->MdlAddress = NULL;
    Oip->AssociatedOip.SystemBuffer = NULL;
    Oip->Flags = OIP_NOCACHE | OIP_DEFER_IO_COMPLETION;

    /* Bangun Master MDL dan "jahit" PFN tiap segmen halaman */
    MasterMdl = ObioAllocateMdl(SegmentList[0], BufferLength, FALSE, FALSE, Oip);
    if (!MasterMdl)
    {
        UlFreePoolWithTag(SegmentList, TAG_OBIO);
        ObioFreeOip(Oip);
        if (IoEvent) ObDereferenceObject(IoEvent);
        ObDereferenceObject(FileObject);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    MasterPfnArray = (PPFN_NUMBER)(MasterMdl + 1);

    _VEA_TRY
    {
        for (ULONG i = 0; i < PageCount; i++)
        {
            PMDL TempMdl = ObioAllocateMdl(SegmentList[i], PAGE_SIZE, FALSE, FALSE, NULL);
            if (!TempMdl)
            {
                UlRaiseStatus(STATUS_INSUFFICIENT_RESOURCES);
            }

            /* ReadFileScatter menulis ke buffer user -> IoWriteAccess
               WriteFileGather membaca dari buffer user -> IoReadAccess */
            MmProbeAndLockPages(
                TempMdl,
                PreviousMode,
                IsWrite ? IoReadAccess : IoWriteAccess
            );

            MasterPfnArray[i] = *((PPFN_NUMBER)(TempMdl + 1));
            ObioFreeMdl(TempMdl); 
        }

        MasterMdl->MdlFlags |= MDL_PAGES_LOCKED;
    }
    _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
    {
        /* TODO: Unlock halaman yang sudah sempat ter-lock sebelum index error */
        UlFreePoolWithTag(SegmentList, TAG_OBIO);
        if (IoEvent) ObDereferenceObject(IoEvent);
        ObDereferenceObject(FileObject);
        _VEA_YIELD(return GetExceptionCode());
    }
    _VEA_END;

    UlFreePoolWithTag(SegmentList, TAG_OBIO);

    /* Setup Stack Location untuk FSD (VeaFAT, dll.) */
    StackLocation = ObioGetNextOipStackLocation(Oip);
    StackLocation->FileObject = FileObject;
    StackLocation->MajorFunction = IsWrite ? OIP_MJ_WRITE : OIP_MJ_READ;
    StackLocation->MinorFunction = 0;
    StackLocation->Control = 0;
    StackLocation->Flags = 0;
    StackLocation->Parameters.ReadWrite.Length = BufferLength;
    StackLocation->Parameters.ReadWrite.Offset = LocalByteOffset;
    StackLocation->Parameters.ReadWrite.Key = LocalKey;

    if (FileObject->Flags & FO_SYNCHRONOUS_IO)
    {
        LockedForSynch = TRUE;
    }

    Status = ObioPerformSychronusRequest(
        DeviceObject,
        Oip,
        FileObject,
        TRUE,
        PreviousMode,
        LockedForSynch,
        IsWrite ? IopWriteTransfer : IopReadTransfer
    );

    if (IoEvent)
    {
        ObDereferenceObject(IoEvent);
    }

    ObDereferenceObject(FileObject);
    return Status;
}

static
VEASTATUS
ObioDeviceFsIoControl(
    IN HANDLE DeviceHandle,
    IN HANDLE Event OPTIONAL,
    IN PIO_APC_ROUTINE UserApcRoutine OPTIONAL,
    IN PVOID UserApcContext OPTIONAL,
    OUT POIP_INFORMATION IoStatusBlock,
    IN ULONG IoControlCode,
    IN PVOID InputBuffer OPTIONAL,
    IN ULONG InputBufferLength,
    OUT PVOID OutputBuffer OPTIONAL,
    IN ULONG OutputBufferLength,
    IN BOOLEAN IsDevIoCtl
)
{
    VEASTATUS Status;
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();
    PFILE_OBJECT FileObject = NULL;
    PDEVICE_OBJECT DeviceObject = NULL;
    PKEVENT IoEvent = NULL;
    POIP Oip = NULL;
    POIP_STACK StackLocation = NULL;
    ULONG BufferLength;
    ULONG AccessType;
    BOOLEAN LockedForSynch = FALSE;

    if (!IoStatusBlock)
    {
        return STATUS_INVALID_PARAMETER;
    }

    AccessType = IO_METHOD_FROM_CTL_CODE(IoControlCode);

    if (PreviousMode != KernelMode)
    {
        _VEA_TRY
        {
            ProbeForWrite(IoStatusBlock, sizeof(OIP_INFORMATION), sizeof(ULONG));

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
        _VEA_END;
    }

    Status = ObReferenceObjectByHandle(
        DeviceHandle,
        0,
        ObpFileObjectType,
        PreviousMode,
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
        LockedForSynch = TRUE;
    }

    if (FileObject->Flags & FO_DIRECT_DEVICE_OPEN)
    {
        DeviceObject = ObioGetTopDeviceObject(FileObject->DeviceObject);
    }
    else
    {
        DeviceObject = ObioGetRelatedDeviceObject(FileObject);
    }

    KsClearEvent(&FileObject->Event);

    Oip = ObioAllocateOip(OBJECT_TO_OBJECT_HEADER(DeviceObject)->StackDepth);
    if (!Oip)
    {
        if (IoEvent) ObDereferenceObject(IoEvent);
        ObDereferenceObject(FileObject);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Oip->UserInfoBlock = IoStatusBlock;
    Oip->UserEvent = IoEvent;
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
                    Oip->UserBuffer = NULL;
                }
            }
            _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
            {
                if (IoEvent) ObDereferenceObject(IoEvent);
                ObDereferenceObject(FileObject);
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
                if (IoEvent) ObDereferenceObject(IoEvent);
                ObDereferenceObject(FileObject);
                _VEA_YIELD(return GetExceptionCode());
            }
            _VEA_END;
            break;

        case METHOD_NEITHER:
            Oip->UserBuffer = OutputBuffer;
            StackLocation->Parameters.DeviceControl.Type3InputBuffer = InputBuffer;
            break;
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
    PKEVENT CompletionEvent;
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

    CompletionEvent = EventObject ? EventObject : &FileObject->Event;
    KsClearEvent(CompletionEvent);

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
        CompletionEvent,
        IoStatusBlock
    );

    if (!Oip)
    {
        if (EventObject) ObDereferenceObject(EventObject);
        ObDereferenceObject(FileObject);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Oip->RequestorThread = (PETHREAD)PtGetCurrentThread();
    if (Oip->RequestorThread)
    {
        ObReferenceObject(Oip->RequestorThread);
    }
    
    StackPtr = ObioGetNextOipStackLocation(Oip);
    StackPtr->FileObject = FileObject;
    if (Key) StackPtr->Parameters.ReadWrite.Key = *Key;

    Status = ObioCallObject(DeviceObject, Oip);

    if (Status == STATUS_PENDING)
    {
        KsWaitForSingleObject(CompletionEvent);
        Status = IoStatusBlock->Status;
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
    IN HANDLE FileHandle,
    IN HANDLE Event OPTIONAL,
    IN PIO_APC_ROUTINE UserApcRoutine OPTIONAL,
    IN PVOID UserApcContext OPTIONAL,
    OUT POIP_INFORMATION IoStatusBlock,
    IN ULONG FsControlCode,
    IN PVOID InputBuffer OPTIONAL,
    IN ULONG InputBufferLength,
    OUT PVOID OutputBuffer OPTIONAL,
    IN ULONG OutputBufferLength
)
{
    return ObioDeviceFsIoControl(
        FileHandle,
        Event,
        UserApcRoutine,
        UserApcContext,
        IoStatusBlock,
        FsControlCode,
        InputBuffer,
        InputBufferLength,
        OutputBuffer,
        OutputBufferLength,
        FALSE
    );
}

VEASTATUS
VEAPI
VeaDeviceIoControlFile(
    IN HANDLE FileHandle,
    IN HANDLE Event OPTIONAL,
    IN PIO_APC_ROUTINE UserApcRoutine OPTIONAL,
    IN PVOID UserApcContext OPTIONAL,
    OUT POIP_INFORMATION IoStatusBlock,
    IN ULONG IoControlCode,
    IN PVOID InputBuffer OPTIONAL,
    IN ULONG InputBufferLength,
    OUT PVOID OutputBuffer OPTIONAL,
    IN ULONG OutputBufferLength
)
{
    return ObioDeviceFsIoControl(
        FileHandle,
        Event,
        UserApcRoutine,
        UserApcContext,
        IoStatusBlock,
        IoControlCode,
        InputBuffer,
        InputBufferLength,
        OutputBuffer,
        OutputBufferLength,
        TRUE
    );
}

VEASTATUS
VEAPI
VeaWriteFile(
    IN HANDLE FileHandle,
    IN HANDLE Event OPTIONAL,
    IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
    IN PVOID ApcContext OPTIONAL,
    OUT POIP_INFORMATION OipStatusBlock,
    IN PVOID Buffer,
    IN ULONG Length,
    IN PLARGE_INTEGER ByteOffset OPTIONAL,
    IN PULONG Key OPTIONAL
)
{
    PFILE_OBJECT FileObject = NULL;
    PDEVICE_OBJECT DeviceObject = NULL;
    PKEVENT EventObject = NULL;
    POIP Oip = NULL;
    POIP_STACK StackPtr = NULL;
    VEASTATUS Status;
    KPROCESSOR_MODE AccessMode = KeGetPreviousMode();
    LARGE_INTEGER LocalByteOffset;

    if (!OipStatusBlock || (!Buffer && Length > 0))
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (AccessMode != KernelMode)
    {
        _VEA_TRY
        {
            ProbeForWrite(OipStatusBlock, sizeof(OIP_INFORMATION), sizeof(ULONG));
            if (Length > 0)
            {
                ProbeForRead(Buffer, Length, sizeof(CHAR));
            }
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            _VEA_YIELD(return GetExceptionCode());
        }
        _VEA_END;
    }

    Status = ObReferenceObjectByHandle(
        FileHandle,
        FILE_WRITE_DATA,
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
    else
    {
        KsClearEvent(&FileObject->Event);
    }

    if (ByteOffset)
    {
        LocalByteOffset = *ByteOffset;
    }
    else
    {
        LocalByteOffset = FileObject->CurrentByteOffset;
    }

    DeviceObject = ObioGetRelatedDeviceObject(FileObject);

    Oip = ObioBuildSynchronusFsdRequest(
        OIP_MJ_WRITE,
        DeviceObject,
        Buffer,
        Length,
        &LocalByteOffset,
        EventObject ? EventObject : &FileObject->Event,
        OipStatusBlock
    );
    if (!Oip)
    {
        if (EventObject) ObDereferenceObject(EventObject);
        ObDereferenceObject(FileObject);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    StackPtr = ObioGetNextOipStackLocation(Oip);
    StackPtr->FileObject = FileObject;
    if (Key)
    {
        StackPtr->Parameters.ReadWrite.Key = *Key;
    }

    Status = ObioCallObject(DeviceObject, Oip);

    if (Status == STATUS_PENDING)
    {
        if (FileObject->Flags & FO_SYNCHRONOUS_IO)
        {
            KsWaitForSingleObject(EventObject ? EventObject : &FileObject->Event);
            Status = OipStatusBlock->Status;
        }
    }

    if (VEA_SUCCESS(Status) && Status != STATUS_PENDING)
    {
        if (!ByteOffset && (FileObject->Flags & FO_SYNCHRONOUS_IO))
        {
            FileObject->CurrentByteOffset.QuadPart =
                LocalByteOffset.QuadPart + OipStatusBlock->Information;
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
VeaReadFileScatter(
    IN HANDLE FileHandle,
    IN HANDLE Event OPTIONAL,
    IN PIO_APC_ROUTINE UserApcRoutine OPTIONAL,
    IN PVOID UserApcContext OPTIONAL,
    OUT POIP_INFORMATION IoStatusBlock,
    IN FILE_SEGMENT_ELEMENT BufferDescription[],
    IN ULONG BufferLength,
    IN PLARGE_INTEGER ByteOffset,
    IN PULONG Key OPTIONAL
)
{
    return ObioReadWriteScatterGather(
        FileHandle,
        Event,
        UserApcRoutine,
        UserApcContext,
        IoStatusBlock,
        BufferDescription,
        BufferLength,
        ByteOffset,
        Key,
        FALSE
    );
}

VEASTATUS
VEAPI
VeaWriteFileGather(
    IN HANDLE FileHandle,
    IN HANDLE Event OPTIONAL,
    IN PIO_APC_ROUTINE UserApcRoutine OPTIONAL,
    IN PVOID UserApcContext OPTIONAL,
    OUT POIP_INFORMATION IoStatusBlock,
    IN FILE_SEGMENT_ELEMENT BufferDescription[],
    IN ULONG BufferLength,
    IN PLARGE_INTEGER ByteOffset,
    IN PULONG Key OPTIONAL
)
{
    return ObioReadWriteScatterGather(
        FileHandle,
        Event,
        UserApcRoutine,
        UserApcContext,
        IoStatusBlock,
        BufferDescription,
        BufferLength,
        ByteOffset,
        Key,
        TRUE
    );
}

VEASTATUS
VEAPI
VeaDeleteFile(
    IN POBJECT_ATTRIBUTES ObjectAttributes
)
{
    HANDLE FileHandle = NULL;
    VEASTATUS Status;

    if (!ObjectAttributes)
    {
        return STATUS_INVALID_PARAMETER;
    }

    Status = ObioCreateFile(
        &FileHandle,
        VEA_DELETE,
        ObjectAttributes,
        NULL,
        NULL,
        FILE_ATTRIBUTE_NORMAL,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        FILE_OPEN,
        FILE_DELETE_ON_CLOSE,
        NULL,
        0
    );

    if (VEA_SUCCESS(Status))
    {
        VeaClose(FileHandle);
    }

    return Status;
}

VEASTATUS
VEAPI
VeaFlushBuffersFile(
    IN HANDLE FileHandle,
    OUT POIP_INFORMATION IoStatusBlock
)
{
    VEASTATUS Status;
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();
    PFILE_OBJECT FileObject = NULL;
    PDEVICE_OBJECT DeviceObject = NULL;
    POIP Oip = NULL;
    POIP_STACK StackLocation = NULL;
    BOOLEAN LockedForSynch = FALSE;

    if (!IoStatusBlock)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (PreviousMode != KernelMode)
    {
        _VEA_TRY
        {
            ProbeForWrite(IoStatusBlock, sizeof(OIP_INFORMATION), sizeof(ULONG));
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            _VEA_YIELD(return GetExceptionCode());
        }
        _VEA_END;
    }

    Status = ObReferenceObjectByHandle(
        FileHandle,
        FILE_WRITE_DATA,
        ObpFileObjectType,
        PreviousMode,
        (PVOID*)&FileObject
    );
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    if (FileObject->Flags & FO_SYNCHRONOUS_IO)
    {
        LockedForSynch = TRUE;
    }

    DeviceObject = ObioGetRelatedDeviceObject(FileObject);
    KsClearEvent(&FileObject->Event);

    Oip = ObioAllocateOip(OBJECT_TO_OBJECT_HEADER(DeviceObject)->StackDepth);
    if (!Oip)
    {
        ObDereferenceObject(FileObject);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Oip->UserInfoBlock = IoStatusBlock;
    Oip->UserEvent = NULL;
    Oip->Cancel = FALSE;
    Oip->CancelRoutine = NULL;
    Oip->RequestorMode = PreviousMode;
    Oip->MdlAddress = NULL;
    Oip->AssociatedOip.SystemBuffer = NULL;
    Oip->Flags = OIP_DEFER_IO_COMPLETION;

    StackLocation = ObioGetNextOipStackLocation(Oip);
    StackLocation->MajorFunction = OIP_MJ_FLUSH_BUFFERS;
    StackLocation->MinorFunction = 0;
    StackLocation->Flags = 0;
    StackLocation->Control = 0;
    StackLocation->FileObject = FileObject;

    Status = ObioPerformSychronusRequest(
        DeviceObject,
        Oip,
        FileObject,
        TRUE,
        PreviousMode,
        LockedForSynch,
        IopOtherTransfer
    );

    ObDereferenceObject(FileObject);
    return Status;
}

VEASTATUS
VEAPI
VeaQueryInformationFile(
    IN HANDLE FileHandle,
    OUT POIP_INFORMATION IoStatusBlock,
    OUT PVOID FileInformation,
    IN ULONG Length,
    IN FILE_INFORMATION_CLASS FileInformationClass
)
{
    VEASTATUS Status;
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();
    PFILE_OBJECT FileObject = NULL;
    PDEVICE_OBJECT DeviceObject = NULL;
    POIP Oip = NULL;
    POIP_STACK StackLocation = NULL;
    BOOLEAN LockedForSynch = FALSE;

    if (!IoStatusBlock || !FileInformation || Length == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (PreviousMode != KernelMode)
    {
        _VEA_TRY
        {
            ProbeForWrite(IoStatusBlock, sizeof(OIP_INFORMATION), sizeof(ULONG));
            ProbeForWrite(FileInformation, Length, sizeof(CHAR));
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            _VEA_YIELD(return GetExceptionCode());
        }
        _VEA_END;
    }

    Status = ObReferenceObjectByHandle(
        FileHandle,
        0,
        ObpFileObjectType,
        PreviousMode,
        (PVOID*)&FileObject
    );
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    /* Fast-path NT: FilePositionInformation ditangani langsung oleh I/O Manager */
    if (FileInformationClass == FilePositionInformation)
    {
        if (Length < sizeof(FILE_POSITION_INFORMATION))
        {
            ObDereferenceObject(FileObject);
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        _VEA_TRY
        {
            ((PFILE_POSITION_INFORMATION)FileInformation)->CurrentByteOffset =
                FileObject->CurrentByteOffset;
            IoStatusBlock->Status = STATUS_SUCCESS;
            IoStatusBlock->Information = sizeof(FILE_POSITION_INFORMATION);
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            ObDereferenceObject(FileObject);
            _VEA_YIELD(return GetExceptionCode());
        }
        _VEA_END;

        ObDereferenceObject(FileObject);
        return STATUS_SUCCESS;
    }

    if (FileObject->Flags & FO_SYNCHRONOUS_IO)
    {
        LockedForSynch = TRUE;
    }

    DeviceObject = ObioGetRelatedDeviceObject(FileObject);
    KsClearEvent(&FileObject->Event);

    Oip = ObioAllocateOip(OBJECT_TO_OBJECT_HEADER(DeviceObject)->StackDepth);
    if (!Oip)
    {
        ObDereferenceObject(FileObject);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Oip->AssociatedOip.SystemBuffer = UlAllocatePoolZero(NonPagedPool, Length, TAG_OBIO);
    if (!Oip->AssociatedOip.SystemBuffer)
    {
        ObioFreeOip(Oip);
        ObDereferenceObject(FileObject);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Oip->UserInfoBlock = IoStatusBlock;
    Oip->UserEvent = NULL;
    Oip->UserBuffer = FileInformation;
    Oip->Cancel = FALSE;
    Oip->CancelRoutine = NULL;
    Oip->RequestorMode = PreviousMode;
    Oip->MdlAddress = NULL;
    Oip->Flags = OIP_BUFFERED_IO |
                 OIP_DEALLOCATE_BUFFER |
                 OIP_INPUT_OPERATION |
                 OIP_DEFER_IO_COMPLETION;

    StackLocation = ObioGetNextOipStackLocation(Oip);
    StackLocation->MajorFunction = OIP_MJ_QUERY_INFORMATION;
    StackLocation->MinorFunction = 0;
    StackLocation->Flags = 0;
    StackLocation->Control = 0;
    StackLocation->FileObject = FileObject;
    StackLocation->Parameters.QueryFile.Length = Length;
    StackLocation->Parameters.QueryFile.FileInformationClass = FileInformationClass;

    Status = ObioPerformSychronusRequest(
        DeviceObject,
        Oip,
        FileObject,
        TRUE,
        PreviousMode,
        LockedForSynch,
        IopOtherTransfer
    );

    ObDereferenceObject(FileObject);
    return Status;
}

VEASTATUS
VEAPI
VeaSetInformationFile(
    IN HANDLE FileHandle,
    OUT POIP_INFORMATION IoStatusBlock,
    IN PVOID FileInformation,
    IN ULONG Length,
    IN FILE_INFORMATION_CLASS FileInformationClass
)
{
    VEASTATUS Status;
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();
    PFILE_OBJECT FileObject = NULL;
    PDEVICE_OBJECT DeviceObject = NULL;
    POIP Oip = NULL;
    POIP_STACK StackLocation = NULL;
    BOOLEAN LockedForSynch = FALSE;

    if (!IoStatusBlock || !FileInformation || Length == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (PreviousMode != KernelMode)
    {
        _VEA_TRY
        {
            ProbeForWrite(IoStatusBlock, sizeof(OIP_INFORMATION), sizeof(ULONG));
            ProbeForRead(FileInformation, Length, sizeof(CHAR));
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            _VEA_YIELD(return GetExceptionCode());
        }
        _VEA_END;
    }

    Status = ObReferenceObjectByHandle(
        FileHandle,
        0,
        ObpFileObjectType,
        PreviousMode,
        (PVOID*)&FileObject
    );
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    /* Fast-path NT: Geser CurrentByteOffset langsung tanpa lempar ke FSD */
    if (FileInformationClass == FilePositionInformation)
    {
        FILE_POSITION_INFORMATION PosInfo;

        if (Length < sizeof(FILE_POSITION_INFORMATION))
        {
            ObDereferenceObject(FileObject);
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        _VEA_TRY
        {
            PosInfo = *(PFILE_POSITION_INFORMATION)FileInformation;
            if (PosInfo.CurrentByteOffset.QuadPart < 0)
            {
                UlRaiseStatus(STATUS_INVALID_PARAMETER);
            }

            FileObject->CurrentByteOffset = PosInfo.CurrentByteOffset;
            IoStatusBlock->Status = STATUS_SUCCESS;
            IoStatusBlock->Information = 0;
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            ObDereferenceObject(FileObject);
            _VEA_YIELD(return GetExceptionCode());
        }
        _VEA_END;

        ObDereferenceObject(FileObject);
        return STATUS_SUCCESS;
    }

    if (FileObject->Flags & FO_SYNCHRONOUS_IO)
    {
        LockedForSynch = TRUE;
    }

    DeviceObject = ObioGetRelatedDeviceObject(FileObject);
    KsClearEvent(&FileObject->Event);

    Oip = ObioAllocateOip(OBJECT_TO_OBJECT_HEADER(DeviceObject)->StackDepth);
    if (!Oip)
    {
        ObDereferenceObject(FileObject);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Oip->AssociatedOip.SystemBuffer = UlAllocatePoolWithTag(NonPagedPool, Length, TAG_OBIO);
    if (!Oip->AssociatedOip.SystemBuffer)
    {
        ObioFreeOip(Oip);
        ObDereferenceObject(FileObject);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    _VEA_TRY
    {
        RtlCopyMemory(Oip->AssociatedOip.SystemBuffer, FileInformation, Length);
    }
    _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
    {
        UlFreePoolWithTag(Oip->AssociatedOip.SystemBuffer, TAG_OBIO);
        ObDereferenceObject(FileObject);
        _VEA_YIELD(return GetExceptionCode());
    }
    _VEA_END;

    Oip->UserInfoBlock = IoStatusBlock;
    Oip->UserEvent = NULL;
    Oip->UserBuffer = NULL;
    Oip->Cancel = FALSE;
    Oip->CancelRoutine = NULL;
    Oip->RequestorMode = PreviousMode;
    Oip->MdlAddress = NULL;
    Oip->Flags = OIP_BUFFERED_IO |
                 OIP_DEALLOCATE_BUFFER |
                 OIP_DEFER_IO_COMPLETION;

    StackLocation = ObioGetNextOipStackLocation(Oip);
    StackLocation->MajorFunction = OIP_MJ_SET_INFORMATION;
    StackLocation->MinorFunction = 0;
    StackLocation->Flags = 0;
    StackLocation->Control = 0;
    StackLocation->FileObject = FileObject;
    StackLocation->Parameters.SetFile.Length = Length;
    StackLocation->Parameters.SetFile.FileInformationClass = FileInformationClass;

    Status = ObioPerformSychronusRequest(
        DeviceObject,
        Oip,
        FileObject,
        TRUE,
        PreviousMode,
        LockedForSynch,
        IopOtherTransfer
    );

    ObDereferenceObject(FileObject);
    return Status;
}

VEASTATUS
VEAPI
VeaQueryVolumeInformationFile(
    IN HANDLE FileHandle,
    OUT POIP_INFORMATION IoStatusBlock,
    OUT PVOID FsInformation,
    IN ULONG Length,
    IN FS_INFORMATION_CLASS FsInformationClass
)
{
    VEASTATUS Status;
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();
    PFILE_OBJECT FileObject = NULL;
    PDEVICE_OBJECT DeviceObject = NULL;
    POIP Oip = NULL;
    POIP_STACK StackLocation = NULL;
    BOOLEAN LockedForSynch = FALSE;

    if (!IoStatusBlock || !FsInformation || Length == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (PreviousMode != KernelMode)
    {
        _VEA_TRY
        {
            ProbeForWrite(IoStatusBlock, sizeof(OIP_INFORMATION), sizeof(ULONG));
            ProbeForWrite(FsInformation, Length, sizeof(CHAR));
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            _VEA_YIELD(return GetExceptionCode());
        }
        _VEA_END;
    }

    Status = ObReferenceObjectByHandle(
        FileHandle,
        0,
        ObpFileObjectType,
        PreviousMode,
        (PVOID*)&FileObject
    );
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    if (FileObject->Flags & FO_SYNCHRONOUS_IO)
    {
        LockedForSynch = TRUE;
    }

    DeviceObject = ObioGetRelatedDeviceObject(FileObject);
    KsClearEvent(&FileObject->Event);

    Oip = ObioAllocateOip(OBJECT_TO_OBJECT_HEADER(DeviceObject)->StackDepth);
    if (!Oip)
    {
        ObDereferenceObject(FileObject);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Oip->AssociatedOip.SystemBuffer = UlAllocatePoolZero(NonPagedPool, Length, TAG_OBIO);
    if (!Oip->AssociatedOip.SystemBuffer)
    {
        /* TODO: ObioFreeOip(Oip) */
        ObDereferenceObject(FileObject);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Oip->UserInfoBlock = IoStatusBlock;
    Oip->UserEvent = NULL;
    Oip->UserBuffer = FsInformation;
    Oip->Cancel = FALSE;
    Oip->CancelRoutine = NULL;
    Oip->RequestorMode = PreviousMode;
    Oip->MdlAddress = NULL;
    Oip->Flags = OIP_BUFFERED_IO |
                 OIP_DEALLOCATE_BUFFER |
                 OIP_INPUT_OPERATION |
                 OIP_DEFER_IO_COMPLETION;

    StackLocation = ObioGetNextOipStackLocation(Oip);
    StackLocation->MajorFunction = OIP_MJ_QUERY_VOLUME_INFORMATION;
    StackLocation->MinorFunction = 0;
    StackLocation->Control = 0;
    StackLocation->Flags = 0;
    StackLocation->FileObject = FileObject;
    StackLocation->Parameters.QueryVolume.Length = Length;
    StackLocation->Parameters.QueryVolume.FsInformationClass = FsInformationClass;

    Status = ObioPerformSychronusRequest(
        DeviceObject,
        Oip,
        FileObject,
        TRUE,
        PreviousMode,
        LockedForSynch,
        IopOtherTransfer
    );

    ObDereferenceObject(FileObject);
    return Status;
}

VEASTATUS
VEAPI
VeaQueryDirectoryFile(
    IN HANDLE FileHandle,
    IN HANDLE Event OPTIONAL,
    IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
    IN PVOID ApcContext OPTIONAL,
    OUT POIP_INFORMATION IoStatusBlock,
    OUT PVOID FileInformation,
    IN ULONG Length,
    IN FILE_INFORMATION_CLASS FileInformationClass,
    IN BOOLEAN ReturnSingleEntry,
    IN PANSI_STRING FileName OPTIONAL,
    IN BOOLEAN RestartScan
)
{
    VEASTATUS Status;
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();
    PFILE_OBJECT FileObject = NULL;
    PDEVICE_OBJECT DeviceObject = NULL;
    PKEVENT IoEvent = NULL;
    POIP Oip = NULL;
    POIP_STACK StackLocation = NULL;
    PANSI_STRING KernelFileName = NULL;
    BOOLEAN LockedForSynch = FALSE;

    if (!IoStatusBlock || !FileInformation || Length == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (PreviousMode != KernelMode)
    {
        _VEA_TRY
        {
            ProbeForWrite(IoStatusBlock, sizeof(OIP_INFORMATION), sizeof(ULONG));
            ProbeForWrite(FileInformation, Length, sizeof(CHAR));

            if (FileName)
            {
                ANSI_STRING CapturedName;

                ProbeForRead(FileName, sizeof(ANSI_STRING), sizeof(ULONG));
                CapturedName = *FileName;

                if (CapturedName.Length > 0)
                {
                    if (!CapturedName.Buffer)
                    {
                        UlRaiseStatus(STATUS_INVALID_PARAMETER);
                    }

                    ProbeForRead(CapturedName.Buffer, CapturedName.Length, sizeof(CHAR));

                    KernelFileName = UlAllocatePoolZero(
                        NonPagedPool,
                        sizeof(ANSI_STRING) + CapturedName.Length,
                        TAG_OBIO
                    );
                    if (!KernelFileName)
                    {
                        UlRaiseStatus(STATUS_INSUFFICIENT_RESOURCES);
                    }

                    KernelFileName->Length = CapturedName.Length;
                    KernelFileName->MaximumLength = CapturedName.Length;
                    KernelFileName->Buffer = (PCHAR)(KernelFileName + 1);
                    RtlCopyMemory(KernelFileName->Buffer, CapturedName.Buffer, CapturedName.Length);
                }
            }
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            if (KernelFileName)
            {
                UlFreePoolWithTag(KernelFileName, TAG_OBIO);
            }
            _VEA_YIELD(return GetExceptionCode());
        }
        _VEA_END;
    }
    else if (FileName && FileName->Length > 0)
    {
        KernelFileName = UlAllocatePoolZero(
            NonPagedPool,
            sizeof(ANSI_STRING) + FileName->Length,
            TAG_OBIO
        );
        if (!KernelFileName)
        {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        KernelFileName->Length = FileName->Length;
        KernelFileName->MaximumLength = FileName->Length;
        KernelFileName->Buffer = (PCHAR)(KernelFileName + 1);
        RtlCopyMemory(KernelFileName->Buffer, FileName->Buffer, FileName->Length);
    }

    Status = ObReferenceObjectByHandle(
        FileHandle,
        FILE_LIST_DIRECTORY,
        ObpFileObjectType,
        PreviousMode,
        (PVOID*)&FileObject
    );
    if (!VEA_SUCCESS(Status))
    {
        if (KernelFileName) UlFreePoolWithTag(KernelFileName, TAG_OBIO);
        return Status;
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
            if (KernelFileName) UlFreePoolWithTag(KernelFileName, TAG_OBIO);
            ObDereferenceObject(FileObject);
            return Status;
        }
        KsClearEvent(IoEvent);
    }
    else
    {
        KsClearEvent(&FileObject->Event);
    }

    if (FileObject->Flags & FO_SYNCHRONOUS_IO)
    {
        LockedForSynch = TRUE;
    }

    DeviceObject = ObioGetRelatedDeviceObject(FileObject);

    Oip = ObioAllocateOip(OBJECT_TO_OBJECT_HEADER(DeviceObject)->StackDepth);
    if (!Oip)
    {
        if (KernelFileName) UlFreePoolWithTag(KernelFileName, TAG_OBIO);
        if (IoEvent) ObDereferenceObject(IoEvent);
        ObDereferenceObject(FileObject);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Oip->AssociatedOip.SystemBuffer = UlAllocatePoolZero(NonPagedPool, Length, TAG_OBIO);
    if (!Oip->AssociatedOip.SystemBuffer)
    {
        /* TODO: ObioFreeOip(Oip) */
        if (KernelFileName) UlFreePoolWithTag(KernelFileName, TAG_OBIO);
        if (IoEvent) ObDereferenceObject(IoEvent);
        ObDereferenceObject(FileObject);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Oip->UserInfoBlock = IoStatusBlock;
    Oip->UserEvent = IoEvent;
    Oip->UserBuffer = FileInformation;
    Oip->Cancel = FALSE;
    Oip->CancelRoutine = NULL;
    Oip->RequestorMode = PreviousMode;
    Oip->MdlAddress = NULL;
    Oip->Flags = OIP_BUFFERED_IO |
                 OIP_DEALLOCATE_BUFFER |
                 OIP_INPUT_OPERATION |
                 OIP_DEFER_IO_COMPLETION;

    StackLocation = ObioGetNextOipStackLocation(Oip);
    StackLocation->MajorFunction = OIP_MJ_DIRECTORY_CONTROL;
    StackLocation->MinorFunction = OIP_MN_QUERY_DIRECTORY;
    StackLocation->Control = 0;
    StackLocation->Flags = 0;
    if (RestartScan)
    {
        StackLocation->Flags |= SL_RESTART_SCAN;
    }
    if (ReturnSingleEntry)
    {
        StackLocation->Flags |= SL_RETURN_SINGLE_ENTRY;
    }

    StackLocation->FileObject = FileObject;
    StackLocation->Parameters.QueryDirectory.Length = Length;
    StackLocation->Parameters.QueryDirectory.FileName = KernelFileName;
    StackLocation->Parameters.QueryDirectory.FileInformationClass = FileInformationClass;
    StackLocation->Parameters.QueryDirectory.FileIndex = 0;

    Status = ObioPerformSychronusRequest(
        DeviceObject,
        Oip,
        FileObject,
        TRUE,
        PreviousMode,
        LockedForSynch,
        IopOtherTransfer
    );

    if (KernelFileName && Status != STATUS_PENDING)
    {
        UlFreePoolWithTag(KernelFileName, TAG_OBIO);
    }

    if (IoEvent)
    {
        ObDereferenceObject(IoEvent);
    }

    ObDereferenceObject(FileObject);
    return Status;
}

/* EOF */