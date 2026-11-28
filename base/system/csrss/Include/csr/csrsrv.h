#pragma once

#define CSR_PROCESS_TERMINATED      0x00000001
#define CSR_PROCESS_CONSOLEAPP      0x00000002
#define CSR_PROCESS_GUIAPP          0x00000004
#define CSR_PROCESS_SHUTDOWN_SKIP   0x00000008

typedef struct _CSR_PROCESS {
    LIST_ENTRY ListLink;
    LIST_ENTRY ThreadList;         
    HANDLE ProcessId;
    HANDLE ParentProcessId;
    HANDLE ProcessHandle;   
    HANDLE ClientCommPort;
    ULONG CreationFlags;
    ULONG Flags;
    ULONG ShutdownLevel;
    ULONG ExitCode;
    LONG ReferenceCount;
    PVOID *ServerData;        
} CSR_PROCESS, *PCSR_PROCESS;

typedef struct _CSR_THREAD {
    LIST_ENTRY Link;
    PCSR_PROCESS Process;
    CLIENT_ID ClientId;
    HANDLE ThreadHandle;
    ULONG Flags;
} CSR_THREAD, *PCSR_THREAD;

typedef VEASTATUS (VEAPI *PCSR_API_ROUTINE)(
    IN PCSR_PROCESS Process,
    IN OUT PCSR_KPC_MESSAGE Request,
    OUT PBOOLEAN Reply
);

typedef VOID (VEAPI *PCSR_DISCONNECT_ROUTINE)(
    IN PCSR_PROCESS Process
);

typedef struct _CSR_SERVER_DLL {
    LIST_ENTRY ListLink;
    USHORT ServerId;
    CONST CHAR *Name;
    CONST PCSR_API_ROUTINE *ApiTable;
    ULONG ApiCount;
    PCSR_DISCONNECT_ROUTINE DisconnectRoutine;
} CSR_SERVER_DLL, *PCSR_SERVER_DLL;

extern HANDLE hVeaApiPort;
extern HANDLE hSbApiPort;
extern HANDLE hSmApiPort;
extern ULONG gServerDllCapacity;
extern LIST_ENTRY gProcessListHead;
extern PCSR_SERVER_DLL *gServerDlls;

/* csrsrv.c */
VEASTATUS VEAPI CsrServerInitialization(VOID);
VEASTATUS VEAPI CsrRegisterServerDll(IN PCSR_SERVER_DLL ServerDll);
PCSR_PROCESS VEAPI CsrLocateProcessById(IN HANDLE ProcessId);
VOID VEAPI CsrRunSbSessionLoop(VOID);

VOID
VEAPI
CsrApiRequestThread(
    IN PVOID Parameter
);

PCSR_PROCESS
VEAPI
CsrpAllocateProcess(
    IN HANDLE ProcessId,
    IN HANDLE ClientCommPort
);

VOID
VEAPI
CsrpRemoveProcess(
    IN HANDLE ProcessId
);

VOID
VEAPI
CsrpInitializeProcessList(VOID);

#ifndef CSR_GET_SERVER_ID
#define CSR_GET_SERVER_ID(ApiNumber)    ((USHORT)(((ULONG)(ApiNumber)) >> 16))
#endif

#ifndef CSR_GET_FUNCTION_ID
#define CSR_GET_FUNCTION_ID(ApiNumber)  ((USHORT)(((ULONG)(ApiNumber)) & 0xFFFF))
#endif

typedef VEASTATUS (VEAPI *PCSR_SERVER_DLL_INIT_ROUTINE)(VOID);

VEASTATUS
VEAPI
CsrLoadServerDll(
    IN PCHAR DllPath,
    IN PCHAR EntryPointName
);

/** EOF */