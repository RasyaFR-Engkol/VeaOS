#pragma once

BOOL
VEAPI
CreateProcessA(
    IN PSTR lpApplicationName,
    IN PSTR lpCommandLine,
    IN LPSECURITY_ATTRIBUTES lpProcessAttributes,
    IN LPSECURITY_ATTRIBUTES lpThreadAttributes,
    IN BOOL bInheritHandles,
    IN DWORD dwCreationFlags,
    IN PVOID lpEnvironment,
    IN PSTR lpCurrentDirectory,
    IN LPSTARTUPINFOA lpStartupInfo,
    OUT LPPROCESS_INFORMATION lpProcessInformation
);

VOID
VEAPI
ExitProcess(
    IN UINT uExitCode
);

/* EOF */