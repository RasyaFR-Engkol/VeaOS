/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : mouse.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Mouse handling and message
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veapi.h>

/* Revision History ------------------------------------------------------
 * DATE       : 22-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file mouse.c
 * --------------------------------------------------------------------- */

VOID
VEAPI
MsgPostMouseMessage(
    IN HWND      hWnd,
    IN ULONG     Message,
    IN ULONG_PTR wParam,
    IN LONG_PTR  lParam
)
{
    if (!hWnd) return;
    MsgPostToWindow(hWnd, Message, wParam, lParam, TRUE);
}

/* EOF */