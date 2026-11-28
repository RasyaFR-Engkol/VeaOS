#pragma once

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

#define CSR_SERVER_BASE         0   /* basesrv */
#define CSR_SERVER_WIN          1   /* winsrv  */

#define CSR_MAKE_API_NUMBER(ServerId, FuncId) \
    ((ULONG)(((ServerId) << 16) | ((FuncId) & 0xFFFF)))

typedef enum _WINSRV_CONSOLE_ROUTINE {
    WinSrvpAllocConsole = 0,
    WinSrvpWriteConsole = 1,
    WinSrvpReadConsole  = 2,
    WinSrvpExitWindows  = 3,
    WinSrvpMaxApiNumber
} WINSRV_CONSOLE_ROUTINE;

/* EOF */