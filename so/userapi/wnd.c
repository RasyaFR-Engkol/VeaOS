/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : wnd.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : UserAPI Window Management
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <userapi.h>

/* Revision History ------------------------------------------------------
 * DATE       : 23-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file wnd.c
 * --------------------------------------------------------------------- */

static BOOLEAN
UserpEnumWindowsInternal(
    IN HWND        hWndParent,
    IN BOOLEAN     bChildren,
    IN WNDENUMPROC lpEnumFunc,
    IN LPARAM      lParam
)
{
    HWND List[256];
    ULONG Needed = 0;

    if (!lpEnumFunc)
    {
        return FALSE;
    }

    VEASTATUS Status = VeaUserBuildHwndList(
        NULL,
        hWndParent,
        bChildren,
        0,
        256,
        List,
        &Needed
    );

    if (!VEA_SUCCESS(Status) && Status != STATUS_BUFFER_TOO_SMALL)
    {
        return FALSE;
    }

    ULONG Count = (Needed < 256) ? Needed : 256;

    for (ULONG i = 0; i < Count; i++)
    {
        /* Pastikan window belum di-destroy di tengah iterasi callback */
        if (!VeaUserIsWindow(List[i]))
        {
            continue;
        }

        if (!lpEnumFunc(List[i], lParam))
        {
            return FALSE;
        }
    }

    return TRUE;
}

BOOLEAN
WINAPI
EnumWindows(
    IN WNDENUMPROC lpEnumFunc,
    IN LPARAM      lParam
)
{
    return UserpEnumWindowsInternal(NULL, FALSE, lpEnumFunc, lParam);
}

BOOLEAN
WINAPI
EnumChildWindows(
    IN HWND        hWndParent,
    IN WNDENUMPROC lpEnumFunc,
    IN LPARAM      lParam
)
{
    if (!hWndParent)
    {
        return FALSE;
    }

    return UserpEnumWindowsInternal(hWndParent, TRUE, lpEnumFunc, lParam);
}

HWND
WINAPI
GetWindow(
    IN HWND hWnd,
    IN UINT uCmd
)
{
    return VeaUserGetWindow(hWnd, uCmd);
}

HWND
WINAPI
GetTopWindow(
    IN HWND hWnd
)
{
    if (!hWnd)
    {
        return NULL;
    }

    return VeaUserGetWindow(hWnd, GW_CHILD);
}

HWND
WINAPI
GetNextWindow(
    IN HWND hWnd,
    IN UINT wCmd
)
{
    if (wCmd != GW_HWNDNEXT && wCmd != GW_HWNDPREV)
    {
        return NULL;
    }

    return VeaUserGetWindow(hWnd, wCmd);
}

HWND
WINAPI
FindWindowA(
    IN CONST CHAR *lpClassName,
    IN CONST CHAR *lpWindowName
)
{
    return VeaUserFindWindowEx(NULL, NULL, lpClassName, lpWindowName);
}

HWND
WINAPI
FindWindowExA(
    IN HWND        hWndParent,
    IN HWND        hWndChildAfter,
    IN CONST CHAR *lpszClass,
    IN CONST CHAR *lpszWindow
)
{
    return VeaUserFindWindowEx(hWndParent, hWndChildAfter, lpszClass, lpszWindow);
}

HWND
WINAPI
GetForegroundWindow(VOID)
{
    return VeaUserGetForegroundWindow();
}

BOOLEAN
WINAPI
SetForegroundWindow(
    IN HWND hWnd
)
{
    return VeaUserSetForegroundWindow(hWnd);
}

INT
WINAPI
GetWindowTextLengthA(
    IN HWND hWnd
)
{
    CHAR Temp[64];
    return VeaUserGetWindowText(hWnd, Temp, sizeof(Temp));
}

BOOL
WINAPI
SetWindowTextA(
    IN HWND hWnd,
    IN PSTR lpString
)
{
    if (!hWnd)
    {
        return FALSE;
    }

    return (SendMessageA(hWnd, WM_SETTEXT, 0, (LPARAM)lpString) != 0);
}

INT
WINAPI
GetWindowTextA(
    IN  HWND hWnd,
    OUT PSTR lpString,
    IN  INT  nMaxCount
)
{
    if (!hWnd || !lpString || nMaxCount <= 0)
    {
        return 0;
    }

    /* SendMessageA mengembalikan 0 jika lintas proses (belum marshaled) */
    LRESULT Len = SendMessageA(hWnd, WM_GETTEXT, (WPARAM)nMaxCount, (LPARAM)lpString);
    if (Len > 0)
    {
        return (INT)Len;
    }

    return VeaUserGetWindowText(hWnd, lpString, nMaxCount);
}

