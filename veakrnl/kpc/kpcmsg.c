/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : kpcmsg.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : KPC Waiting Message
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 10-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file kpcmsg.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
KpcpReplyWaitReceivePort(
    IN HANDLE PortHandle,
    OUT PVOID Context OPTIONAL,
    IN PKPC_MESSAGE_HEADER ReplyMessage OPTIONAL,
    OUT PKPC_MESSAGE_HEADER ReceiveMessage
)
{
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();
    PKPC_PORT_OBJECT Port = NULL;
    PKPC_MESSAGE_NODE MsgNode = NULL;
    PLIST_ENTRY Entry = NULL;
    KIRQL OldIrql;
    VEASTATUS Status;

    UNREFERENCED_PARAMETER(Context);

    Status = ObReferenceObjectByHandle(
        PortHandle,
        PORT_CONNECT,
        KpcPortObjectType,
        PreviousMode,
        (PVOID*)&Port
    );

    if (!VEA_SUCCESS(Status)) return Status;

    // 1. Jika ada pesan balasan yang dibawa, proses reply dulu!
    if (ReplyMessage != NULL)
    {
        KpcpReplyPort(PortHandle, ReplyMessage);
    }
    
    // 2. Tidur sampai ada pesan masuk
    Status = KsWaitForSingleObject(&Port->MesssageSemaphore);
    if (!VEA_SUCCESS(Status))
    {
        ObDereferenceObject(Port);
        return Status;
    }

    // 3. Ambil Node Pesan dari Antrean Port
    KsAcquireSpinLock(&Port->QueueLock, &OldIrql);
    
    if (!IsListEmpty(&Port->MessageQueueHead))
    {
        Entry = RemoveHeadList(&Port->MessageQueueHead);
        MsgNode = CONTAINING_RECORD(Entry, KPC_MESSAGE_NODE, QueueListEntry);
    }

    if (!MsgNode)
    {
        KsReleaseSpinLock(&Port->QueueLock, OldIrql);
        ObDereferenceObject(Port);
        return STATUS_UNSUCCESSFUL;
    }

    // 4. Salin isi Header ke ReceiveMessage
    _VEA_TRY
    {
        RtlCopyMemory(ReceiveMessage, &MsgNode->Header, MsgNode->Header.TotalLength);
    }
    _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
    {
        InsertHeadList(&Port->MessageQueueHead, &MsgNode->QueueListEntry);
        KsReleaseSpinLock(&Port->QueueLock, OldIrql);
        ObDereferenceObject(Port);
        return STATUS_ACCESS_VIOLATION;
    }
    _VEA_END;

    // 5. MANAJEMEN MEMORI NODE:
    // Jika Client menanti balasan (ReplyBuffer != NULL), simpan node ke LpcReplyChain!
    // Jika Datagram biasa, langsung free dari pool.
    if (MsgNode->ReplyBuffer != NULL)
    {
        InsertTailList(&Port->LpcReplyChain, &MsgNode->QueueListEntry);
        KsReleaseSpinLock(&Port->QueueLock, OldIrql);
    }
    else
    {
        KsReleaseSpinLock(&Port->QueueLock, OldIrql);
        UlFreePool(MsgNode);
    }

    ObDereferenceObject(Port);
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
VeaReplyWaitReceivePort(
    IN HANDLE PortHandle,
    OUT PVOID Context OPTIONAL,
    IN PKPC_MESSAGE_HEADER ReplyMessage OPTIONAL,
    OUT PKPC_MESSAGE_HEADER ReceiveMessage
)
{
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();

    if (PreviousMode == UserMode)
    {
        if (!ReceiveMessage || (ULONG_PTR)ReceiveMessage >= (ULONG)MM_HIGHEST_USER_ADDRESS)
        {
            return STATUS_ACCESS_VIOLATION;
        }

        _VEA_TRY
        {
            *(volatile UCHAR*)ReceiveMessage = *(volatile UCHAR*)ReceiveMessage;
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            return STATUS_ACCESS_VIOLATION;
        }
        _VEA_END;
    }

    return KpcpReplyWaitReceivePort(
        PortHandle,
        Context,
        ReplyMessage,
        ReceiveMessage
    );
}

VEASTATUS
VEAPI
KpcpReplyPort(
    IN HANDLE PortHandle,
    IN PKPC_MESSAGE_HEADER ReplyMessage
)
{
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();
    PKPC_PORT_OBJECT Port = NULL;
    PKPC_PORT_OBJECT TargetPort = NULL;
    PKPC_MESSAGE_NODE MsgNode = NULL;
    PLIST_ENTRY Entry = NULL;
    KIRQL OldIrql;
    VEASTATUS Status;

    if (!ReplyMessage) return STATUS_INVALID_PARAMETER;

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

    // Cari node pesan di LpcReplyChain milik Port yang cocok dengan MessageId
    KsAcquireSpinLock(&Port->QueueLock, &OldIrql);

    for (Entry = Port->LpcReplyChain.Flink;
         Entry != &Port->LpcReplyChain;
         Entry = Entry->Flink)
    {
        PKPC_MESSAGE_NODE Candidate = CONTAINING_RECORD(Entry, KPC_MESSAGE_NODE, QueueListEntry);
        
        if (Candidate->Header.MessageId == ReplyMessage->MessageId)
        {
            MsgNode = Candidate;
            RemoveEntryList(&Candidate->QueueListEntry);
            break;
        }
    }

    KsReleaseSpinLock(&Port->QueueLock, OldIrql);

    if (!MsgNode)
    {
        DPRINT("[KPC/REPLY] ERROR: No pending reply node found for MessageId %u!\n", ReplyMessage->MessageId);
        ObDereferenceObject(Port);
        return STATUS_INVALID_PARAMETER; 
    }

    PVOID KernelReplyCopy = NULL;
    ULONG ReplyLen = ReplyMessage->TotalLength;
    PEPROCESS ClientProcess = MsgNode->RequestingThread
        ? ((PETHREAD)MsgNode->RequestingThread)->ThreadProcess
        : TargetPort->CreatorProcess;

    if (MsgNode->ReplyBuffer && ReplyLen > 0)
    {
        KernelReplyCopy = UlAllocatePoolWithTag(NonPagedPool, ReplyLen, TAG_KPC_MESSAGE);
        if (!KernelReplyCopy)
        {
            UlFreePool(MsgNode);
            ObDereferenceObject(Port);
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        _VEA_TRY
        {
            RtlCopyMemory(KernelReplyCopy, ReplyMessage, ReplyLen);
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            UlFreePool(KernelReplyCopy);
            UlFreePool(MsgNode);
            ObDereferenceObject(Port);
            return STATUS_ACCESS_VIOLATION;
        }
        _VEA_END;

        if (ClientProcess && ClientProcess != (PEPROCESS)PtGetCurrentProcess())
        {
            PtAttachProcess((PKPROCESS)ClientProcess);
        }

        _VEA_TRY
        {
            RtlCopyMemory(MsgNode->ReplyBuffer, KernelReplyCopy, ReplyLen);
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            if (ClientProcess && ClientProcess != (PEPROCESS)PtGetCurrentProcess())
                PtDetachProcess();
            UlFreePool(KernelReplyCopy);
            UlFreePool(MsgNode);
            ObDereferenceObject(Port);
            return STATUS_ACCESS_VIOLATION;
        }
        _VEA_END;

        if (ClientProcess && ClientProcess != (PEPROCESS)PtGetCurrentProcess())
        {
            PtDetachProcess();
        }

        UlFreePool(KernelReplyCopy);
    }

    // 2. BANGUNIN CLIENT THREAD VIA SEMAPHORE THREAD!
    if (MsgNode->RequestingThread)
    {
        KsReleaseSemaphore(
            &((PETHREAD)MsgNode->RequestingThread)->KpcReplySemaphore,
            1,
            FALSE
        );
    }

    // 3. Hapus node dari NonPagedPool setelah balasan terkirim sempurna
    UlFreePool(MsgNode);
    ObDereferenceObject(Port);

    return STATUS_SUCCESS;
}

/* EOF */