/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : kpcsend.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Sending message for KPC
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 10-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file kpcsend.c
 * --------------------------------------------------------------------- */

static volatile LONG KpcpNextMessageId = 1;

VEASTATUS
VEAPI
KpcpMapSectionView(
    IN PEPROCESS SenderProcess,
    IN PEPROCESS ReceiverProcess,
    IN OUT PKPC_SECTION_WRITE SectionWrite,
    OUT PKPC_SECTION_READ SectionRead OPTIONAL
)
{
    PVOID SectionObject = NULL;
    PVOID RemoteBase = NULL;
    PVOID LocalBase = NULL;
    SIZE_T ViewSize = 0;
    LARGE_INTEGER SectionOffset;
    VEASTATUS Status;
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();

    if (!SenderProcess || !ReceiverProcess || !SectionWrite)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (PreviousMode == UserMode)
    {
        if ((ULONG_PTR)SectionWrite >= (ULONG)MM_HIGHEST_USER_ADDRESS)
        {
            return STATUS_ACCESS_VIOLATION;
        }

        _VEA_TRY
        {
            ViewSize = SectionWrite->ViewSize;
            SectionOffset.QuadPart = SectionWrite->SectionOffset;
            LocalBase = SectionWrite->ViewBase;
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            return STATUS_ACCESS_VIOLATION;
        }
        _VEA_END;
    }
    else
    {
        ViewSize = SectionWrite->ViewSize;
        SectionOffset.QuadPart = SectionWrite->SectionOffset;
        LocalBase = SectionWrite->ViewBase;
    }

    if (ViewSize == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    Status = ObReferenceObjectByHandle(
        SectionWrite->SectionHandle,
        SECTION_MAP_READ | SECTION_MAP_WRITE,
        MmSectionObjectType,
        PreviousMode,
        &SectionObject
    );

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[KPC/SEC] Failed to reference SectionHandle 0x%p: Status 0x%08X\n", 
               SectionWrite->SectionHandle, Status);
        return Status;
    }

    if (LocalBase == NULL)
    {
        Status = MmMapViewOfSection(
            SectionObject,
            SenderProcess,
            &LocalBase,
            0,
            0,
            &SectionOffset,
            &ViewSize,
            ViewUnmap,
            0,
            PAGE_READWRITE
        );

        if (!VEA_SUCCESS(Status))
        {
            DPRINT("[KPC/SEC] Failed to map view to SenderProcess: Status 0x%08X\n", Status);
            ObDereferenceObject(SectionObject);
            return Status;
        }
    }

    Status = MmMapViewOfSection(
        SectionObject,
        ReceiverProcess,
        &RemoteBase,
        0,
        0,
        &SectionOffset,
        &ViewSize,
        ViewUnmap,
        0,
        PAGE_READWRITE
    );

    ObDereferenceObject(SectionObject);

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[KPC] Failed to map view to ReceiverProcess: Status 0x%08X\n", Status);

        // Cleanup: Jika tadi kita meng-allocate LocalBase baru di Sender, unmap balik agar tidak memory leak
        if (SectionWrite->ViewBase == NULL && LocalBase != NULL)
        {
            MmUnmapViewOfSection(SenderProcess, LocalBase);
        }

        return Status;
    }

    _VEA_TRY
    {
        SectionWrite->ViewBase = LocalBase;
        SectionWrite->ViewRemoteBase = RemoteBase;
        SectionWrite->ViewSize = ViewSize;

        if (SectionRead)
        {
            SectionRead->Length = sizeof(KPC_SECTION_READ);
            SectionRead->ViewSize = ViewSize;
            SectionRead->ViewBase = RemoteBase;
        }
    }
    _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
    {
        MmUnmapViewOfSection(ReceiverProcess, RemoteBase);
        return STATUS_ACCESS_VIOLATION;
    }
    _VEA_END;

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
KpcpUnmapSectionView(
    IN PEPROCESS Process,
    IN PVOID BaseAddress
)
{
    if (!Process || !BaseAddress)
    {
        return STATUS_INVALID_PARAMETER;
    }

    return MmUnmapViewOfSection(Process, BaseAddress);
}

