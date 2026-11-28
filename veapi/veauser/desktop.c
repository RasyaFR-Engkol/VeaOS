/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : desktop.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Desktop Implementation
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veapi.h>
#include <psdk/w32thread.h>

/* Revision History ------------------------------------------------------
 * DATE       : 21-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file desktop.c
 * --------------------------------------------------------------------- */

PEPROCESS GpLogonProcess      = NULL;
HWND      GpLogonNotifyWindow = NULL;

static BOOLEAN
DskCopyNameFromCaller(
    OUT PCHAR       Dest,
    IN  SIZE_T      DestSize,
    IN  CONST CHAR *Src
)
{
    BOOLEAN IsUser = (KeGetPreviousMode() == UserMode);

    if (!Dest || DestSize == 0 || !Src)
    {
        return FALSE;
    }

    RtlZeroMemory(Dest, DestSize);

    _VEA_TRY
    {
        for (SIZE_T i = 0; i < DestSize - 1; i++)
        {
            if (IsUser && ((ULONG_PTR)(Src + i) >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS))
            {
                _VEA_YIELD(return FALSE);
            }
            Dest[i] = Src[i];
            if (Src[i] == '\0')
            {
                break;
            }
        }
    }
    _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
    {
        return FALSE;
    }
    _VEA_END

    return (Dest[0] != '\0');
}

static PDESKTOP_OBJECT
DskFindDesktopLocked(
    IN PWINDOW_STATION_OBJECT WinSta,
    IN PANSI_STRING           TargetName
)
{
    PLIST_ENTRY Entry;

    for (Entry = WinSta->DesktopListHead.Flink;
         Entry != &WinSta->DesktopListHead;
         Entry = Entry->Flink)
    {
        PDESKTOP_OBJECT Dsk = CONTAINING_RECORD(Entry, DESKTOP_OBJECT, ListEntry);
        if (RtlEqualString(&Dsk->Name, TargetName, TRUE))
        {
            return Dsk;
        }
    }

    return NULL;
}

PDESKTOP_OBJECT
VEAPI
DskEnsureThreadDesktop(
    IN PTHREADINFO pti
)
{
    CHAR DeskName[64];
    ANSI_STRING DeskAnsi;
    KIRQL OldIrql;

    if (!pti || !GpInteractiveWindowStation)
    {
        return GpActiveDesktop;
    }

    if (pti->rpdesk != NULL)
    {
        return pti->rpdesk;
    }

    RtlZeroMemory(DeskName, sizeof(DeskName));

    /* Ambil DesktopInfo dari PEB->ProcessParameters milik proses saat ini */
    PEPROCESS Proc = (PEPROCESS)PtGetCurrentProcess();
    PPEB Peb = Proc ? Proc->Peb : NULL; /* Sesuaikan dengan getter/field PEB di kernelmu */

    if (Peb != NULL && KeGetPreviousMode() == UserMode)
    {
        _VEA_TRY
        {
            PRTL_USER_PROCESS_PARAMETERS Params = Peb->ProcessParameters;
            if (Params != NULL && Params->DesktopInfo.Length > 0 && Params->DesktopInfo.Buffer != NULL)
            {
                PCHAR SrcBuf = Params->DesktopInfo.Buffer;
                if (!(Params->Flags & RTL_USER_PROCESS_PARAMETERS_NORMALIZED))
                {
                    SrcBuf = (PCHAR)((ULONG_PTR)Params + (ULONG_PTR)SrcBuf);
                }

                USHORT CopyLen = Params->DesktopInfo.Length;
                if (CopyLen >= sizeof(DeskName))
                {
                    CopyLen = sizeof(DeskName) - 1;
                }
                RtlCopyMemory(DeskName, SrcBuf, CopyLen);
                DeskName[CopyLen] = '\0';
            }
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            DeskName[0] = '\0';
        }
        _VEA_END
    }

    /* Fallback standar Win32: proses tanpa DesktopInfo masuk ke 'Default', bukan ActiveDesktop */
    if (DeskName[0] == '\0')
    {
        RtlCopyMemory(DeskName, "Default", sizeof("Default"));
    }

    RtlInitAnsiString(&DeskAnsi, DeskName);

    KsAcquireSpinLock(&GpInteractiveWindowStation->Lock, &OldIrql);
    PDESKTOP_OBJECT Target = DskFindDesktopLocked(GpInteractiveWindowStation, &DeskAnsi);
    if (Target)
    {
        ObReferenceObject(Target);
    }
    KsReleaseSpinLock(&GpInteractiveWindowStation->Lock, OldIrql);

    if (Target)
    {
        pti->rpdesk = Target;
    }

    return pti->rpdesk ? pti->rpdesk : GpActiveDesktop;
}

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

