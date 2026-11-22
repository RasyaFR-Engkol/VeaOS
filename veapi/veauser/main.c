/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : main.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : VEAPI Main Entry
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veapi.h>

/* Revision History ------------------------------------------------------
 * DATE       : 20-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file main.c
 * --------------------------------------------------------------------- */

extern ULONG_PTR VeapiSSDT[];
extern UCHAR VeapiSSPT[];
extern ULONG VeapiNumberOfSysCalls;

VEASTATUS
VEAPI
VeaUserInitialize(VOID)
{
    VEASTATUS Status;

    KdPrintf("[VEAPI] VeaUserInitialize called!\n");

    Status = UserInitialize();

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
DriverEntry(
    IN PDRIVER_OBJECT DriverObject
)
{
    HANDLE hRitThread = NULL;
    VEASTATUS Status;
    VEAPI_CALLOUTS_FPNS Callouts;

    UNREFERENCED_PARAMETER(DriverObject);

    /* Daftarkan Shadow SSDT ke slot [1] */
    if (!RmAddSystemServiceTable(VeapiSSDT, NULL, VeapiNumberOfSysCalls, VeapiSSPT, 1))
    {
        KdPrintf("[VEAPI] Gagal mendaftarkan Shadow SSDT!\n");
        return STATUS_UNSUCCESSFUL;
    }

    RtlZeroMemory(&Callouts, sizeof(VEAPI_CALLOUTS_FPNS));

    Callouts.ProcessCallout                  = UserProcessCallout;
    Callouts.ThreadCallout                   = UserThreadCallout;
    Callouts.WindowStationObjectSize         = sizeof(WINDOW_STATION_OBJECT);
    Callouts.DesktopObjectSize               = sizeof(DESKTOP_OBJECT);
    Callouts.WindowStationDeleteProcedure    = WndWinStaDelete;
    Callouts.WindowStationParseProcedure     = WndWinStaParse;
    Callouts.WindowStationOkToCloseProcedure = WndWinStaOkToClose;
    Callouts.DesktopDeleteProcedure          = DskDesktopDelete;
    Callouts.DesktopOpenProcedure            = DskDesktopOpen;
    Callouts.DesktopCloseProcedure           = DskDesktopClose;
    Callouts.DesktopOkToCloseProcedure       = DskDesktopOkToClose;

    PtEstablishVeapiCallouts(&Callouts);

    MsgInitializeInputQueue();

    /* Jalankan Raw Input Thread di kernel mode */
    Status = PtCreateSystemThread(
        &hRitThread,
        0,
        NULL,
        NULL,
        NULL,
        IptUserRawInputThread,
        NULL
    );

    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[VEAPI] Gagal membuat Raw Input Thread: 0x%08X\n", Status);
        return Status;
    }

    if (hRitThread)
    {
        VzClose(hRitThread);
    }

    KdPrintf("[VEAPI] system_veapi.ko berhasil dimuat!\n");
    return STATUS_SUCCESS;
}

/* EOF */