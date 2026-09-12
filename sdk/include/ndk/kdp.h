#pragma once

#include "kdptypes.h"
#include "ks.h"

VOID 
VEAPI
kdp_serial_putchar(CHAR c);

VOID
VEAPI
kdp_print(PCHAR string);

VOID
KdPrintf(const char *Format, ...);

#define DPRINT(fmt, ...) \
    do { \
        KdPrintf("[%s:%d] ", __FILE__, __LINE__); \
        KdPrintf(fmt, ##__VA_ARGS__); \
    } while (0)

VOID
VEAPI
kdp_activate_serial(VOID);

BOOLEAN
VEAPI
KdpHandleDebug(PKREGISTER_FRAME RegisterFrame);

/* Limit debugger by 256 character */
#define MAXIMUM_INPUT_DEBUGGER 256