BOOLEAN
WINAPI
IsWindow(
    IN HWND hWnd
)
{
    return VeaUserIsWindow(hWnd);
}

BOOLEAN
WINAPI
IsWindowVisible(
    IN HWND hWnd
)
{
    return VeaUserIsWindowVisible(hWnd);
}

BOOLEAN
WINAPI
GetWindowRect(
    IN  HWND  hWnd,
    OUT PRECT lpRect
)
{
    return VeaUserGetWindowRect(hWnd, lpRect);
}

BOOLEAN
WINAPI
GetClientRect(
    IN  HWND  hWnd,
    OUT PRECT lpRect
)
{
    return VeaUserGetClientRect(hWnd, lpRect);
}

BOOLEAN
WINAPI
ClientToScreen(
    IN     HWND   hWnd,
    IN OUT PPOINT lpPoint
)
{
    return VeaUserClientToScreen(hWnd, lpPoint);
}

BOOLEAN
WINAPI
ScreenToClient(
    IN     HWND   hWnd,
    IN OUT PPOINT lpPoint
)
{
    return VeaUserScreenToClient(hWnd, lpPoint);
}

INT
WINAPI
MapWindowPoints(
    IN     HWND   hWndFrom,
    IN     HWND   hWndTo,
    IN OUT PPOINT lpPoints,
    IN     UINT   cPoints
)
{
    if (!lpPoints || cPoints == 0)
    {
        return 0;
    }

    POINT OriginFrom = { 0, 0 };
    POINT OriginTo   = { 0, 0 };

    if (hWndFrom != NULL && !VeaUserClientToScreen(hWndFrom, &OriginFrom))
    {
        return 0;
    }

    if (hWndTo != NULL && !VeaUserClientToScreen(hWndTo, &OriginTo))
    {
        return 0;
    }

    LONG DeltaX = OriginFrom.x - OriginTo.x;
    LONG DeltaY = OriginFrom.y - OriginTo.y;

    for (UINT i = 0; i < cPoints; i++)
    {
        lpPoints[i].x += DeltaX;
        lpPoints[i].y += DeltaY;
    }

    return (INT)(((USHORT)DeltaY << 16) | (USHORT)DeltaX);
}

HWND
WINAPI
WindowFromPoint(
    IN POINT Point
)
{
    /* Standar Win32: terima struct POINT by value di Ring-3, pecah jadi 2 LONG untuk syscall */
    return VeaUserWindowFromPoint(Point.x, Point.y);
}

INT WINAPI GetSystemMetrics(INT nIndex)
{
    return VeaUserGetSystemMetrics(nIndex);
}

LONG WINAPI GetWindowLongA(HWND hWnd, INT nIndex)
{
    return VeaUserGetWindowLong(hWnd, nIndex);
}

LONG WINAPI SetWindowLongA(HWND hWnd, INT nIndex, LONG dwNewLong)
{
    return VeaUserSetWindowLong(hWnd, nIndex, dwNewLong);
}

BOOL WINAPI SetWindowPos(
    HWND hWnd,
    HWND hWndInsertAfter,
    INT  X,
    INT  Y,
    INT  cx,
    INT  cy,
    UINT uFlags
)
{
    return VeaUserSetWindowPos(hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags) ? TRUE : FALSE;
}

BOOL
WINAPI
MoveWindow(
    IN HWND hWnd,
    IN INT  X,
    IN INT  Y,
    IN INT  nWidth,
    IN INT  nHeight,
    IN BOOL bRepaint
)
{
    UINT Flags = SWP_NOZORDER | SWP_NOACTIVATE;

    if (!bRepaint)
    {
        Flags |= SWP_NOREDRAW;
    }

    return SetWindowPos(hWnd, NULL, X, Y, nWidth, nHeight, Flags);
}

BOOL WINAPI SetTaskmanWindow(IN HWND hWnd)
{
    return VeaUserSetTaskmanWindow(hWnd) ? TRUE : FALSE;
}

BOOL WINAPI RegisterShellHookWindow(IN HWND hWnd)
{
    return VeaUserRegisterShellHookWindow(hWnd) ? TRUE : FALSE;
}

BOOL WINAPI DeregisterShellHookWindow(IN HWND hWnd)
{
    return VeaUserDeregisterShellHookWindow(hWnd) ? TRUE : FALSE;
}

/* EOF */