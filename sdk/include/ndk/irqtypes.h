#pragma once

#include <procbind.h>

typedef enum _KINTERRUPT_CONNECTION_TYPE
{
    InterruptLineBased,
    InterruptMessageBased,
    InterruptExtendedMessageBased
} KINTERRUPT_CONNECTION_TYPE;

typedef struct _KINTERRUPT_MESSAGE
{
    ULONG AddressLow;
    ULONG AddressHigh;
    ULONG Data;
    ULONG Vector;
} KINTERRUPT_MESSAGE, *PKINTERRUPT_MESSAGE;

/* EOF */
