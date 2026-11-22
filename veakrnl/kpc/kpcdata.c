/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : kpcdata.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : KPC Write and Read Data
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 11-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file kpcdata.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
KpcpReadRequestData(
    IN HANDLE PortHandle,
    IN PKPC_MESSAGE_HEADER Message,
    IN ULONG DataOffset,
    OUT PVOID Buffer,
    IN SIZE_T BufferSize,
    OUT PSIZE_T NumberOfBytesRead OPTIONAL
)
//
// Routine Description:
// Copying the Stream Data from KPC Pipe Mode to Buffer
// 
// Parameter:
// HANDLE PortHandle
//      Handle that point to Port type object
// PKPC_MESSAGE_HEADER Message
//      Message Header to reference message
// ULONG Data Offset
//      Data offset byte count
// SIZE_T BufferSize
//      Callee buffer size
// PSIZE_T NumberOfBytesRead
//      Argument to pass total of Number Bytes Read
// 
// return VEASTATUS:
// VEASTATUS failed if read data failed
//
{
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();
    PKPC_PORT_OBJECT Port = NULL;
    PKPC_PORT_OBJECT TargetPort = NULL;
    PEPROCESS ClientProcess = NULL;
    KAPC_STATE ApcState;
    VEASTATUS Status;
    PVOID RemoteAddress;

    if (!Message || !Buffer || BufferSize == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Reference the port to get the target port. Target port save information such as
    // connected client (process). Attach the process to ourself, and then copy the data
    // to buffer. And then detach the process
    //

    Status = ObReferenceObjectByHandle(
        PortHandle,
        PORT_CONNECT,
        KpcPortObjectType,
        PreviousMode,
        (PVOID*)&Port
    );

    if (!VEA_SUCCESS(Status)) return Status;

    TargetPort = Port->ConnectedPort;
    if (!TargetPort)
    {
        ObDereferenceObject(Port);
        return STATUS_PORT_DISCONNECTED;
    }

    ClientProcess = TargetPort->CreatorProcess;
    if (!ClientProcess)
    {
        ObDereferenceObject(Port);
        return STATUS_PROCESS_IS_TERMINATING;
    }

    RemoteAddress = (PVOID)((ULONG_PTR)Message->DataInfoOffset + DataOffset);

    PtAttachProcess((PKPROCESS)ClientProcess);

    _VEA_TRY
    {
        RtlCopyMemory(Buffer, RemoteAddress, BufferSize);
        if (NumberOfBytesRead)
        {
            *NumberOfBytesRead = BufferSize;
        }
        Status = STATUS_SUCCESS;
    }
    _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
    {
        Status = STATUS_ACCESS_VIOLATION;
    }
    _VEA_END;

    PtDetachProcess();

    ObDereferenceObject(Port);
    return Status;
}

VEASTATUS
VEAPI
KpcpWriteRequestData(
    IN HANDLE PortHandle,
    IN PKPC_MESSAGE_HEADER Message,
    IN ULONG DataOffset,
    IN PVOID Buffer,
    IN SIZE_T BufferSize,
    OUT PSIZE_T NumberOfBytesWritten OPTIONAL
)
///
/// Routine Description:
/// Writing data to Pipe Buffer in Target Process
/// 
/// Parameter:
/// HANDLE PortHandle
///     Handle to Port
/// PKPC_MESSAGE_HEADER Message
///     Pointer to message header
/// ULONG DataOffset
///     Specified DataOffset
/// PVOID Buffer
///     Pointer to buffer that has the data
/// SIZE_T BufferSize
///     Size of the PVOID Buffer
/// PSIZE_T NumberOfBytesWritten
///     Pointer to passing total of Bytes Written
/// 
/// return VEASTATUS
/// Failed means Write has failed
///
{
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();
    PKPC_PORT_OBJECT Port = NULL;
    PKPC_PORT_OBJECT TargetPort = NULL;
    PEPROCESS ClientProcess = NULL;
    KAPC_STATE ApcState;
    VEASTATUS Status;
    PVOID RemoteAddress;

    if (!Message || !Buffer || BufferSize == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    Status = ObReferenceObjectByHandle(
        PortHandle,
        PORT_CONNECT,
        KpcPortObjectType,
        PreviousMode,
        (PVOID*)&Port
    );

    if (!VEA_SUCCESS(Status)) return Status;

    TargetPort = Port->ConnectedPort;
    if (!TargetPort)
    {
        ObDereferenceObject(Port);
        return STATUS_PORT_DISCONNECTED;
    }

    ClientProcess = TargetPort->CreatorProcess;
    if (!ClientProcess)
    {
        ObDereferenceObject(Port);
        return STATUS_PROCESS_IS_TERMINATING;
    }

    RemoteAddress = (PVOID)((ULONG_PTR)Message->DataInfoOffset + DataOffset);

    // Attach ke Address Space Client untuk menulis
    PtAttachProcess((PKPROCESS)ClientProcess);

    _VEA_TRY
    {
        RtlCopyMemory(RemoteAddress, Buffer, BufferSize);
        if (NumberOfBytesWritten)
        {
            *NumberOfBytesWritten = BufferSize;
        }
        Status = STATUS_SUCCESS;
    }
    _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
    {
        Status = STATUS_ACCESS_VIOLATION;
    }
    _VEA_END;

    PtDetachProcess();

    ObDereferenceObject(Port);
    return Status;
}

VEASTATUS
VEAPI
KpcpReadPort(
    IN HANDLE PortHandle,
    OUT PVOID Buffer,
    IN ULONG Length,
    OUT PULONG BytesRead
)
///
/// Routine Description:
/// Reading Data from Port Node and ensure the Data should be freed
/// from the KPC Pipe if the Data is empty
/// 
/// Parameter:
/// HANDLE PortHandle
///     Handle to port
/// PVOID Buffer
///     Pointer to buffer that filled with KPC data later
/// ULONG Length
///     Data length that should be read
/// PULONG BytesRead
///     Pointer to pass total bytes read
/// 
{
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();
    PKPC_PORT_OBJECT Port = NULL;
    PKPC_MESSAGE_NODE MsgNode = NULL;
    PLIST_ENTRY Entry = NULL;
    KIRQL OldIrql;
    VEASTATUS Status;
    ULONG CopySize = 0;
    ULONG RemainingPayload = 0;

    if (!Buffer || Length == 0 || !BytesRead)
    {
        return STATUS_INVALID_PARAMETER;
    }

    Status = ObReferenceObjectByHandle(
        PortHandle,
        PORT_CONNECT,
        KpcPortObjectType,
        PreviousMode,
        (PVOID*)&Port
    );

    if (!VEA_SUCCESS(Status)) return Status;

    // KpcReadPort khusus didesain untuk KPC_MODE_STREAM
    if (Port->Mode != KPC_MODE_STREAM)
    {
        ObDereferenceObject(Port);
        return STATUS_PORT_TYPES_MISMATCH;
    }

    // Intip Head Queue tanpa langsung menghapus Node
    KsAcquireSpinLock(&Port->QueueLock, &OldIrql);

    if (IsListEmpty(&Port->MessageQueueHead))
    {
        KsReleaseSpinLock(&Port->QueueLock, OldIrql);
        ObDereferenceObject(Port);
        *BytesRead = 0;
        return STATUS_SUCCESS;
    }

    Entry = Port->MessageQueueHead.Flink;
    MsgNode = CONTAINING_RECORD(Entry, KPC_MESSAGE_NODE, QueueListEntry);

    RemainingPayload = MsgNode->Header.DataLength - MsgNode->ReadOffset;
    CopySize = (Length < RemainingPayload) ? Length : RemainingPayload;

    _VEA_TRY
    {
        RtlCopyMemory(Buffer, &MsgNode->MessageData[MsgNode->ReadOffset], CopySize);
        MsgNode->ReadOffset += CopySize;
        *BytesRead = CopySize;
        Status = STATUS_SUCCESS;
    }
    _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
    {
        KsReleaseSpinLock(&Port->QueueLock, OldIrql);
        ObDereferenceObject(Port);
        return STATUS_ACCESS_VIOLATION;
    }
    _VEA_END;

    if (MsgNode->ReadOffset >= MsgNode->Header.DataLength)
    {
        RemoveEntryList(&MsgNode->QueueListEntry);
        UlFreePool(MsgNode);
    }

    KsReleaseSpinLock(&Port->QueueLock, OldIrql);
    ObDereferenceObject(Port);

    return Status;
}

VEASTATUS
VEAPI
VeaReadRequestData(
    IN HANDLE PortHandle,
    IN PKPC_MESSAGE_HEADER Message,
    IN ULONG DataOffset,
    OUT PVOID Buffer,
    IN SIZE_T BufferSize,
    OUT PSIZE_T NumberOfBytesRead OPTIONAL
)
{
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();

    if (PreviousMode == UserMode)
    {
        if (!Message || (ULONG_PTR)Message >= (ULONG)MM_HIGHEST_USER_ADDRESS ||
            !Buffer || (ULONG_PTR)Buffer >= (ULONG)MM_HIGHEST_USER_ADDRESS)
        {
            return STATUS_ACCESS_VIOLATION;
        }
    }

    return KpcpReadRequestData(PortHandle, Message, DataOffset, Buffer, BufferSize, NumberOfBytesRead);
}

VEASTATUS
VEAPI
VeaWriteRequestData(
    IN HANDLE PortHandle,
    IN PKPC_MESSAGE_HEADER Message,
    IN ULONG DataOffset,
    IN PVOID Buffer,
    IN SIZE_T BufferSize,
    OUT PSIZE_T NumberOfBytesWritten OPTIONAL
)
{
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();

    if (PreviousMode == UserMode)
    {
        if (!Message || (ULONG_PTR)Message >= (ULONG)MM_HIGHEST_USER_ADDRESS ||
            !Buffer || (ULONG_PTR)Buffer >= (ULONG)MM_HIGHEST_USER_ADDRESS)
        {
            return STATUS_ACCESS_VIOLATION;
        }
    }

    return KpcpWriteRequestData(PortHandle, Message, DataOffset, Buffer, BufferSize, NumberOfBytesWritten);
}

VEASTATUS
VEAPI
VeaReadPort(
    IN HANDLE PortHandle,
    OUT PVOID Buffer,
    IN ULONG Length,
    OUT PULONG BytesRead
)
{
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();

    if (PreviousMode == UserMode)
    {
        if (!Buffer || (ULONG_PTR)Buffer >= (ULONG)MM_HIGHEST_USER_ADDRESS ||
            !BytesRead || (ULONG_PTR)BytesRead >= (ULONG)MM_HIGHEST_USER_ADDRESS)
        {
            return STATUS_ACCESS_VIOLATION;
        }
    }

    return KpcpReadPort(PortHandle, Buffer, Length, BytesRead);
}

/* EOF */