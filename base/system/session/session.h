#pragma once

#include <veastatus.h>
#include <rtl.h>
#include "vz_wrappers.h"

VEASTATUS
VEAPI
SmpInitializeNamespace(VOID);

VEASTATUS
VEAPI
SmpLoadConfiguration(
    IN int argc,
    IN char *argv[],
    IN PCHAR envp
);

VEASTATUS
VEAPI
SmpCreateApiPort(
    VOID
);

VEASTATUS
VEAPI
SmpExecuteImage(
    IN PCHAR ImagePath,
    IN PCHAR CommandLine OPTIONAL,
    IN PVOID Environment OPTIONAL,
    IN BOOLEAN StartSuspended,
    OUT PRTL_USER_PROCESS_INFORMATION ProcessInfo
);

VEASTATUS
VEAPI
SmpInitializeKnownDlls(VOID);

BOOLEAN
SmpStrEqualI(
    IN PCHAR Str1,
    IN PCHAR Str2
);

extern CHAR SmpSubSystemPath[260];
extern CHAR SmpSessionUiPath[260];
extern CHAR SmpVeapiPath[260];
extern BOOLEAN SmpMiniVeaMode;
extern BOOLEAN SmpUseWinTerminal;

/* EOF */