/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : ptveapi.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : VEAPI 
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 21-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file ptveapi.c
 * --------------------------------------------------------------------- */

VEAPI_CALLOUTS_FPNS PtpVeapiCallouts = { 0 };

VOID
VEAPI
PtEstablishVeapiCallouts(
    IN PVEAPI_CALLOUTS_FPNS CalloutData
)
{
    if (!CalloutData)
    {
        return;
    }

    /* Salin seluruh pointer fungsi dari veapi.ko ke kernel */
    RtlCopyMemory(&PtpVeapiCallouts, CalloutData, sizeof(VEAPI_CALLOUTS_FPNS));

    /* Update ukuran body objek supaya ObCreateObject alokasi ukuran yang pas */
    if (UlWindowStationObjectType && CalloutData->WindowStationObjectSize > 0)
    {
        UlWindowStationObjectType->ObjectSize = CalloutData->WindowStationObjectSize;
    }

    if (UlDesktopObjectType && CalloutData->DesktopObjectSize > 0)
    {
        UlDesktopObjectType->ObjectSize = CalloutData->DesktopObjectSize;
    }

    DPRINT("[PT] VEAPI callouts established (WinStaSize=%u, DeskSize=%u)\n",
           CalloutData->WindowStationObjectSize,
           CalloutData->DesktopObjectSize);
}

VEASTATUS
VEAPI
PtConvertToGuiThread(VOID)
{
    PKTHREAD  CurrentThread  = PtGetCurrentThread();
    PEPROCESS CurrentProcess = (PEPROCESS)PtGetCurrentProcess();
    VEASTATUS Status;

    /* Hanya valid dipanggil dari UserMode */
    if (CurrentThread->PreviousMode == KernelMode)
    {
        return STATUS_INVALID_PARAMETER;
    }

    /* Pastikan veapi.ko sudah mendaftarkan callout-nya */
    if (!PtpVeapiCallouts.ProcessCallout || !PtpVeapiCallouts.ThreadCallout)
    {
        return STATUS_ACCESS_DENIED;
    }

    /* Jika ServiceTable sudah menunjuk Shadow SSDT, thread ini sudah GUI thread */
    if ((ULONG_PTR)CurrentThread->ServiceTable == (ULONG_PTR)KsServiceTableDescriptorShadow)
    {
        return STATUS_ALREADY_WIN32;
    }

    /* 1. Selalu panggil ProcessCallout (veapi.ko yang cek/init PROCESSINFO) */
    Status = PtpVeapiCallouts.ProcessCallout(CurrentProcess, TRUE);
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    /* 2. Pindahkan ServiceTable thread ke Shadow SSDT */
    CurrentThread->ServiceTable = KsServiceTableDescriptorShadow;

    /* 3. Alokasi & inisialisasi THREADINFO (rpdesk, dll.) via ThreadCallout */
    Status = PtpVeapiCallouts.ThreadCallout(
        (PETHREAD)CurrentThread,
        VeapiThreadCalloutInitialize
    );
    if (!VEA_SUCCESS(Status))
    {
        /* Rollback ke SSDT standar jika inisialisasi THREADINFO gagal */
        CurrentThread->ServiceTable = KsServiceTableDescriptor;
        return Status;
    }

    return STATUS_SUCCESS;
}

PVOID
VEAPI
PtGetCurrentThreadVeapiThread(VOID)
{
    return ((PKTHREAD)PtGetCurrentThread())->Win32Thread;
}

/* EOF */