HDESK
VEAPI
VeaUserCreateDesktop(
    IN CONST CHAR  *lpszDesktop,
    IN CONST CHAR  *lpszDevice,
    IN PVOID        pDevmode,
    IN ULONG        dwFlags,
    IN ACCESS_MASK  dwDesiredAccess,
    IN BOOLEAN      bInheritHandle
)
{
    CHAR LocalName[64];
    CHAR FullPathBuf[128];
    ANSI_STRING DesktopAnsi;
    ANSI_STRING FullPathAnsi;
    OBJECT_ATTRIBUTES ObjAttr;
    KIRQL OldIrql;
    VEASTATUS Status;
    HDESK hDesktop = NULL;
    ULONG ObjFlags;
    PDESKTOP_OBJECT NewDesktop = NULL;
    SIZE_T NameLen;
    PCHAR NameBuf;

    UNREFERENCED_PARAMETER(lpszDevice);
    UNREFERENCED_PARAMETER(pDevmode);
    UNREFERENCED_PARAMETER(dwFlags);

    if (!GpInteractiveWindowStation ||
        !DskCopyNameFromCaller(LocalName, sizeof(LocalName), lpszDesktop))
    {
        return NULL;
    }

    RtlInitAnsiString(&DesktopAnsi, LocalName);

    KsAcquireSpinLock(&GpInteractiveWindowStation->Lock, &OldIrql);
    PDESKTOP_OBJECT Existing = DskFindDesktopLocked(GpInteractiveWindowStation, &DesktopAnsi);
    if (Existing)
    {
        ObReferenceObject(Existing);
        KsReleaseSpinLock(&GpInteractiveWindowStation->Lock, OldIrql);

        Status = ObOpenObjectByPointer(
            Existing,
            bInheritHandle ? OBJ_INHERIT : 0,
            NULL,
            dwDesiredAccess,
            UlDesktopObjectType,
            KeGetPreviousMode(),
            (PHANDLE)&hDesktop
        );
        ObDereferenceObject(Existing);

        return VEA_SUCCESS(Status) ? hDesktop : NULL;
    }
    KsReleaseSpinLock(&GpInteractiveWindowStation->Lock, OldIrql);

    RtlStringCbPrintfA(
        FullPathBuf,
        sizeof(FullPathBuf),
        "/Windows/WindowStations/WinSta0/%s",
        LocalName
    );

    RtlInitAnsiString(&FullPathAnsi, FullPathBuf);
    ObjFlags = OBJ_CASE_INSENSITIVE | OBJ_OPENIF | (bInheritHandle ? OBJ_INHERIT : 0);
    InitializeObjectAttributes(&ObjAttr, &FullPathAnsi, ObjFlags, NULL, NULL);

    Status = ObCreateObject(
        KernelMode,
        UlDesktopObjectType,
        &ObjAttr,
        KernelMode,
        NULL,
        sizeof(DESKTOP_OBJECT),
        0,
        0,
        (PVOID *)&NewDesktop
    );

    if (!VEA_SUCCESS(Status) || !NewDesktop)
    {
        return NULL;
    }

    RtlZeroMemory(NewDesktop, sizeof(DESKTOP_OBJECT));
    KsInitializeSpinlock(&NewDesktop->Lock);
    InitializeListHead(&NewDesktop->WindowListHead);
    NewDesktop->ParentWinSta = GpInteractiveWindowStation;
    NewDesktop->BackgroundColor  = 0xFF008080;

    NameLen = (SIZE_T)DesktopAnsi.Length;
    NameBuf = (PCHAR)GDI_POOL_ALLOC(NameLen + 1);
    if (!NameBuf)
    {
        ObDereferenceObject(NewDesktop);
        return NULL;
    }
    RtlCopyMemory(NameBuf, LocalName, NameLen + 1);
    RtlInitAnsiString(&NewDesktop->Name, NameBuf);

    Status = ObInsertObject(
        NewDesktop,
        NULL,
        dwDesiredAccess,
        0,
        NULL,
        (PHANDLE)&hDesktop
    );
    if (!VEA_SUCCESS(Status))
    {
        /* Catatan: jika ObInsertObject gagal, NewDesktop sudah di-dereference oleh Ob */
        return NULL;
    }

    if (Status == STATUS_SUCCESS)
    {
        KsAcquireSpinLock(&GpInteractiveWindowStation->Lock, &OldIrql);
        InsertTailList(&GpInteractiveWindowStation->DesktopListHead, &NewDesktop->ListEntry);
        KsReleaseSpinLock(&GpInteractiveWindowStation->Lock, OldIrql);

        DPRINT("[VEAPI/DSK] Created Desktop '%s' (%p, HDESK=%p)\n",
               LocalName, NewDesktop, hDesktop);
    }

    return hDesktop;
}

