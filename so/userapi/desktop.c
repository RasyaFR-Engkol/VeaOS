/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : desktop.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Desktop Handle
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include "userapi_priv.h"
#include <userapi.h>

/* Revision History ------------------------------------------------------
 * DATE       : 22-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file desktop.c
 * --------------------------------------------------------------------- */

HDESK
WINAPI
CreateDesktopA(
    IN PSTR                  lpszDesktop,
    IN PSTR                  lpszDevice,
    IN PVOID                 pDevmode,
    IN DWORD                 dwFlags,
    IN ACCESS_MASK           dwDesiredAccess,
    IN LPSECURITY_ATTRIBUTES lpsa
)
{
    BOOLEAN Inherit = (lpsa != NULL && lpsa->bInheritHandle) ? TRUE : FALSE;

    return VeaUserCreateDesktop(
        lpszDesktop,
        lpszDevice,
        pDevmode,
        dwFlags,
        dwDesiredAccess,
        Inherit
    );
}

HDESK WINAPI OpenDesktopA(
    IN PSTR        lpszDesktop,
    IN DWORD       dwFlags,
    IN BOOL        fInherit,
    IN ACCESS_MASK dwDesiredAccess
)
{
    return VeaUserOpenDesktop(lpszDesktop, dwFlags, fInherit ? TRUE : FALSE, dwDesiredAccess);
}

BOOL WINAPI SetThreadDesktop(IN HDESK hDesktop)
{
    return VeaUserSetThreadDesktop(hDesktop) ? TRUE : FALSE;
}

BOOL WINAPI SwitchDesktop(IN HDESK hDesktop)
{
    return VeaUserSwitchDesktop(hDesktop) ? TRUE : FALSE;
}

BOOL WINAPI CloseDesktop(IN HDESK hDesktop)
{
    // FIXME???
    // VeaClose()
    return TRUE;
}

BOOL WINAPI SetLogonNotifyWindow(IN HWND hWnd)
{
    return VeaUserSetLogonNotifyWindow(hWnd);
}

/* EOF */