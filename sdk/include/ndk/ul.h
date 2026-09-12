#pragma once

#include <ldrtypes.h>
#include "veastatus.h"
#include "ultypes.h"
#include <mm.h>

void ul_initialize_layer(PBLOCK_BOOT_2 block_boot);

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

VOID
VEAPI
UlPhase1(VOID);

PVOID 
VEAPI 
UlAllocatePoolWithTag(POOL_TYPE PoolType, ULONG NumberOfBytes, ULONG Tag);

VOID 
VEAPI 
UlFreePoolWithTag(PVOID P, ULONG Tag);
