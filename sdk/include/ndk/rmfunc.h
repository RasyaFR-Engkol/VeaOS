// RM FUNC

#pragma once

BOOLEAN
VEAPI
RmAddSystemServiceTable(
    IN PULONG_PTR Base,
    IN PULONG Count OPTIONAL,
    IN ULONG Limit,
    IN PUCHAR Number,
    IN ULONG Index
);

NORET
VOID
VEAPI
RiSyscallDispatcherHandler(
    IN PKTRAP_FRAME TrapFrame,
    IN PVOID Context
);