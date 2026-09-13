#pragma once

#include "hcttype.h"

PACPI_HEADER
VEAPI
HctFindAcpiTable(const char* Signature);

FORCEINLINE
UCHAR
IoReadPortByte( USHORT Port)
{
    UCHAR Value;
    __asm__ __volatile__("inb %1, %0" : "=a"(Value) : "Nd"(Port));
    return Value;
}

FORCEINLINE
VOID
IoWritePortByte( USHORT Port,  UCHAR Value)
{
    __asm__ __volatile__("outb %0, %1" :: "a"(Value), "Nd"(Port));
};

VOID
VEAPI
HctInitializePnp(VOID);