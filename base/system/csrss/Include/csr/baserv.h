#pragma once

VEASTATUS
VEAPI
BaseServerDllInitialization(VOID);

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