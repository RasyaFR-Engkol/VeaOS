/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : windowstation.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Window Station handling
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veapi.h>

/* Revision History ------------------------------------------------------
 * DATE       : 21-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file windowstation.c
 * --------------------------------------------------------------------- */

PWINDOW_STATION_OBJECT GpInteractiveWindowStation = NULL;
PDESKTOP_OBJECT        GpActiveDesktop            = NULL;
static HANDLE GhWindowsDir    = NULL;
static HANDLE GhWinStaDir     = NULL;
static HANDLE GhWinSta0       = NULL;
static HANDLE GhDefaultDesk   = NULL;

VOID
VEAPI
WndWinStaDelete(
    IN PVOID Object
)
{
    PWINDOW_STATION_OBJECT WinSta = (PWINDOW_STATION_OBJECT)Object;

    if (!WinSta)
    {
        return;
    }

    if (GpInteractiveWindowStation == WinSta)
    {
        GpInteractiveWindowStation = NULL;
    }

    DPRINT("[VEAPI/WINSTA] WindowStation '%s' deleted (%p)\n",
           WinSta->Name.Buffer ? WinSta->Name.Buffer : "<unnamed>",
           WinSta);
}

LONG
VEAPI
WndWinStaParse(
    IN PVOID ParseObject,
    IN POB_LOOKUP_CONTEXT LookupContext,
    IN PVOID Context,
    OUT PVOID *ResolvedObject
)
{
    PWINDOW_STATION_OBJECT WinSta = (PWINDOW_STATION_OBJECT)ParseObject;
    PLIST_ENTRY Entry;
    KIRQL OldIrql;

    UNREFERENCED_PARAMETER(Context);

    if (!WinSta || !LookupContext || !ResolvedObject)
    {
        return STATUS_INVALID_PARAMETER;
    }

    *ResolvedObject = NULL;

    if (LookupContext->RemainingName.Length == 0)
    {
        *ResolvedObject = WinSta;
        return STATUS_SUCCESS;
    }

    for (USHORT i = 0; i < LookupContext->RemainingName.Length; i++)
    {
        if (LookupContext->RemainingName.Buffer[i] == '/' ||
            LookupContext->RemainingName.Buffer[i] == '\\')
        {
            return STATUS_OBJECT_PATH_INVALID;
        }
    }

    KsAcquireSpinLock(&WinSta->Lock, &OldIrql);
    for (Entry = WinSta->DesktopListHead.Flink;
         Entry != &WinSta->DesktopListHead;
         Entry = Entry->Flink)
    {
        PDESKTOP_OBJECT Desktop = CONTAINING_RECORD(Entry, DESKTOP_OBJECT, ListEntry);

        if (RtlEqualString(&Desktop->Name,
                           &LookupContext->RemainingName,
                           LookupContext->CaseInsensitive))
        {
            KsReleaseSpinLock(&WinSta->Lock, OldIrql);

            LookupContext->RemainingName.Buffer += LookupContext->RemainingName.Length;
            LookupContext->RemainingName.Length = 0;

            *ResolvedObject = Desktop;
            return STATUS_SUCCESS;
        }
    }
    KsReleaseSpinLock(&WinSta->Lock, OldIrql);

    /* 4. Tidak ketemu: habis-kan RemainingName agar ObpLookupObjectName cek InsertMode */
    LookupContext->RemainingName.Buffer += LookupContext->RemainingName.Length;
    LookupContext->RemainingName.Length = 0;

    return STATUS_SUCCESS;
}

BOOLEAN
VEAPI
WndWinStaOkToClose(
    IN PEPROCESS Process OPTIONAL,
    IN PVOID Object,
    IN HANDLE Handle,
    IN KPROCESSOR_MODE PreviousMode
)
{
    PWINDOW_STATION_OBJECT WinSta = (PWINDOW_STATION_OBJECT)Object;

    UNREFERENCED_PARAMETER(Process);
    UNREFERENCED_PARAMETER(Handle);

    /* Lindungi WinSta0 interaktif dari close langsung oleh proses UserMode */
    if (PreviousMode == UserMode && WinSta == GpInteractiveWindowStation)
    {
        return FALSE;
    }

    return TRUE;
}

