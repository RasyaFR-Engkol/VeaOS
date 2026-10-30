#pragma once

#include "hcttype.h"

PACPI_HEADER
VEAPI
HctFindAcpiTable(const char* Signature);

VOID
VEAPI
HctpDumpCachedAcpiTable(VOID);

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

KIRQL
VEAPI
HctConvertVectorToIrql(IN UCHAR Vector);

VEASTATUS
VEAPI
HctInitializeIoApic(IN PACPI_HEADER MadtTable);

VEASTATUS
VEAPI
HctTranslateIrqToGsi(
    IN ULONG Irq,
    OUT PULONG Gsi,
    OUT PUSHORT Flags
);

VEASTATUS
VEAPI
HctSetGsiRedirection(
    IN ULONG Gsi,
    IN ULONG Vector,
    IN USHORT Flags,
    IN BOOLEAN Masked
);

PVOID
VEAPI
HctGetFadt(IN PVOID Context);

PVOID
VEAPI
HctGetDsdt(IN PVOID Context);

PVOID
VEAPI
HctGetSsdt(IN PVOID Context, IN ULONG Index);