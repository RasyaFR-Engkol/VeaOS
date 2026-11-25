/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : process.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : VEAPI Process Handling
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veapi.h>
#include <psdk/w32thread.h>

/* Revision History ------------------------------------------------------
 * DATE       : 21-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file process.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
UserProcessCallout(
    IN PEPROCESS Process,
    IN BOOLEAN Create
)
{
    if (!Create && Process != NULL)
    {
        WndCleanupProcessWindows(Process);
        WndCleanupProcessClasses(Process);
    }
    // TODO

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
UserThreadCallout(
    IN PETHREAD Thread,
    IN VEAPI_THREAD_CALLOUT_TYPE Type
)
{
    if (!Thread)
    {
        return STATUS_INVALID_PARAMETER;
    }

    PKTHREAD KThread = (PKTHREAD)Thread;

    if (Type == VeapiThreadCalloutInitialize)
    {
        if (KThread->Win32Thread != NULL)
        {
            return STATUS_SUCCESS;
        }

        PTHREADINFO pti = (PTHREADINFO)UlAllocatePoolWithTag(
            NonPagedPool,
            sizeof(THREADINFO),
            TAG_VEAPI_THREAD
        );
        if (!pti)
        {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        RtlZeroMemory(pti, sizeof(THREADINFO));
        pti->pEThread = Thread;
        pti->RefCount = 1;
        InitializeListHead(&pti->WindowListHead);

        /* Default: ikat ke GpActiveDesktop sampai thread memanggil SetThreadDesktop */
        if (GpActiveDesktop != NULL)
        {
            ObReferenceObject(GpActiveDesktop);
            pti->rpdesk = GpActiveDesktop;
        }

        KThread->Win32Thread = pti;
        return STATUS_SUCCESS;
    }
    else if (Type == VeapiThreadCalloutExit)
    {
        /* 1. Hancurkan antrean pesan milik thread ini */
        MsgDestroyThreadQueue(Thread);

        /* 2. Lepas desktop & bebaskan THREADINFO */
        PTHREADINFO pti = (PTHREADINFO)KThread->Win32Thread;
        if (pti != NULL)
        {
            KThread->Win32Thread = NULL;

            if (pti->rpdesk != NULL)
            {
                ObDereferenceObject(pti->rpdesk);
                pti->rpdesk = NULL;
            }

            UlFreePoolWithTag(pti, TAG_VEAPI_THREAD);
        }

        return STATUS_SUCCESS;
    }

    return STATUS_INVALID_PARAMETER;
}

