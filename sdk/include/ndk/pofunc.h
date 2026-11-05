#pragma once

#include "ks.h"
#include <procbind.h>

VOID
VEAPI
PoInitializePrcb(IN PKPRCB Prcb);

VOID
VEAPI
PopInitShutdownList(VOID);

VEASTATUS
VEAPI
PopAddRemoveSysCapsCallback(
    IN PVOID NotificationStructure,
    IN PVOID Context
);

BOOLEAN
VEAPI
PoInitSystem(IN ULONG BootPhase);

/* EOF */