VEASTATUS
VEAPI
KpcpSendMessage(
    IN HANDLE PortHandle,
    IN PKPC_MESSAGE_HEADER RequestMessage,
    IN PKPC_SECTION_WRITE SectionWrite OPTIONAL,
    IN BOOLEAN WaitReply,
    OUT PKPC_MESSAGE_HEADER ReplyMessage OPTIONAL
)
{
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();
    PKPC_PORT_OBJECT Port = NULL;
    PKPC_PORT_OBJECT TargetPort = NULL;
    PKPC_MESSAGE_NODE MsgNode = NULL;
    KIRQL OldIrql;
    VEASTATUS Status;
    ULONG PayloadSize;
    ULONG NodeSize;
    ULONG GeneratedId;

    if (!RequestMessage) return STATUS_INVALID_PARAMETER_2;

    //
    // Reference the Port and check wether the port still VALID or has been
    // closed
    //

    Status = ObReferenceObjectByHandle(
        PortHandle,
        PORT_CONNECT,
        KpcPortObjectType,
        PreviousMode,
        (PVOID*)&Port
    );

    if (!VEA_SUCCESS(Status)) return Status;

    if (Port->Type != KpcServerCommunicationPort && 
        Port->Type != KpcClientCommunicationPort)
    {
        ObDereferenceObject(Port);
        return STATUS_PORT_DISCONNECTED;
    }

    TargetPort = Port->ConnectedPort;
    if (!TargetPort)
    {
        ObDereferenceObject(Port);
        return STATUS_PORT_DISCONNECTED;
    }

    //
    // The message should valid in this condition:
    // 1. Message is exist
    // 2. Message length is not exceeded capability of port maximum size
    // 
    // Allocating the message and then initialize the message node, copy the 
    // message
    //

    if (RequestMessage->TotalLength > TargetPort->MaxMessageLength ||
        RequestMessage->TotalLength < sizeof(KPC_MESSAGE_HEADER))
    {
        ObDereferenceObject(Port);
        return STATUS_INVALID_PARAMETER;
    }

    PayloadSize = RequestMessage->TotalLength - sizeof(KPC_MESSAGE_HEADER);
    NodeSize = sizeof(KPC_MESSAGE_NODE) + PayloadSize;

    MsgNode = (PKPC_MESSAGE_NODE)UlAllocatePoolWithTag(
        NonPagedPool,
        NodeSize,
        TAG_KPC_MESSAGE
    );

    if (!MsgNode)
    {
        ObDereferenceObject(Port);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(MsgNode, NodeSize);

    GeneratedId = (ULONG)InterlockedIncrement((PLONG)&KpcpNextMessageId);
    MsgNode->RequestingThread = PtGetCurrentThread();
    MsgNode->ReadOffset = 0;
    MsgNode->ReplyBuffer = ReplyMessage;
    MsgNode->Header.MessageId = GeneratedId;
    
    RequestMessage->MessageId = GeneratedId;

    RtlCopyMemory(&MsgNode->Header, RequestMessage, RequestMessage->TotalLength);
    MsgNode->Header.ClientId.UniqueProcess = ((PEPROCESS)PtGetCurrentProcess())->UniqueProcessId;
    MsgNode->Header.ClientId.UniqueThread = ((PETHREAD)PtGetCurrentThread())->Cid.UniqueThread;

    //
    // If section write is exist and mentioned, we should copy view them
    //

    if (SectionWrite != NULL && RequestMessage->DataInfoOffset != 0)
    {
        KPC_SECTION_READ RemoteSectionRead;
        
        Status = KpcpMapSectionView(
            (PEPROCESS)PtGetCurrentProcess(),// LOCAL
            TargetPort->CreatorProcess,    // THE REMOTE
            SectionWrite,
            &RemoteSectionRead
        );

        if (!VEA_SUCCESS(Status))
        {
            UlFreePool(MsgNode);
            ObDereferenceObject(Port);
            return Status;
        }
    }

    //
    // We can safely enqueue the message and then notify the other side using SEMAPHORE.
    // If no waiting for Reply, we can return instead
    //

    KsAcquireSpinLock(&TargetPort->QueueLock, &OldIrql);
    InsertTailList(&TargetPort->MessageQueueHead, &MsgNode->QueueListEntry);
    KsReleaseSpinLock(&TargetPort->QueueLock, OldIrql);

    KsReleaseSemaphore(
        &TargetPort->MesssageSemaphore,
        1,
        FALSE
    );

    ObDereferenceObject(Port);

    if (!WaitReply)
    {
        return STATUS_SUCCESS;
    }

    Status = KsWaitForSingleObject(
        &((PETHREAD)PtGetCurrentThread())->KpcReplySemaphore
    );

    return Status;
}

VEASTATUS
VEAPI
VeaRequestWaitReplyPort(
    IN HANDLE PortHandle,
    IN PKPC_MESSAGE_HEADER RequestMessage,
    OUT PKPC_MESSAGE_HEADER ReplyMessage OPTIONAL
)
{
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();
    KPC_MESSAGE_HEADER CapturedHeader;
    VEASTATUS Status;

    if (PreviousMode == UserMode)
    {
        if (!RequestMessage || (ULONG_PTR)RequestMessage >= (ULONG)MM_HIGHEST_USER_ADDRESS)
        {
            return STATUS_ACCESS_VIOLATION;
        }

        _VEA_TRY
        {
            // Probe penulisan/pembacaan buffer header dari User Space
            RtlCopyMemory(&CapturedHeader, RequestMessage, sizeof(KPC_MESSAGE_HEADER));

            if (ReplyMessage)
            {
                if ((ULONG_PTR)ReplyMessage >= (ULONG)MM_HIGHEST_USER_ADDRESS)
                {
                    _VEA_YIELD(return STATUS_ACCESS_VIOLATION;);
                }
                *(volatile UCHAR*)ReplyMessage = *(volatile UCHAR*)ReplyMessage;
            }
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            return STATUS_ACCESS_VIOLATION;
        }
        _VEA_END;
    }
    else
    {
        RtlCopyMemory(&CapturedHeader, RequestMessage, sizeof(KPC_MESSAGE_HEADER));
    }

    // Panggil Engine Kernel (WaitReply = TRUE)
    Status = KpcpSendMessage(
        PortHandle,
        RequestMessage,
        NULL,
        TRUE,
        ReplyMessage
    );

    return Status;
}

/* EOF */