HDESK
VEAPI
VeaUserOpenDesktop(
    IN CONST CHAR  *lpszDesktop,
    IN ULONG        dwFlags,
    IN BOOLEAN      fInherit,
    IN ACCESS_MASK  dwDesiredAccess
)
{
    CHAR LocalName[64];
    ANSI_STRING DesktopAnsi;
    KIRQL OldIrql;
    HDESK hDesktop = NULL;

    UNREFERENCED_PARAMETER(dwFlags);

    if (!GpInteractiveWindowStation ||
        !DskCopyNameFromCaller(LocalName, sizeof(LocalName), lpszDesktop))
    {
        return NULL;
    }

    RtlInitAnsiString(&DesktopAnsi, LocalName);

    KsAcquireSpinLock(&GpInteractiveWindowStation->Lock, &OldIrql);
    PDESKTOP_OBJECT Target = DskFindDesktopLocked(GpInteractiveWindowStation, &DesktopAnsi);
    if (Target)
    {
        ObReferenceObject(Target);
    }
    KsReleaseSpinLock(&GpInteractiveWindowStation->Lock, OldIrql);

    if (!Target)
    {
        return NULL;
    }

    VEASTATUS Status = ObOpenObjectByPointer(
        Target,
        fInherit ? OBJ_INHERIT : 0,
        NULL,
        dwDesiredAccess,
        UlDesktopObjectType,
        KeGetPreviousMode(),
        (PHANDLE)&hDesktop
    );
    ObDereferenceObject(Target);

    return VEA_SUCCESS(Status) ? hDesktop : NULL;
}

BOOLEAN
VEAPI
VeaUserSetThreadDesktop(
    IN HDESK hDesktop
)
{
    PDESKTOP_OBJECT Desktop = NULL;
    VEASTATUS Status = ObReferenceObjectByHandle(
        (HANDLE)hDesktop,
        0,
        UlDesktopObjectType,
        KeGetPreviousMode(),
        (PVOID *)&Desktop
    );
    if (!VEA_SUCCESS(Status) || !Desktop)
    {
        return FALSE;
    }

    PTHREADINFO pti = (PTHREADINFO)PtGetCurrentThreadVeapiThread();
    if (!pti)
    {
        ObDereferenceObject(Desktop);
        return FALSE;
    }

    /* Kalau sudah terikat ke desktop yang sama, lepas ref ekstra dari ObReferenceObjectByHandle */
    if (pti->rpdesk == Desktop)
    {
        pti->hdesk = hDesktop;
        ObDereferenceObject(Desktop);
        return TRUE;
    }

    /* Standar Win32: thread yang sudah punya window tidak boleh pindah desktop */
    if (!IsListEmpty(&pti->WindowListHead))
    {
        ObDereferenceObject(Desktop);
        return FALSE;
    }

    if (pti->rpdesk != NULL)
    {
        ObDereferenceObject(pti->rpdesk);
    }

    pti->rpdesk = Desktop;
    pti->hdesk  = hDesktop;
    return TRUE;
}

