/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : class.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : CLASS OF UI
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <psdk/userapi.h>
#include "userapi_priv.h"

/* Revision History ------------------------------------------------------
 * DATE       : 21-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file class.c
 * --------------------------------------------------------------------- */

BOOLEAN gSystemControlsRegistered = FALSE;
static ATOM gNextClassAtom = 0xC001;

ATOM
WINAPI
RegisterClassA(
    IN CONST WNDCLASSA *lpWndClass
)
{
    WNDCLASS VeaClass;

    if (!lpWndClass || !lpWndClass->lpfnWndProc || !lpWndClass->lpszClassName)
    {
        return 0;
    }

    RtlZeroMemory(&VeaClass, sizeof(WNDCLASS));
    VeaClass.Style         = lpWndClass->style;
    VeaClass.lpfnWndProc   = (PVOID)lpWndClass->lpfnWndProc;
    VeaClass.cbClsExtra    = lpWndClass->cbClsExtra;
    VeaClass.cbWndExtra    = lpWndClass->cbWndExtra;
    VeaClass.hInstance     = lpWndClass->hInstance;
    VeaClass.hIcon         = lpWndClass->hIcon;
    VeaClass.hCursor       = lpWndClass->hCursor;
    VeaClass.hbrBackground = lpWndClass->hbrBackground;
    VeaClass.lpszMenuName  = (PCHAR)lpWndClass->lpszMenuName;
    VeaClass.lpszClassName = (PCHAR)lpWndClass->lpszClassName;

    if (!VeaUserRegisterClass(&VeaClass))
    {
        return 0;
    }

    return gNextClassAtom++;
}

ATOM
WINAPI
RegisterClassExA(
    IN CONST WNDCLASSEXA *lpwcx
)
{
    WNDCLASSA wc;

    if (!lpwcx || lpwcx->cbSize != sizeof(WNDCLASSEXA))
    {
        return 0;
    }

    RtlZeroMemory(&wc, sizeof(wc));
    wc.style         = lpwcx->style;
    wc.lpfnWndProc   = lpwcx->lpfnWndProc;
    wc.cbClsExtra    = lpwcx->cbClsExtra;
    wc.cbWndExtra    = lpwcx->cbWndExtra;
    wc.hInstance     = lpwcx->hInstance;
    wc.hIcon         = lpwcx->hIcon;
    wc.hCursor       = lpwcx->hCursor;
    wc.hbrBackground = lpwcx->hbrBackground;
    wc.lpszMenuName  = lpwcx->lpszMenuName;
    wc.lpszClassName = lpwcx->lpszClassName;

    return RegisterClassA(&wc);
}

VOID
UserapiRegisterSystemControls(VOID)
{
    WNDCLASSA wc;

    if (gSystemControlsRegistered)
    {
        return;
    }

    /* Set TRUE duluan supaya pemanggilan RegisterClassA di bawah tidak rekursi tak terbatas */
    gSystemControlsRegistered = TRUE;

    /* 1. Label ("STATIC") */
    RtlZeroMemory(&wc, sizeof(wc));
    wc.style         = CS_GLOBALCLASS;
    wc.lpfnWndProc   = StaticWndProc;
    wc.lpszClassName = "STATIC";
    RegisterClassA(&wc);

    /* 2. TextBox ("EDIT") */
    RtlZeroMemory(&wc, sizeof(wc));
    wc.style         = CS_GLOBALCLASS;
    wc.lpfnWndProc   = EditWndProc;
    wc.lpszClassName = "EDIT";
    RegisterClassA(&wc);

    /* 3. Button ("BUTTON") */
    RtlZeroMemory(&wc, sizeof(wc));
    wc.style         = CS_GLOBALCLASS;
    wc.lpfnWndProc   = ButtonWndProc;
    wc.lpszClassName = "BUTTON";
    RegisterClassA(&wc);
}

DWORD
WINAPI
GetClassLongA(
    IN HWND hWnd,
    IN INT  nIndex
)
{
    if (!hWnd)
    {
        return 0;
    }

    return (DWORD)VeaUserGetClassLong(hWnd, nIndex);
}

DWORD
WINAPI
SetClassLongA(
    IN HWND hWnd,
    IN INT  nIndex,
    IN LONG dwNewLong
)
{
    if (!hWnd)
    {
        return 0;
    }

    return (DWORD)VeaUserSetClassLong(hWnd, nIndex, dwNewLong);
}

/* EOF */