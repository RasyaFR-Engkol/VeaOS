#pragma once

#include <procbind.h>
#include <kpctypes.h>
#include <csr/csrmsg.h>
#include <csr/csrsrv.h>
#include <csr/baserv.h>
#include <csr/winsrv.h>
#include <csr/basemsg.h>
#include <vz_wrappers.h>

extern 
VEASTATUS
VEAPI
VeaUserInitialize(VOID);

extern
VEASTATUS
VEAPI
LdrGetProcedureAddress(
    IN PVOID BaseAddress,
    IN PANSI_STRING Name OPTIONAL,
    IN ULONG Ordinal OPTIONAL,
    OUT PVOID *ProcedureAddress
);

extern 
VEASTATUS
VEAPI
LdrLoadDll(
    IN PCSTR SearchPath OPTIONAL,
    IN PULONG DllCharacteristics OPTIONAL,
    IN PANSI_STRING DllName,
    OUT PVOID *BaseAddress
);

/* EOF */