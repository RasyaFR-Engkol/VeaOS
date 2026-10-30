/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : generictable.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : RTL Generic Table
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <rtl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 30-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file generictable.c
 * --------------------------------------------------------------------- */

VOID
VEAPI
RtlInitializeGenericTable(IN PRTL_GENERIC_TABLE Table,
                          IN PRTL_GENERIC_COMPARE_ROUTINE CompareRoutine,
                          IN PRTL_GENERIC_ALLOCATE_ROUTINE AllocateRoutine,
                          IN PRTL_GENERIC_FREE_ROUTINE FreeRoutine,
                          IN PVOID TableContext)
{
    /* Initialize the table to default and passed values */
    InitializeListHead(&Table->InsertOrderList);
    Table->TableRoot = NULL;
    Table->NumberGenericTableElements = 0;
    Table->WhichOrderedElement = 0;
    Table->OrderedPointer = &Table->InsertOrderList;
    Table->CompareRoutine = CompareRoutine;
    Table->AllocateRoutine = AllocateRoutine;
    Table->FreeRoutine = FreeRoutine;
    Table->TableContext = TableContext;
}

/* EOF */