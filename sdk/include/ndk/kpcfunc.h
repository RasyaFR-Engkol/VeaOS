// VEAOS KPC FUNCTION
#pragma once

#include <procbind.h>
#include <kpctypes.h>

BOOLEAN
VEAPI
KpcInitSystem(VOID);

VEASTATUS
VEAPI
KpcpCreatePort(
    OUT PHANDLE PortHandle,
    IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL,
    IN ULONG MaxConnectionInfoLength,
    IN ULONG MaxMessageLength,
    IN KPC_PORT_MODE Mode
);

VEASTATUS
VEAPI
VeaCreatePort(
    OUT PHANDLE PortHandle,
    IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL,
    IN ULONG MaxConnectionInfoLength,
    IN ULONG MaxMessageLength,
    IN KPC_PORT_MODE Mode
);

VEASTATUS
VEAPI
VeaRequestWaitReplyPort(
    IN HANDLE PortHandle,
    IN PKPC_MESSAGE_HEADER RequestMessage,
    OUT PKPC_MESSAGE_HEADER ReplyMessage OPTIONAL
);

VEASTATUS
VEAPI
KpcpUnmapSectionView(
    IN PEPROCESS Process,
    IN PVOID BaseAddress
);

VEASTATUS
VEAPI
KpcpMapSectionView(
    IN PEPROCESS SenderProcess,
    IN PEPROCESS ReceiverProcess,
    IN OUT PKPC_SECTION_WRITE SectionWrite,
    OUT PKPC_SECTION_READ SectionRead OPTIONAL
);

VEASTATUS
VEAPI
KpcpSendMessage(
    IN HANDLE PortHandle,
    IN PKPC_MESSAGE_HEADER RequestMessage,
    IN PKPC_SECTION_WRITE SectionWrite OPTIONAL,
    IN BOOLEAN WaitReply,
    OUT PKPC_MESSAGE_HEADER ReplyMessage OPTIONAL
);

VEASTATUS
VEAPI
VeaReplyWaitReceivePort(
    IN HANDLE PortHandle,
    OUT PVOID Context OPTIONAL,
    IN PKPC_MESSAGE_HEADER ReplyMessage OPTIONAL,
    OUT PKPC_MESSAGE_HEADER ReceiveMessage
);

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
);

VEASTATUS
VEAPI
VeaAcceptConnectPort(
    OUT PHANDLE ServerPortHandle,
    IN PVOID Context OPTIONAL,
    IN PKPC_MESSAGE_HEADER Message,
    IN BOOLEAN AcceptConnection,
    IN OUT PKPC_SECTION_WRITE WriteSection OPTIONAL,
    IN OUT PKPC_SECTION_READ ReadSection OPTIONAL
);

VEASTATUS
VEAPI
KpcpReplyPort(
    IN HANDLE PortHandle,
    IN PKPC_MESSAGE_HEADER ReplyMessage
);

VEASTATUS
VEAPI
VeaReadRequestData(
    IN HANDLE PortHandle,
    IN PKPC_MESSAGE_HEADER Message,
    IN ULONG DataOffset,
    OUT PVOID Buffer,
    IN SIZE_T BufferSize,
    OUT PSIZE_T NumberOfBytesRead OPTIONAL
);

VEASTATUS
VEAPI
VeaWriteRequestData(
    IN HANDLE PortHandle,
    IN PKPC_MESSAGE_HEADER Message,
    IN ULONG DataOffset,
    IN PVOID Buffer,
    IN SIZE_T BufferSize,
    OUT PSIZE_T NumberOfBytesWritten OPTIONAL
);

VEASTATUS
VEAPI
VeaReadPort(
    IN HANDLE PortHandle,
    OUT PVOID Buffer,
    IN ULONG Length,
    OUT PULONG BytesRead
);

/* EOF */