VEASTATUS
VEAPI
UserInitializeWindowStations(VOID)
{
    VEASTATUS Status;
    OBJECT_ATTRIBUTES ObjAttr;
    ANSI_STRING WindowsDirName  = RTL_CONSTANT_ANSI_STRING("/Windows");
    ANSI_STRING WinStaDirName   = RTL_CONSTANT_ANSI_STRING("/Windows/WindowStations");
    ANSI_STRING WinSta0Path     = RTL_CONSTANT_ANSI_STRING("/Windows/WindowStations/WinSta0");
    ANSI_STRING DefaultDeskPath = RTL_CONSTANT_ANSI_STRING("/Windows/WindowStations/WinSta0/Default");

    PWINDOW_STATION_OBJECT WinSta0 = NULL;
    PDESKTOP_OBJECT DefaultDesk    = NULL;

    /* 1. Buat direktori /Windows dan /Windows/WindowStations */
    InitializeObjectAttributes(&ObjAttr, &WindowsDirName, OBJ_CASE_INSENSITIVE | OBJ_PERMANENT, NULL, NULL);
    Status = VzCreateDirectoryNamespace(&GhWindowsDir, DIRECTORY_ALL_ACCESS, &ObjAttr);
    if (!VEA_SUCCESS(Status) && Status != STATUS_OBJECT_NAME_COLLISION)
    {
        DPRINT("[VEAPI] Gagal buat /Windows: 0x%08X\n", Status);
        return Status;
    }

    InitializeObjectAttributes(&ObjAttr, &WinStaDirName, OBJ_CASE_INSENSITIVE | OBJ_PERMANENT, NULL, NULL);
    Status = VzCreateDirectoryNamespace(&GhWinStaDir, DIRECTORY_ALL_ACCESS, &ObjAttr);
    if (!VEA_SUCCESS(Status) && Status != STATUS_OBJECT_NAME_COLLISION)
    {
        DPRINT("[VEAPI] Gagal buat /Windows/WindowStations: 0x%08X\n", Status);
        return Status;
    }

    /* 2. Buat objek WinSta0 (ExWindowStationObjectType) */
    InitializeObjectAttributes(&ObjAttr, &WinSta0Path, OBJ_CASE_INSENSITIVE | OBJ_PERMANENT, NULL, NULL);
    Status = ObCreateObject(
        KernelMode,
        UlWindowStationObjectType,
        &ObjAttr,
        KernelMode,
        NULL,
        sizeof(WINDOW_STATION_OBJECT),
        0,
        0,
        (PVOID *)&WinSta0
    );
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[VEAPI] ObCreateObject WinSta0 gagal: 0x%08X\n", Status);
        return Status;
    }

    RtlZeroMemory(WinSta0, sizeof(WINDOW_STATION_OBJECT));
    RtlInitAnsiString(&WinSta0->Name, "WinSta0");
    KsInitializeSpinlock(&WinSta0->Lock);
    InitializeListHead(&WinSta0->DesktopListHead);

    Status = ObInsertObject(WinSta0, NULL, VEA_STANDARD_RIGHTS_ALL | 0x037F, 0, NULL, &GhWinSta0);
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[VEAPI] ObInsertObject WinSta0 gagal: 0x%08X\n", Status);
        return Status;
    }

    /* 3. Buat objek Desktop "Default" di bawah WinSta0 */
    InitializeObjectAttributes(&ObjAttr, &DefaultDeskPath, OBJ_CASE_INSENSITIVE | OBJ_PERMANENT, NULL, NULL);
    Status = ObCreateObject(
        KernelMode,
        UlDesktopObjectType,
        &ObjAttr,
        KernelMode,
        NULL,
        sizeof(DESKTOP_OBJECT),
        0,
        0,
        (PVOID *)&DefaultDesk
    );
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[VEAPI] ObCreateObject Default Desktop gagal: 0x%08X\n", Status);
        return Status;
    }

    RtlZeroMemory(DefaultDesk, sizeof(DESKTOP_OBJECT));
    RtlInitAnsiString(&DefaultDesk->Name, "Default");
    DefaultDesk->ParentWinSta = WinSta0;
    KsInitializeSpinlock(&DefaultDesk->Lock);
    InitializeListHead(&DefaultDesk->WindowListHead);
    DefaultDesk->BackgroundColor = 0xFF008080;

    Status = ObInsertObject(DefaultDesk, NULL, VEA_STANDARD_RIGHTS_ALL | 0x01FF, 0, NULL, &GhDefaultDesk);
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[VEAPI] ObInsertObject Default Desktop gagal: 0x%08X\n", Status);
        return Status;
    }

    /* 4. Kaitkan Default Desktop ke WinSta0 dan set sebagai ActiveDesktop */
    InsertTailList(&WinSta0->DesktopListHead, &DefaultDesk->ListEntry);
    WinSta0->ActiveDesktop     = DefaultDesk;
    GpInteractiveWindowStation = WinSta0;
    GpActiveDesktop            = DefaultDesk;

    WndAdoptOrphanWindowsToDesktop(DefaultDesk);

    DPRINT("[VEAPI] WinSta0 & Desktop 'Default' aktif (WinSta=%p, Desktop=%p)\n",
           WinSta0, DefaultDesk);

    return STATUS_SUCCESS;
}