/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : ulintrin.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Ul Ex Intrinsic
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 18-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file ulintrin.c
 * --------------------------------------------------------------------- */

VOID
VEAPI
ProbeForRead(
    IN const PVOID Address,
    IN SIZE_T Length,
    IN ULONG Alignment
)
{
    ULONG_PTR Last, Current = (ULONG_PTR)Address;

    if (Length != 0)
    {
        if ((Current & (Alignment - 1)) != 0)
        {
            UlRaiseStatus(STATUS_DATATYPE_MISALIGNMENT);
        }

        Last = Current + Length - 1;
        if ((Last < Current) || (Last >= (ULONG_PTR)MI_USER_PROBE_ADDRESS))
        {
            UlRaiseStatus(STATUS_ACCESS_VIOLATION);
        }
    }
}

VOID
VEAPI
ProbeForWrite(IN PVOID Address,
              IN SIZE_T Length,
              IN ULONG Alignment)
{
    ULONG_PTR Last, Current = (ULONG_PTR)Address;

    if (Length != 0)
    {

        if ((Current & (Alignment - 1)) != 0)
        {
            UlRaiseStatus(STATUS_DATATYPE_MISALIGNMENT);
        }

        Last = Current + Length - 1;
        if ((Last < Current) || (Last >= (ULONG_PTR)MI_USER_PROBE_ADDRESS))
        {
            UlRaiseStatus(STATUS_ACCESS_VIOLATION);
        }

        /* Round down to the last page */
        Last = PAGE_ROUND_DOWN(Last) + PAGE_SIZE;
        do
        {
            *(volatile CHAR*)Current = *(volatile CHAR*)Current;

            Current = PAGE_ROUND_DOWN(Current) + PAGE_SIZE;
        } while (Current != Last);
    }
}