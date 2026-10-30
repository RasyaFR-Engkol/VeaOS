/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : pcirw.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : PCI Read Write
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include "ahciheader.h"

/* Revision History ------------------------------------------------------
 * DATE       : 26-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file pcirw.c
 * --------------------------------------------------------------------- */

ULONG
VEAPI
AhciReadConfig(
    IN PBUS_INTERFACE_STANDARD BusInterface,
    IN ULONG Offset,
    IN ULONG Length,
    OUT PVOID Buffer
)
{
    if (!BusInterface || !BusInterface->GetBusData || !Buffer || Length == 0)
    {
        return 0;
    }

    return BusInterface->GetBusData(
        BusInterface->Context,
        0, // DataType / PCI_WHICHSPACE_CONFIG
        Buffer,
        Offset,
        Length
    );
}

ULONG
VEAPI
AhciWriteConfig(
    IN PBUS_INTERFACE_STANDARD BusInterface,
    IN ULONG Offset,
    IN ULONG Length,
    IN PVOID Buffer
)
{
    if (!BusInterface || !BusInterface->SetBusData || !Buffer || Length == 0)
    {
        return 0;
    }

    return BusInterface->SetBusData(
        BusInterface->Context,
        0, // DataType / PCI_WHICHSPACE_CONFIG
        Buffer,
        Offset,
        Length
    );
}

USHORT
VEAPI
AhciReadConfigWord(
    IN PBUS_INTERFACE_STANDARD BusInterface,
    IN ULONG Offset
)
{
    USHORT Value = 0;
    AhciReadConfig(BusInterface, Offset, sizeof(USHORT), &Value);
    return Value;
}

VOID
VEAPI
AhciWriteConfigWord(
    IN PBUS_INTERFACE_STANDARD BusInterface,
    IN ULONG Offset,
    IN USHORT Value
)
{
    AhciWriteConfig(BusInterface, Offset, sizeof(USHORT), &Value);
}

VOID
VEAPI
AhciWriteConfigDword(
    IN PBUS_INTERFACE_STANDARD BusInterface,
    IN ULONG Offset,
    IN ULONG Value
)
{
    AhciWriteConfig(BusInterface, Offset, sizeof(ULONG), &Value);
}

/* EOF */