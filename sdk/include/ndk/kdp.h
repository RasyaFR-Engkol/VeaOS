#pragma once

#include "kdptypes.h"

VOID 
VEAPI
kdp_serial_putchar(CHAR c);

VOID
VEAPI
kdp_print(PCHAR string);

VOID
KdPrintf(const char *Format, ...);

VOID
VEAPI
kdp_activate_serial(VOID);

