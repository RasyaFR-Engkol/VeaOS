#pragma once

#include "procbind.h"

#define COM1_PORT 0x3F8

/* DebugStatus */

typedef ULONG DEBUGSTATUS;

#define DEBUG_BREAK_CONTROL_C 0x90
#define DEBUG_BREAK_BUG_CHECK 0x91
#define DEBUG_BREAK_ASSERTION 0x92

extern DEBUGSTATUS KdpLastStatus;

/* Limit debugger by 256 character */
#define KDP_MAXIMUM 500
#define MAXIMUM_INPUT_DEBUGGER KDP_MAXIMUM
#define MAXIMUM_PENDING_MESSAGE_DEBUGGER KDP_MAXIMUM

extern CHAR KdpPendingMessage[MAXIMUM_PENDING_MESSAGE_DEBUGGER];
extern ULONG KdpPendingMessageIndex;
extern BOOLEAN KdpTheresPendingMesssage;
extern BOOLEAN KdpPresent;
