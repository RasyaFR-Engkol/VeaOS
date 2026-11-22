#pragma once

#include "pttypes.h"
#include <procbind.h>
#include <veastatus.h>
#include <rtl.h>
#include <vz_wrappers.h>

#define CSR_MAX_MSG_LEN         256
#define CSR_MAX_CONN_INFO       64
#define CSR_MAX_PROCESSES       128
#define CSR_MAX_SERVER_DLLS     4

#define CSR_SERVER_BASE         0   /* basesrv */
#define CSR_SERVER_WIN          1   /* winsrv  */

#define CSR_MAKE_API_NUMBER(ServerId, FuncId) \
    ((ULONG)(((ServerId) << 16) | ((FuncId) & 0xFFFF)))

#define CSR_GET_SERVER_ID(ApiNumber) \
    ((USHORT)(((ApiNumber) >> 16) & 0xFFFF))

#define CSR_GET_FUNCTION_ID(ApiNumber) \
    ((USHORT)((ApiNumber) & 0xFFFF))

typedef struct _CSR_KPC_MESSAGE {
    KPC_MESSAGE_HEADER Header;
    ULONG ApiNumber;
    VEASTATUS Status;
    UCHAR Data[192];
} CSR_KPC_MESSAGE, *PCSR_KPC_MESSAGE;

typedef struct _CSR_PROCESS {
    BOOLEAN InUse;
    HANDLE ProcessId;
    HANDLE ClientCommPort;
    ULONG Flags;
    ULONG ShutdownLevel;
} CSR_PROCESS, *PCSR_PROCESS;

typedef VEASTATUS (VEAPI *PCSR_API_ROUTINE)(
    IN PCSR_PROCESS Process,
    IN OUT PCSR_KPC_MESSAGE Request,
    OUT PBOOLEAN Reply
);

typedef VOID (VEAPI *PCSR_DISCONNECT_ROUTINE)(
    IN PCSR_PROCESS Process
);

typedef struct _CSR_SERVER_DLL {
    USHORT ServerId;
    CONST CHAR *Name;
    CONST PCSR_API_ROUTINE *ApiTable;
    ULONG ApiCount;
    PCSR_DISCONNECT_ROUTINE DisconnectRoutine;
} CSR_SERVER_DLL, *PCSR_SERVER_DLL;

extern
VEASTATUS
VEAPI
VeaUserInitialize(VOID);

/* csrsrv.c */
VEASTATUS VEAPI CsrServerInitialization(VOID);
VEASTATUS VEAPI CsrRegisterServerDll(IN PCSR_SERVER_DLL ServerDll);
PCSR_PROCESS VEAPI CsrLocateProcessById(IN HANDLE ProcessId);
VOID VEAPI CsrRunSbSessionLoop(VOID);

/* basesrv.c & winsrv.c */
VEASTATUS VEAPI BaseServerDllInitialization(VOID);
VEASTATUS VEAPI WinServerDllInitialization(VOID);