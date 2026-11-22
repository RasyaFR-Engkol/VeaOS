/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : desktop.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Desktop Implementation
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veapi.h>

/* Revision History ------------------------------------------------------
 * DATE       : 21-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file desktop.c
 * --------------------------------------------------------------------- */

VOID
VEAPI
DskDesktopDelete(
    IN PVOID Object
)
{
    PDESKTOP_OBJECT Desktop = (PDESKTOP_OBJECT)Object;
    KIRQL OldIrql;

    if (!Desktop)
    {
        return;
    }

    /* 1. Cabut Desktop ini dari DesktopListHead milik ParentWinSta */
    if (Desktop->ParentWinSta)
    {
        PWINDOW_STATION_OBJECT WinSta = Desktop->ParentWinSta;
        KsAcquireSpinLock(&WinSta->Lock, &OldIrql);

        if (Desktop->ListEntry.Flink != NULL && Desktop->ListEntry.Blink != NULL)
        {
            RemoveEntryList(&Desktop->ListEntry);
            Desktop->ListEntry.Flink = NULL;
            Desktop->ListEntry.Blink = NULL;
        }

        if (WinSta->ActiveDesktop == Desktop)
        {
            WinSta->ActiveDesktop = NULL;
        }

        KsReleaseSpinLock(&WinSta->Lock, OldIrql);
    }

    /* 2. Reset pointer global jika Desktop yang aktif dihapus */
    if (GpActiveDesktop == Desktop)
    {
        GpActiveDesktop = NULL;
    }

    /* 3. Lepas referensi Desktop dari sisa window yang masih nyantol */
    KsAcquireSpinLock(&Desktop->Lock, &OldIrql);
    while (!IsListEmpty(&Desktop->WindowListHead))
    {
        PLIST_ENTRY Entry = RemoveHeadList(&Desktop->WindowListHead);
        PWND_OBJECT Wnd = CONTAINING_RECORD(Entry, WND_OBJECT, DesktopListEntry);

        Wnd->Desktop = NULL;
        Wnd->DesktopListEntry.Flink = NULL;
        Wnd->DesktopListEntry.Blink = NULL;

        if (IptFocusWindow == Wnd->hWnd)
        {
            IptFocusWindow = NULL;
        }
    }
    Desktop->ForegroundWindow = NULL;
    Desktop->FocusWindow      = NULL;
    KsReleaseSpinLock(&Desktop->Lock, OldIrql);

    /* Catatan: Jangan UlFreePool(Desktop->Name.Buffer) kalau diinisialisasi
       pakai string literal RtlInitAnsiString(&Desktop->Name, "Default")! */
    DPRINT("[VEAPI/DSK] Desktop '%s' deleted (%p)\n",
           Desktop->Name.Buffer ? Desktop->Name.Buffer : "<unnamed>",
           Desktop);
}

VEASTATUS
VEAPI
DskDesktopOpen(
    IN OB_OPEN_REASON OpenReason,
    IN PEPROCESS Process OPTIONAL,
    IN PVOID Object,
    IN ACCESS_MASK GrantedAccess,
    IN ULONG HandleCount
)
{
    UNREFERENCED_PARAMETER(OpenReason);
    UNREFERENCED_PARAMETER(Process);
    UNREFERENCED_PARAMETER(Object);
    UNREFERENCED_PARAMETER(GrantedAccess);
    UNREFERENCED_PARAMETER(HandleCount);

    /* Akan diisi setelah W32PROCESS & Desktop Heap Section siap */
    return STATUS_SUCCESS;
}

VOID
VEAPI
DskDesktopClose(
    IN PEPROCESS Process OPTIONAL,
    IN PVOID Object,
    IN ACCESS_MASK GrantedAccess,
    IN ULONG ProcessHandleCount,
    IN ULONG SystemHandleCount
)
{
    UNREFERENCED_PARAMETER(Process);
    UNREFERENCED_PARAMETER(Object);
    UNREFERENCED_PARAMETER(GrantedAccess);
    UNREFERENCED_PARAMETER(ProcessHandleCount);
    UNREFERENCED_PARAMETER(SystemHandleCount);
}

BOOLEAN
VEAPI
DskDesktopOkToClose(
    IN PEPROCESS Process OPTIONAL,
    IN PVOID Object,
    IN HANDLE Handle,
    IN KPROCESSOR_MODE PreviousMode
)
{
    PDESKTOP_OBJECT Desktop = (PDESKTOP_OBJECT)Object;

    UNREFERENCED_PARAMETER(Process);
    UNREFERENCED_PARAMETER(Handle);

    /* Lindungi Default / Active Desktop agar tidak ditutup sembarangan oleh UserMode */
    if (PreviousMode == UserMode && Desktop == GpActiveDesktop)
    {
        return FALSE;
    }

    return TRUE;
}

/* EOF */