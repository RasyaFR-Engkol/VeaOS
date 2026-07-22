#pragma once

#include <ldrtypes.h>
#include "veastatus.h"
#include "ultypes.h"

void ul_initialize_layer(PBLOCK_BOOT_1 block_boot);

VEASTATUS
VEAPI
UlDestroyHandle(
    PHANDLE_TABLE Table,
    HANDLE Handle
);

HANDLE
VEAPI
UlCreateHandle(
    PHANDLE_TABLE Table,
    PVOID Object,
    ULONG GrantedAccess
);

PHANDLE_TABLE
VEAPI
UlCreateHandleTable(
    ULONG InitialCapacity
);