BOOLEAN
VEAPI
VeaUserSwitchDesktop(
    IN HDESK hDesktop
)
{
    PDESKTOP_OBJECT TargetDesktop = NULL;
    KIRQL OldIrql;
    PLIST_ENTRY Entry;

    VEASTATUS Status = ObReferenceObjectByHandle(
        (HANDLE)hDesktop,
        0,
        UlDesktopObjectType,
        KeGetPreviousMode(),
        (PVOID *)&TargetDesktop
    );
    if (!VEA_SUCCESS(Status) || !TargetDesktop)
    {
        return FALSE;
    }

    if (GpActiveDesktop == TargetDesktop)
    {
        ObDereferenceObject(TargetDesktop);
        return TRUE;
    }

    if (GpActiveDesktop)
    {
        GpActiveDesktop->FocusWindow = IptFocusWindow;
    }

    if (TargetDesktop->ParentWinSta)
    {
        KsAcquireSpinLock(&TargetDesktop->ParentWinSta->Lock, &OldIrql);
        TargetDesktop->ParentWinSta->ActiveDesktop = TargetDesktop;
        KsReleaseSpinLock(&TargetDesktop->ParentWinSta->Lock, OldIrql);
    }

    GpActiveDesktop = TargetDesktop;

    /* Validasi FocusWindow milik TargetDesktop sebelum disalin ke IptFocusWindow */
    HWND RestoredFocus = NULL;
    KsAcquireSpinLock(&TargetDesktop->Lock, &OldIrql);
    PWND_OBJECT FocusObj = (PWND_OBJECT)TargetDesktop->FocusWindow;
    if (FocusObj != NULL && FocusObj->Magic == WND_OBJECT_MAGIC && FocusObj->Used)
    {
        RestoredFocus = FocusObj->hWnd;
    }
    else
    {
        TargetDesktop->FocusWindow = NULL;
    }
    KsReleaseSpinLock(&TargetDesktop->Lock, OldIrql);
    IptFocusWindow = RestoredFocus;

    ULONG ScrW = GpPrimaryDisplay.PrimarySurface.Width;
    ULONG ScrH = GpPrimaryDisplay.PrimarySurface.Height;
    if (ScrW > 0 && ScrH > 0)
    {
        EngHidePointer();
        EngFillDesktopRect(0, 0, ScrW, ScrH, WndGetDesktopBgColor(TargetDesktop));
        EngShowPointer();
    }

    gCaptureWindow = NULL;

    KsAcquireSpinLock(&gWindowListLock, &OldIrql);
    for (Entry = gWindowListHead.Flink;
        Entry != &gWindowListHead;
        Entry = Entry->Flink)
    {
        PWND_OBJECT Wnd = CONTAINING_RECORD(Entry, WND_OBJECT, GlobalListEntry);
        if (Wnd->Magic == WND_OBJECT_MAGIC &&
            Wnd->Used &&
            Wnd->Desktop == TargetDesktop &&
            WndIsEffectivelyVisibleLocked(Wnd))
        {
            Wnd->NeedsPaint = TRUE;
        }
    }
    KsReleaseSpinLock(&gWindowListLock, OldIrql);

    MsgNotifyPaintAll();
    ObDereferenceObject(TargetDesktop);

    DPRINT("[VEAPI/DSK] Switched active desktop to '%s'\n",
           TargetDesktop->Name.Buffer ? TargetDesktop->Name.Buffer : "<unnamed>");
    return TRUE;
}

BOOLEAN
VEAPI
VeaUserSetLogonNotifyWindow(
    IN HWND hWnd
)
{
    PEPROCESS CurrentProcess = (PEPROCESS)PtGetCurrentProcess();

    /* Kalau sudah ada proses Winlogon terdaftar, tolak proses lain yang mencoba membajak */
    if (GpLogonProcess != NULL && GpLogonProcess != CurrentProcess)
    {
        return FALSE;
    }

    if (hWnd == NULL)
    {
        return FALSE;
    }

    PWND_OBJECT Wnd = WndLookupHandle(hWnd);
    if (!Wnd)
    {
        return FALSE;
    }

    /* Pastikan window memang milik proses pemanggil */
    if (Wnd->OwnerProcess != CurrentProcess)
    {
        WndDereference(Wnd);
        return FALSE;
    }

    WndDereference(Wnd);

    GpLogonProcess      = CurrentProcess;
    GpLogonNotifyWindow = hWnd;

    DPRINT("[VEAPI/SAS] Registered Logon Notify Window HWND=%p (Process=%p)\n",
           hWnd, CurrentProcess);
    return TRUE;
}

/* EOF */