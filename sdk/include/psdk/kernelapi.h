#pragma once

#include "processthreadsapi.h"

ULONG
VEAPI
BaseSetLastVeaError(
    IN VEASTATUS Status
);

DWORD
VEAPI
GetLastError(VOID);

VOID
VEAPI
SetLastError(
    IN DWORD dwErrCode
);

/* EOF */