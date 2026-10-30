/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : obdump.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Object Dumping and Helping
 ----- Effective since 09-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 15-09-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file obdump.c
 * --------------------------------------------------------------------- */

VOID
VEAPI
ObDumpLifecycle(VOID)
{
    ULONG ReportCount;

    /* Check whether the lifecycle report table has been initialized. */
    if (!ObReportLifecycleReady || !ObLifecycleTable)
    {
        KdPrintf("OB: Lifecycle report table is not available.\n\r");
        return;
    }

    /* Clamp the count to the fixed table capacity before iterating. */
    ReportCount = ObLifecycleTableFreeIndex;
    if (ReportCount > 100)
    {
        ReportCount = 100;
    }

    /* Print the lifecycle report header and number of recorded entries. */
    KdPrintf("OB Lifecycle Report (%u entries)\n\r", ReportCount);
    KdPrintf("----------------------------------------\n\r");

    /* Print every recorded object and handle lifecycle event. */
    for (ULONG ReportIndex = 0; ReportIndex < ReportCount; ReportIndex++)
    {
        POB_LIFECYCLE_REPORT_TABLE Report;

        Report = &ObLifecycleTable[ReportIndex];

        /* Handle reports include owner thread and referenced object identity. */
        if (Report->IsHandle)
        {
            PETHREAD OwnerThread = (PETHREAD)Report->OwnerThread;

            if (OwnerThread)
            {
                KdPrintf(
                    "[%u] Handle=%p Thread=%p (TID=%u PID=%u) Object=%p Ref=%u State=%s\n\r",
                    ReportIndex,
                    Report->ObjectOrHandle,
                    Report->OwnerThread,
                    OwnerThread->Cid.UniqueThread,
                    OwnerThread->Cid.UniqueProcess,
                    Report->TargetObject,
                    Report->CurrentReference,
                    Report->IsReferenced ? "Referenced" : "Released"
                );
            }
            else
            {
                KdPrintf(
                    "[%u] Handle=%p Thread=(null) Object=%p Ref=%u State=%s\n\r",
                    ReportIndex,
                    Report->ObjectOrHandle,
                    Report->TargetObject,
                    Report->CurrentReference,
                    Report->IsReferenced ? "Referenced" : "Released"
                );
            }
        }
        else
        {
            /* Object reports retain the object identity for comparison. */
            KdPrintf(
                "[%u] Object=%p Ref=%u State=%s\n\r",
                ReportIndex,
                Report->TargetObject,
                Report->CurrentReference,
                Report->IsReferenced ? "Referenced" : "Released"
            );
        }
    }

    KdPrintf("----------------------------------------\n\r");
}

BOOLEAN
VEAPI
ObProcessDebug(
    IN ULONG ArgumentCount, 
    IN PCHAR* Argument)
{
    KdPrintf("ObProcessDebug!\n\n");

    /* Validate argument count */
    if (ArgumentCount == 0)
    {
        KdPrintf("OB: No Argument were given\n");
        return TRUE;
    }

    /* Is it dump? */
    if (RtlRawStringCompareN(Argument[0], "dump", 4) == 0)
    {
        if (ArgumentCount < 2)
        {
            KdPrintf("  OB: Usage: Ob dump <to_dump>\n\n");
            KdPrintf(
                "list of available things to dump:\n"
                "- lifecycle\n"
                "- handle\n\n"
            );

            return TRUE;
        }

        if (RtlRawStringCompareN(Argument[1], "lifecycle", 9) == 0)
        {
            ObDumpLifecycle();
            return TRUE;
        }
    }

    return TRUE;
}