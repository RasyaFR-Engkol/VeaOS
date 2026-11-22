/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : kpcconnect.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : KPC Connect Method
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 10-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file kpcconnect.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
KpcpConnectPort(
    OUT PHANDLE ClientPortHandle,
    IN PANSI_STRING PortName,
    IN PSECURITY_QUALITY_OF_SERVICE SecurityQos OPTIONAL,
    IN OUT PKPC_SECTION_WRITE WriteSection OPTIONAL,
    IN OUT PKPC_SECTION_READ ReadSection OPTIONAL,
    IN KPC_PORT_MODE RequestedMode,
    IN PVOID ConnectionInformation OPTIONAL,
    IN OUT PULONG ConnectionInformationLength OPTIONAL
)
{
    PKPC_PORT_OBJECT ServerPort = NULL;
    PKPC_PORT_OBJECT ClientCommPort = NULL;
    PKPC_PORT_OBJECT ServerCommPort = NULL;
    PKPC_MESSAGE_NODE ConnectNode = NULL;
    HANDLE hClientPort = NULL;
    HANDLE hServerPort = NULL;
    KIRQL OldIrql;
    VEASTATUS Status;
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();
    OBJECT_ATTRIBUTES ObjAttr;

    UNREFERENCED_PARAMETER(SecurityQos);
    UNREFERENCED_PARAMETER(WriteSection);
    UNREFERENCED_PARAMETER(ReadSection);
    UNREFERENCED_PARAMETER(ConnectionInformation);
    UNREFERENCED_PARAMETER(ConnectionInformationLength);

    InitializeObjectAttributes(&ObjAttr, PortName, OBJ_CASE_INSENSITIVE, NULL, NULL);

    Status = ObReferenceObjectByName(
        &ObjAttr,
        KpcPortObjectType,
        (PVOID*)&ServerPort
    );

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[KPC/CONN] Failed to reference ServerPort by name: 0x%08X\n", Status);
        return Status;
    }

    if (ServerPort->Mode != RequestedMode)
    {
        DPRINT("[KPC/CONN] Mode Mismatch! Server: %u, Requested: %u\n", ServerPort->Mode, RequestedMode);
        ObDereferenceObject(ServerPort);
        return STATUS_PORT_TYPES_MISMATCH;
    }

    Status = ObCreateObject(
        PreviousMode, KpcPortObjectType, NULL, PreviousMode,
        NULL, sizeof(KPC_PORT_OBJECT), 0, 0, (PVOID*)&ClientCommPort
    );
    if (!VEA_SUCCESS(Status))
    {
        ObDereferenceObject(ServerPort);
        return Status;
    }

    Status = ObCreateObject(
        PreviousMode, KpcPortObjectType, NULL, PreviousMode,
        NULL, sizeof(KPC_PORT_OBJECT), 0, 0, (PVOID*)&ServerCommPort
    );
    if (!VEA_SUCCESS(Status))
    {
        ObDereferenceObject(ClientCommPort);
        ObDereferenceObject(ServerPort);
        return Status;
    }

    ClientCommPort->Type = KpcClientCommunicationPort;
    ClientCommPort->Mode = RequestedMode;
    ClientCommPort->ConnectedPort = ServerCommPort;
    ClientCommPort->CreatorProcess = (PEPROCESS)PtGetCurrentProcess();
    ClientCommPort->MaxMessageLength = ServerPort->MaxMessageLength;
    ClientCommPort->MaxConnectionInfoLength = ServerPort->MaxConnectionInfoLength;
    KsInitializeSpinlock(&ClientCommPort->QueueLock);
    InitializeListHead(&ClientCommPort->MessageQueueHead);
    InitializeListHead(&ClientCommPort->LpcReplyChain);
    KsInitializeSemaphore(&ClientCommPort->MesssageSemaphore, 0, 0x7FFFFFFF);

    ServerCommPort->Type = KpcServerCommunicationPort;
    ServerCommPort->Mode = RequestedMode;
    ServerCommPort->ConnectedPort = ClientCommPort;
    ServerCommPort->CreatorProcess = ServerPort->CreatorProcess;
    ServerCommPort->MaxMessageLength = ServerPort->MaxMessageLength;
    ServerCommPort->MaxConnectionInfoLength = ServerPort->MaxConnectionInfoLength;
    KsInitializeSpinlock(&ServerCommPort->QueueLock);
    InitializeListHead(&ServerCommPort->MessageQueueHead);
    InitializeListHead(&ServerCommPort->LpcReplyChain);
    KsInitializeSemaphore(&ServerCommPort->MesssageSemaphore, 0, 0x7FFFFFFF);

    Status = ObInsertObject(ClientCommPort, NULL, PORT_ALL_ACCESS, 0, NULL, &hClientPort);
    if (!VEA_SUCCESS(Status))
    {
        ObDereferenceObject(ClientCommPort);
        ObDereferenceObject(ServerCommPort);
        ObDereferenceObject(ServerPort);
        return Status;
    }

    ConnectNode = (PKPC_MESSAGE_NODE)UlAllocatePoolWithTag(
        NonPagedPool,
        sizeof(KPC_MESSAGE_NODE),
        TAG_KPC_MESSAGE
    );

    if (!ConnectNode)
    {
        VzClose(hClientPort);
        ObDereferenceObject(ServerPort);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(ConnectNode, sizeof(KPC_MESSAGE_NODE));
    ConnectNode->Header.TotalLength = sizeof(KPC_MESSAGE_HEADER);
    ConnectNode->Header.MessageType = KPC_CONNECTION_REQUEST;
    ConnectNode->Header.MessageId = (ULONG)(ULONG_PTR)ServerCommPort;
    ConnectNode->Header.ClientId.UniqueProcess = ((PEPROCESS)PtGetCurrentProcess())->UniqueProcessId;
    ConnectNode->Header.ClientId.UniqueThread = ((PETHREAD)PtGetCurrentThread())->Cid.UniqueThread;

    KsAcquireSpinLock(&ServerPort->QueueLock, &OldIrql);
    *ClientPortHandle = hClientPort;
    InsertTailList(&ServerPort->MessageQueueHead, &ConnectNode->QueueListEntry);
    KsReleaseSpinLock(&ServerPort->QueueLock, OldIrql);

    KsReleaseSemaphore(
        &ServerPort->MesssageSemaphore,
        1,
        FALSE
    );

    *ClientPortHandle = hClientPort;
    ObDereferenceObject(ServerPort);

    DPRINT("[KPC/CONN] Connection request enqueued to Listen Port! ClientHandle: %p, ServerCommHandle: %p\n",
           hClientPort, hServerPort);

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
VeaConnectPort(
    OUT PHANDLE ClientPortHandle,
    IN PANSI_STRING PortName,
    IN PSECURITY_QUALITY_OF_SERVICE SecurityQos OPTIONAL,
    IN OUT PKPC_SECTION_WRITE WriteSection OPTIONAL,
    IN OUT PKPC_SECTION_READ ReadSection OPTIONAL,
    IN KPC_PORT_MODE RequestedMode,
    IN PVOID ConnectionInformation OPTIONAL,
    IN OUT PULONG ConnectionInformationLength OPTIONAL
)
{
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();

    if (PreviousMode == UserMode)
    {
        if (!ClientPortHandle || (ULONG_PTR)ClientPortHandle >= (ULONG)MM_HIGHEST_USER_ADDRESS)
        {
            return STATUS_ACCESS_VIOLATION;
        }

        _VEA_TRY
        {
            *(volatile HANDLE*)ClientPortHandle = *(volatile HANDLE*)ClientPortHandle;
            
            if (!PortName || (ULONG_PTR)PortName >= (ULONG)MM_HIGHEST_USER_ADDRESS)
            {
                _VEA_YIELD(return STATUS_ACCESS_VIOLATION;);
            }
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            return STATUS_ACCESS_VIOLATION;
        }
        _VEA_END;
    }

    // 2. Serahkan eksekusi pembuatan port ke KpcpConnectPort
    return KpcpConnectPort(
        ClientPortHandle,
        PortName,
        SecurityQos,
        WriteSection,
        ReadSection,
        RequestedMode,
        ConnectionInformation,
        ConnectionInformationLength
    );
}

VEASTATUS
VEAPI
KpcpAcceptConnectPort(
    OUT PHANDLE ServerPortHandle,
    IN PVOID Context OPTIONAL,
    IN PKPC_MESSAGE_HEADER Message,
    IN BOOLEAN AcceptConnection,
    IN OUT PKPC_SECTION_WRITE WriteSection OPTIONAL,
    IN OUT PKPC_SECTION_READ ReadSection OPTIONAL
)
{
    PKPC_PORT_OBJECT ServerCommPort = NULL;
    HANDLE hServerPort = NULL;
    VEASTATUS Status;
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();

    UNREFERENCED_PARAMETER(Context);
    UNREFERENCED_PARAMETER(WriteSection);
    UNREFERENCED_PARAMETER(ReadSection);

    if (!Message || !ServerPortHandle)
    {
        return STATUS_INVALID_PARAMETER;
    }

    // 1. Ambil Pointer Objek ServerCommPort dari MessageId (hasil simpan KpcpConnectPort)
    ServerCommPort = (PKPC_PORT_OBJECT)(ULONG_PTR)Message->MessageId;

    if (!ServerCommPort || ServerCommPort->Type != KpcServerCommunicationPort)
    {
        DPRINT("[KPC/ACCEPT] ERROR: Invalid ServerCommPort object pointer!\n");
        return STATUS_INVALID_PORT_HANDLE;
    }

    // 2. Jika Server Menolak Koneksi (AcceptConnection == FALSE)
    if (!AcceptConnection)
    {
        DPRINT("[KPC/ACCEPT] Server rejected connection request.\n");

        if (ServerCommPort->ConnectedPort)
        {
            ServerCommPort->ConnectedPort->ConnectedPort = NULL;
        }
        
        ObDereferenceObject(ServerCommPort);
        return STATUS_PORT_DISCONNECTED;
    }

    // 3. Masukkan ServerCommPort ke Handle Table milik SERVER PROCESS yang sedang jalan!
    Status = ObInsertObject(
        ServerCommPort,
        NULL,
        PORT_ALL_ACCESS,
        0,
        NULL,
        &hServerPort
    );

    if (VEA_SUCCESS(Status))
    {
        *ServerPortHandle = hServerPort;
        DPRINT("[KPC/ACCEPT] Connection accepted! ServerCommHandle created: %p\n", hServerPort);
    }
    else
    {
        DPRINT("[KPC/ACCEPT] Failed to insert ServerCommPort to handle table: 0x%08X\n", Status);
    }

    return Status;
}

VEASTATUS
VEAPI
VeaAcceptConnectPort(
    OUT PHANDLE ServerPortHandle,
    IN PVOID Context OPTIONAL,
    IN PKPC_MESSAGE_HEADER Message,
    IN BOOLEAN AcceptConnection,
    IN OUT PKPC_SECTION_WRITE WriteSection OPTIONAL,
    IN OUT PKPC_SECTION_READ ReadSection OPTIONAL
)
{
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();

    if (PreviousMode == UserMode)
    {
        if (!ServerPortHandle || (ULONG_PTR)ServerPortHandle >= (ULONG)MM_HIGHEST_USER_ADDRESS)
        {
            return STATUS_ACCESS_VIOLATION;
        }

        if (!Message || (ULONG_PTR)Message >= (ULONG)MM_HIGHEST_USER_ADDRESS)
        {
            return STATUS_ACCESS_VIOLATION;
        }

        _VEA_TRY
        {
            *(volatile HANDLE*)ServerPortHandle = *(volatile HANDLE*)ServerPortHandle;
            *(volatile UCHAR*)Message = *(volatile UCHAR*)Message;
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            return STATUS_ACCESS_VIOLATION;
        }
        _VEA_END;
    }

    return KpcpAcceptConnectPort(
        ServerPortHandle,
        Context,
        Message,
        AcceptConnection,
        WriteSection,
        ReadSection
    );
}
