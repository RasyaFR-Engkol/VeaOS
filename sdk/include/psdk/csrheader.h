#pragma once

#include <kpctypes.h>

#ifndef CSR_MAX_CONN_INFO
#define CSR_MAX_CONN_INFO   256
#endif

#ifndef CSR_MAX_MSG_LEN
#define CSR_MAX_MSG_LEN     256
#endif

typedef struct _CSR_KPC_MESSAGE {
    KPC_MESSAGE_HEADER Header;
    ULONG ApiNumber;
    VEASTATUS Status;
    UCHAR Data[CSR_MAX_MSG_LEN - sizeof(KPC_MESSAGE_HEADER) - sizeof(ULONG) - sizeof(VEASTATUS)];
} CSR_KPC_MESSAGE, *PCSR_KPC_MESSAGE;

typedef struct _BASE_CREATE_PROCESS_MSG {
    HANDLE ProcessHandle;
    HANDLE ThreadHandle;
    CLIENT_ID ClientId;
    ULONG CreationFlags;
} BASE_CREATE_PROCESS_MSG, *PBASE_CREATE_PROCESS_MSG;

typedef struct _BASE_CREATE_THREAD_MSG {
    HANDLE ThreadHandle;
    CLIENT_ID ClientId;
} BASE_CREATE_THREAD_MSG, *PBASE_CREATE_THREAD_MSG;

typedef struct _BASE_EXIT_PROCESS_MSG {
    ULONG ExitCode;
} BASE_EXIT_PROCESS_MSG, *PBASE_EXIT_PROCESS_MSG;

VEASTATUS
VEAPI
CsrClientCallServer(
    IN OUT PKPC_MESSAGE_HEADER ApiMessage,
    IN ULONG ApiNumber,
    IN ULONG DataLength
);

#define CSR_MAKE_API_NUMBER(ServerId, FuncId) \
    ((ULONG)(((ServerId) << 16) | ((FuncId) & 0xFFFF)))

#define CSR_SERVER_BASE         0   /* basesrv */
#define CSR_SERVER_WIN          1   /* winsrv  */

typedef enum _BASE_MESSAGE_TYPE {
    BasepCreateProcess = 0,
    BasepCreateThread  = 1,
    BasepExitProcess   = 2,
    BasepMaxApiNumber
} BASE_MESSAGE_TYPE;

VEASTATUS
VEAPI
CsrClientConnectToServer(
    IN PCSTR ObjectDirectory OPTIONAL,
    IN ULONG ServerId,
    IN PVOID ConnectionInformation OPTIONAL,
    IN OUT PULONG ConnectionInformationLength OPTIONAL,
    OUT PBOOLEAN ServerToServer OPTIONAL
);

/* EOF */