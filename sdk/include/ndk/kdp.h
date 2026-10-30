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

VOID
VEAPI
KdpAssertFailedDebug(
   const PCHAR Expression,
   const PCHAR File, 
   const PCHAR Function, 
   ULONG Line
);

VOID
KdpDebugMailboxPrint(
   PCHAR String,
   ...
);

/* Assertation and macro */
#define ASSERTATION(expr) \
    do { \
        if (!(expr)) \
        { \
            KdpAssertFailedDebug(#expr, (const PCHAR)__FILE__, (const PCHAR)__FUNCTION__, __LINE__);    \
        } \
    } while (0)

#define UNIMPLEMENTED_DBGBREAK \
    do { \
        KdpDebugMailboxPrint("Unimplemented: %s %d %s \n", __FILE__, __LINE__, __FUNCTION__); \
    } while(0); 

BOOLEAN
VEAPI
KdpProcessInput(
    PCHAR String,
    PKREGISTER_FRAME Frame
);

VOID
VEAPI
KdpDebugBreak(DEBUGSTATUS Reason);

BOOLEAN
VEAPI
KdpSubsystemDump(PCHAR String);