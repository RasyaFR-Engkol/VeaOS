#pragma once

typedef struct _BASE_CREATE_PROCESS_MSG {
    HANDLE ProcessHandle;
    HANDLE ThreadHandle;
    CLIENT_ID ClientId;
    ULONG CreationFlags;
} BASE_CREATE_PROCESS_MSG, *PBASE_CREATE_PROCESS_MSG;

typedef enum _BASE_MESSAGE_TYPE {
    BasepCreateProcess = 0,
    BasepCreateThread  = 1,
    BasepExitProcess   = 2,
    BasepDisconnect    = 3,
    BasepMaxApiNumber
} BASE_MESSAGE_TYPE;
