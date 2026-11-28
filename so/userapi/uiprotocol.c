/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : uiprotocol.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : UI Window Proc
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <psdk/userapi.h>
#include "userapi_priv.h"

/* Revision History ------------------------------------------------------
 * DATE       : 21-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file uiprotocol.c
 * --------------------------------------------------------------------- */

/* VARIABLE ***************************************************************/

#define VEA_BORDER_GRAB         4
#define VEA_TITLE_HEIGHT        26
#define VEA_CAP_BTN_SIZE        18
#define VEA_CAP_BTN_GAP         2

static LIST_ENTRY gCtrlListHead;
static BOOLEAN    gCtrlListInitialized = FALSE;

/* PRIVATE ****************************************************************/

static VOID
CtrlEnsureListInitialized(VOID)
{
    if (!gCtrlListInitialized)
    {
        InitializeListHead(&gCtrlListHead);
        gCtrlListInitialized = TRUE;
    }
}

static PCTRL_STATE
CtrlFindState(
    IN HWND hWnd
)
{
    PLIST_ENTRY Entry;

    if (!hWnd)
    {
        return NULL;
    }

    CtrlEnsureListInitialized();

    for (Entry = gCtrlListHead.Flink;
         Entry != &gCtrlListHead;
         Entry = Entry->Flink)
    {
        PCTRL_STATE St = CONTAINING_RECORD(Entry, CTRL_STATE, ListEntry);
        if (St->hWnd == hWnd)
        {
            return St;
        }
    }

    return NULL;
}

static BOOLEAN
CtrlEnsureCapacity(
    IN PCTRL_STATE St,
    IN ULONG       NeededBytes
)
{
    if (!St)
    {
        return FALSE;
    }

    if (NeededBytes <= St->TextCapacity && St->Text != NULL)
    {
        return TRUE;
    }

    ULONG NewCap = (NeededBytes + 63) & ~63U;
    PSTR NewBuf = (PSTR)CTRL_ALLOC(NewCap);
    if (!NewBuf)
    {
        return FALSE;
    }

    if (St->Text && St->TextLen > 0)
    {
        RtlCopyMemory(NewBuf, St->Text, St->TextLen);
    }
    NewBuf[St->TextLen] = '\0';

    if (St->Text)
    {
        CTRL_FREE(St->Text);
    }

    St->Text         = NewBuf;
    St->TextCapacity = NewCap;
    return TRUE;
}

static VOID
CtrlSetText(
    IN PCTRL_STATE St,
    IN CONST CHAR *NewText
)
{
    ULONG Len = 0;

    if (!St)
    {
        return;
    }

    if (NewText)
    {
        while (NewText[Len] != '\0')
        {
            Len++;
        }
    }

    if (Len == 0)
    {
        if (St->Text && St->TextCapacity > 0)
        {
            St->Text[0] = '\0';
        }
        St->TextLen = 0;
        return;
    }

    if (!CtrlEnsureCapacity(St, Len + 1))
    {
        return;
    }

    RtlCopyMemory(St->Text, (PVOID)NewText, Len);
    St->Text[Len] = '\0';
    St->TextLen   = Len;
}

static PCTRL_STATE
CtrlCreateState(
    IN HWND   hWnd,
    IN LPARAM lParamCreate
)
{
    PCTRL_STATE Existing = CtrlFindState(hWnd);
    if (Existing)
    {
        return Existing;
    }

    PCTRL_STATE St = (PCTRL_STATE)CTRL_ALLOC(sizeof(CTRL_STATE));
    if (!St)
    {
        return NULL;
    }

    RtlZeroMemory(St, sizeof(CTRL_STATE));
    St->hWnd = hWnd;

    if (lParamCreate != 0)
    {
        LPCREATESTRUCTA cs = (LPCREATESTRUCTA)lParamCreate;
        St->hParent = cs->hwndParent;
        St->CtrlId  = (USHORT)(ULONG_PTR)cs->hMenu;
        St->Style   = (ULONG)cs->style;

        if (cs->lpszName)
        {
            CtrlSetText(St, cs->lpszName);
        }
    }

    InsertTailList(&gCtrlListHead, &St->ListEntry);
    return St;
}

static VOID
CtrlFreeState(
    IN HWND hWnd
)
{
    PCTRL_STATE St = CtrlFindState(hWnd);
    if (!St)
    {
        return;
    }

    RemoveEntryList(&St->ListEntry);

    if (St->Text)
    {
        CTRL_FREE(St->Text);
    }

    CTRL_FREE(St);
}

LRESULT
UserDefNcHitTest(
    IN HWND hWnd,
    IN LONG ScreenX,
    IN LONG ScreenY
)
{
    RECT Rc;
    if (!GetWindowRect(hWnd, &Rc))
    {
        return HTNOWHERE;
    }

    if (ScreenX < Rc.left || ScreenX >= Rc.right ||
        ScreenY < Rc.top  || ScreenY >= Rc.bottom)
    {
        return HTNOWHERE;
    }

    LONG Style = GetWindowLongA(hWnd, GWL_STYLE);

    /* Child window dan borderless popup langsung dianggap HTCLIENT */
    if ((Style & WS_CHILD) || ((Style & WS_POPUP) && ((Style & WS_CAPTION) != WS_CAPTION)))
    {
        return HTCLIENT;
    }

    BOOLEAN IsMaximized = ((Style & WS_MAXIMIZE) != 0);
    BOOLEAN CanResize   = ((Style & WS_THICKFRAME) != 0) && !IsMaximized;

    /* 1. Cek 8 sisi/sudut resize border */
    if (CanResize)
    {
        BOOLEAN OnLeft   = (ScreenX <  Rc.left   + VEA_BORDER_GRAB);
        BOOLEAN OnRight  = (ScreenX >= Rc.right  - VEA_BORDER_GRAB);
        BOOLEAN OnTop    = (ScreenY <  Rc.top    + VEA_BORDER_GRAB);
        BOOLEAN OnBottom = (ScreenY >= Rc.bottom - VEA_BORDER_GRAB);

        if (OnTop && OnLeft)     return HTTOPLEFT;
        if (OnTop && OnRight)    return HTTOPRIGHT;
        if (OnBottom && OnLeft)  return HTBOTTOMLEFT;
        if (OnBottom && OnRight) return HTBOTTOMRIGHT;
        if (OnLeft)              return HTLEFT;
        if (OnRight)             return HTRIGHT;
        if (OnTop)               return HTTOP;
        if (OnBottom)            return HTBOTTOM;
    }

    /* 2. Cek area Titlebar & 3 tombol pojok kanan atas */
    if (ScreenY >= Rc.top + VEA_BORDER_GRAB && ScreenY < Rc.top + VEA_TITLE_HEIGHT)
    {
        LONG BtnTop = Rc.top + (VEA_TITLE_HEIGHT - VEA_CAP_BTN_SIZE) / 2;
        if (ScreenY >= BtnTop && ScreenY < BtnTop + VEA_CAP_BTN_SIZE)
        {
            LONG CloseLeft = Rc.right - VEA_BORDER_GRAB - 4 - VEA_CAP_BTN_SIZE;
            LONG MaxLeft   = CloseLeft - VEA_CAP_BTN_GAP - VEA_CAP_BTN_SIZE;
            LONG MinLeft   = MaxLeft   - VEA_CAP_BTN_GAP - VEA_CAP_BTN_SIZE;

            if (ScreenX >= CloseLeft && ScreenX < CloseLeft + VEA_CAP_BTN_SIZE)
                return HTCLOSE;
            if (ScreenX >= MaxLeft && ScreenX < MaxLeft + VEA_CAP_BTN_SIZE)
                return HTMAXBUTTON;
            if (ScreenX >= MinLeft && ScreenX < MinLeft + VEA_CAP_BTN_SIZE)
                return HTMINBUTTON;
        }

        return HTCAPTION;
    }

    return HTCLIENT;
}

VOID
UserDefTrackCaptionButton(
    IN HWND   hWnd,
    IN WPARAM HitCode
)
{
    ULONG BtnId = CAPTION_BTN_NONE;
    if (HitCode == HTMINBUTTON) BtnId = CAPTION_BTN_MIN;
    if (HitCode == HTMAXBUTTON) BtnId = CAPTION_BTN_MAX;
    if (HitCode == HTCLOSE)     BtnId = CAPTION_BTN_CLOSE;
    if (BtnId == CAPTION_BTN_NONE) return;

    SetCapture(hWnd);
    VeaUserSetCaptionButtonState(hWnd, BtnId, BtnId);

    BOOLEAN Inside = TRUE;
    MSG Msg;

    while (GetMessageA(&Msg, NULL, 0, 0) > 0)
    {
        if (Msg.message == WM_MOUSEMOVE)
        {
            POINT Pt = { (SHORT)(Msg.lParam & 0xFFFF), (SHORT)((Msg.lParam >> 16) & 0xFFFF) };
            ClientToScreen(hWnd, &Pt);
            Inside = (UserDefNcHitTest(hWnd, Pt.x, Pt.y) == (LRESULT)HitCode);
            VeaUserSetCaptionButtonState(hWnd, Inside ? BtnId : CAPTION_BTN_NONE, BtnId);
        }
        else if (Msg.message == WM_LBUTTONUP)
        {
            break;
        }
        else
        {
            TranslateMessage(&Msg);
            DispatchMessageA(&Msg);
        }
    }

    VeaUserSetCaptionButtonState(hWnd, CAPTION_BTN_NONE, CAPTION_BTN_NONE);
    ReleaseCapture();

    if (Inside)
    {
        LONG Style = GetWindowLongA(hWnd, GWL_STYLE);
        if (HitCode == HTCLOSE)
            SendMessageA(hWnd, WM_SYSCOMMAND, SC_CLOSE, 0);
        else if (HitCode == HTMAXBUTTON)
            SendMessageA(hWnd, WM_SYSCOMMAND, (Style & WS_MAXIMIZE) ? SC_RESTORE : SC_MAXIMIZE, 0);
        else if (HitCode == HTMINBUTTON)
            SendMessageA(hWnd, WM_SYSCOMMAND, (Style & WS_MINIMIZE) ? SC_RESTORE : SC_MINIMIZE, 0);
    }
}

VOID
UserDefDoSizeMoveLoop(
    IN HWND   hWnd,
    IN WPARAM SysCmd,
    IN LPARAM lParam
)
{
    ULONG Cmd     = (ULONG)(SysCmd & 0xFFF0);
    ULONG HitEdge = (ULONG)(SysCmd & 0x000F);
    LONG  Style   = GetWindowLongA(hWnd, GWL_STYLE);

    if ((Style & WS_CHILD) || (Style & WS_MAXIMIZE) || (Style & WS_MINIMIZE))
    {
        return;
    }

    RECT StartRect;
    if (!GetWindowRect(hWnd, &StartRect))
    {
        return;
    }

    POINT StartPt = {
        (SHORT)(lParam & 0xFFFF),
        (SHORT)((lParam >> 16) & 0xFFFF)
    };

    SetCapture(hWnd);
    SendMessageA(hWnd, WM_ENTERSIZEMOVE, 0, 0);

    MSG Msg;
    while (GetMessageA(&Msg, NULL, 0, 0) > 0)
    {
        if (Msg.message == WM_LBUTTONUP)
        {
            break;
        }

        if (Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE)
        {
            MoveWindow(hWnd, StartRect.left, StartRect.top,
                       StartRect.right - StartRect.left,
                       StartRect.bottom - StartRect.top, TRUE);
            break;
        }

        if (Msg.message == WM_MOUSEMOVE)
        {
            POINT CurPt = {
                (SHORT)(Msg.lParam & 0xFFFF),
                (SHORT)((Msg.lParam >> 16) & 0xFFFF)
            };
            ClientToScreen(hWnd, &CurPt);

            LONG Dx = CurPt.x - StartPt.x;
            LONG Dy = CurPt.y - StartPt.y;
            RECT NewRect = StartRect;

            if (Cmd == SC_MOVE)
            {
                NewRect.left   += Dx;
                NewRect.right  += Dx;
                NewRect.top    += Dy;
                NewRect.bottom += Dy;

                if (NewRect.top < 0)
                {
                    LONG Shift = -NewRect.top;
                    NewRect.top    += Shift;
                    NewRect.bottom += Shift;
                }

                SendMessageA(hWnd, WM_MOVING, 0, (LPARAM)&NewRect);
            }
            else if (Cmd == SC_SIZE)
            {
                const LONG MinW = 140;
                const LONG MinH = 48;

                /* HitEdge: 1=Left, 2=Right, 3=Top, 4=TopLeft, 5=TopRight, 6=Bottom, 7=BottomLeft, 8=BottomRight */
                if (HitEdge == 1 || HitEdge == 4 || HitEdge == 7)
                {
                    NewRect.left += Dx;
                    if (NewRect.right - NewRect.left < MinW) NewRect.left = NewRect.right - MinW;
                }
                if (HitEdge == 2 || HitEdge == 5 || HitEdge == 8)
                {
                    NewRect.right += Dx;
                    if (NewRect.right - NewRect.left < MinW) NewRect.right = NewRect.left + MinW;
                }
                if (HitEdge == 3 || HitEdge == 4 || HitEdge == 5)
                {
                    NewRect.top += Dy;
                    if (NewRect.bottom - NewRect.top < MinH) NewRect.top = NewRect.bottom - MinH;
                }
                if (HitEdge == 6 || HitEdge == 7 || HitEdge == 8)
                {
                    NewRect.bottom += Dy;
                    if (NewRect.bottom - NewRect.top < MinH) NewRect.bottom = NewRect.top + MinH;
                }

                SendMessageA(hWnd, WM_SIZING, HitEdge, (LPARAM)&NewRect);
            }

            MoveWindow(hWnd, NewRect.left, NewRect.top,
                       NewRect.right - NewRect.left,
                       NewRect.bottom - NewRect.top, TRUE);
        }
        else
        {
            TranslateMessage(&Msg);
            DispatchMessageA(&Msg);
        }
    }

    ReleaseCapture();
    SendMessageA(hWnd, WM_EXITSIZEMOVE, 0, 0);
}

/* PUBLIC *****************************************************************/

LRESULT
CALLBACK
StaticWndProc(
    HWND hWnd,
    UINT Msg,
    WPARAM wParam,
    LPARAM lParam
)
{
    PCTRL_STATE St = (Msg == WM_CREATE)
                     ? CtrlCreateState(hWnd, lParam)
                     : CtrlFindState(hWnd);

    switch (Msg)
    {
        case WM_CREATE:
            KdPrintf("[STATIC] Created hWnd=%p text='%s'\n\r",
                     hWnd, (St && St->Text) ? St->Text : "");
            return 0;

        case WM_SETTEXT:
            CtrlSetText(St, (PSTR)lParam);
            KdPrintf("[STATIC] WM_SETTEXT hWnd=%p -> '%s'\n\r",
                     hWnd, (St && St->Text) ? St->Text : "");
            InvalidateRect(hWnd, NULL, TRUE);
            return TRUE;

        case WM_GETTEXT:
        {
            INT MaxCount = (INT)wParam;
            PSTR Dst = (PSTR)lParam;
            if (!St || !Dst || MaxCount <= 0) return 0;

            INT CopyLen = (INT)St->TextLen;
            if (CopyLen >= MaxCount) CopyLen = MaxCount - 1;
            RtlCopyMemory(Dst, St->Text, (SIZE_T)CopyLen);
            Dst[CopyLen] = '\0';
            return CopyLen;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            if (hdc)
            {
                HWND hParent = (HWND)GetWindowLongA(hWnd, GWL_HWNDPARENT);
                HBRUSH hBgBrush;

                if (hParent)
                {
                    hBgBrush = (HBRUSH)SendMessageA(
                        hParent,
                        WM_CTLCOLORSTATIC,
                        (WPARAM)hdc,
                        (LPARAM)hWnd
                    );

                    if (!hBgBrush)
                    {
                        hBgBrush = (HBRUSH)(ULONG_PTR)GetClassLongA(hParent, GCL_HBRBACKGROUND);
                    }
                }

                if (!hBgBrush)
                {
                    hBgBrush = (HBRUSH)GetStockObject(WHITE_BRUSH);
                }

                FillRect(hdc, &ps.rcPaint, hBgBrush);

                if (St && St->TextLen > 0)
                {
                    COLORREF Cr = GetTextColor(hdc);
                    ULONG TextColorARGB = 0xFF000000 |
                                          ((Cr & 0x0000FF) << 16) |
                                          (Cr & 0x00FF00) |
                                          ((Cr & 0xFF0000) >> 16);

                    VeaGdiDrawText(hdc, 0, 2, St->Text, TextColorARGB);
                }

                EndPaint(hWnd, &ps);
            }
            return 0;
        }

        case WM_NCDESTROY:
            CtrlFreeState(hWnd);
            return 0;
    }

    return DefWindowProcA(hWnd, Msg, wParam, lParam);
}

LRESULT
CALLBACK
ButtonWndProc(
    HWND hWnd,
    UINT Msg,
    WPARAM wParam,
    LPARAM lParam
)
{
    PCTRL_STATE St = (Msg == WM_CREATE)
                     ? CtrlCreateState(hWnd, lParam)
                     : CtrlFindState(hWnd);

    switch (Msg)
    {
        case WM_CREATE:
            KdPrintf("[BUTTON] Created hWnd=%p id=%u caption='%s'\n\r",
                     hWnd, St ? St->CtrlId : 0, (St && St->Text) ? St->Text : "");
            return 0;

        case WM_SETTEXT:
            CtrlSetText(St, (PSTR)lParam);
            InvalidateRect(hWnd, NULL, TRUE);
            return TRUE;

        case WM_MOUSEMOVE:
        {
            if (St)
            {
                RECT rc;
                GetClientRect(hWnd, &rc);

                INT Mx = (SHORT)(lParam & 0xFFFF);
                INT My = (SHORT)((lParam >> 16) & 0xFFFF);
                BOOLEAN NowHovered = (Mx >= rc.left && Mx < rc.right &&
                                      My >= rc.top  && My < rc.bottom);

                if (St->IsHovered != NowHovered)
                {
                    St->IsHovered = NowHovered;
                    if (NowHovered && !St->IsPressed)
                    {
                        SetTimer(hWnd, BTN_HOVER_TIMER_ID, BTN_HOVER_CHECK_MS, NULL);
                    }
                    InvalidateRect(hWnd, NULL, FALSE);
                }
            }
            return 0;
        }

        case WM_TIMER:
        {
            if (wParam == BTN_HOVER_TIMER_ID)
            {
                if (St && !St->IsPressed)
                {
                    POINT pt;
                    RECT rc;
                    GetCursorPos(&pt);
                    ScreenToClient(hWnd, &pt);
                    GetClientRect(hWnd, &rc);

                    if (pt.x < rc.left || pt.x >= rc.right ||
                        pt.y < rc.top  || pt.y >= rc.bottom)
                    {
                        St->IsHovered = FALSE;
                        KillTimer(hWnd, BTN_HOVER_TIMER_ID);
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                }
                else if (!St)
                {
                    KillTimer(hWnd, BTN_HOVER_TIMER_ID);
                }
                return 0;
            }
            break;
        }

        case WM_LBUTTONDOWN:
            if (St)
            {
                St->IsPressed = TRUE;
                St->IsHovered = TRUE;
                SetCapture(hWnd);
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;

        case WM_LBUTTONUP:
            if (St && St->IsPressed)
            {
                BOOLEAN ShouldClick = St->IsHovered;
                St->IsPressed = FALSE;
                ReleaseCapture();
                InvalidateRect(hWnd, NULL, FALSE);

                if (ShouldClick)
                {
                    KdPrintf("[BUTTON] Clicked hWnd=%p id=%u -> notify parent %p\n\r",
                             hWnd, St->CtrlId, St->hParent);

                    if (St->hParent)
                    {
                        SendMessageA(
                            St->hParent,
                            WM_COMMAND,
                            MAKEWPARAM(St->CtrlId, BN_CLICKED),
                            (LPARAM)hWnd
                        );
                    }
                }
            }
            return 0;

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            if (hdc)
            {
                if (St && ((St->Style & BS_TYPEMASK) == BS_OWNERDRAW))
                {
                    DRAWITEMSTRUCT dis;
                    RtlZeroMemory(&dis, sizeof(dis));
                    dis.CtlType    = ODT_BUTTON;
                    dis.CtlID      = St->CtrlId;
                    dis.itemID     = 0;
                    dis.itemAction = ODA_DRAWENTIRE;
                    dis.itemState  = 0;

                    if (St->IsPressed && St->IsHovered)
                    {
                        dis.itemState |= ODS_SELECTED;
                    }
                    if (St->IsHovered)
                    {
                        dis.itemState |= ODS_HOTLIGHT;
                    }
                    if (St->IsFocused)
                    {
                        dis.itemState |= ODS_FOCUS;
                    }
                    if (St->Style & WS_DISABLED)
                    {
                        dis.itemState |= ODS_DISABLED;
                    }

                    dis.hwndItem = hWnd;
                    dis.hDC      = hdc;
                    dis.rcItem   = ps.rcPaint;

                    if (St->hParent)
                    {
                        SendMessageA(
                            St->hParent,
                            WM_DRAWITEM,
                            (WPARAM)St->CtrlId,
                            (LPARAM)&dis
                        );
                    }

                    EndPaint(hWnd, &ps);
                    return 0;
                }

                /* Jalur Default: gambar tombol standar */
                INT R = ps.rcPaint.right - 1;
                INT B = ps.rcPaint.bottom - 1;
                BOOLEAN Pressed = (St && St->IsPressed && St->IsHovered);
                BOOLEAN Hovered = (St && St->IsHovered && !Pressed);

                /* 1. Isi gradasi permukaan tombol (Aqua/Silver tint) */
                RECT rcFace = { 1, 1, R, B };
                if (Pressed)
                {
                    VeaGdiGradientFill(hdc, &rcFace, 0xFFBDD3E8, 0xFFE3EEF8, TRUE);
                }
                else if (Hovered)
                {
                    VeaGdiGradientFill(hdc, &rcFace, 0xFFFAFDFF, 0xFFDFECFA, TRUE);
                }
                else
                {
                    VeaGdiGradientFill(hdc, &rcFace, 0xFFF7FAFD, 0xFFD6E2EE, TRUE);
                }

                /* 2. Border luar tombol */
                ULONG BorderCol = Pressed ? 0xFF2C628B : (Hovered ? 0xFF3C7FB1 : 0xFF707070);
                VeaGdiLineTo(hdc, 0, 0, R, 0, BorderCol);
                VeaGdiLineTo(hdc, 0, B, R, B, BorderCol);
                VeaGdiLineTo(hdc, 0, 0, 0, B, BorderCol);
                VeaGdiLineTo(hdc, R, 0, R, B, BorderCol);

                /* 3. Inner specular highlight di sisi atas */
                if (!Pressed && R > 2 && B > 2)
                {
                    VeaGdiLineTo(hdc, 1, 1, R - 1, 1, 0xFFFFFFFF);
                }

                /* 4. Render caption rata tengah berdasarkan metrik VerdanaNow.ttf */
                if (St && St->TextLen > 0)
                {
                    ULONG TextW = 0, TextH = 0;
                    VeaGdiGetTextExtent(St->Text, &TextW, &TextH);

                    INT TextX = ((INT)ps.rcPaint.right - (INT)TextW) / 2;
                    INT TextY = ((INT)ps.rcPaint.bottom - (INT)TextH) / 2;
                    if (TextX < 4) TextX = 4;
                    if (TextY < 2) TextY = 2;

                    if (Pressed)
                    {
                        TextX += 1;
                        TextY += 1;
                    }

                    VeaGdiDrawText(hdc, TextX, TextY, St->Text, 0xFF000000);
                }

                EndPaint(hWnd, &ps);
            }
            return 0;
        }

        case WM_NCDESTROY:
            KillTimer(hWnd, BTN_HOVER_TIMER_ID);
            CtrlFreeState(hWnd);
            return 0;
    }

    return DefWindowProcA(hWnd, Msg, wParam, lParam);
}

LRESULT
CALLBACK
EditWndProc(
    HWND hWnd,
    UINT Msg,
    WPARAM wParam,
    LPARAM lParam
)
{
    PCTRL_STATE St = (Msg == WM_CREATE)
                     ? CtrlCreateState(hWnd, lParam)
                     : CtrlFindState(hWnd);
    switch (Msg)
    {
        case WM_CREATE:
            KdPrintf("[EDIT] Created hWnd=%p id=%u initial='%s'\n\r",
                     hWnd, St ? St->CtrlId : 0, (St && St->Text) ? St->Text : "");
            return 0;

        case WM_SETTEXT:
            CtrlSetText(St, (PSTR)lParam);
            InvalidateRect(hWnd, NULL, TRUE);
            return TRUE;

        case WM_GETTEXT:
        {
            INT MaxCount = (INT)wParam;
            PSTR Dst = (PSTR)lParam;
            if (!St || !Dst || MaxCount <= 0) return 0;

            INT CopyLen = (INT)St->TextLen;
            if (CopyLen >= MaxCount) CopyLen = MaxCount - 1;
            if (CopyLen > 0 && St->Text)
            {
                RtlCopyMemory(Dst, St->Text, (SIZE_T)CopyLen);
            }
            Dst[CopyLen] = '\0';
            return CopyLen;
        }

        case WM_GETTEXTLENGTH:
            return St ? (LRESULT)St->TextLen : 0;

        case WM_LBUTTONDOWN:
            SetFocus(hWnd);
            if (St && St->IsFocused)
            {
                St->CaretVisible = TRUE;
                SetTimer(hWnd, EDIT_CARET_TIMER_ID, EDIT_CARET_BLINK_MS, NULL);
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;

        case WM_SETFOCUS:
            if (St)
            {
                St->IsFocused    = TRUE;
                St->CaretVisible = TRUE;
                SetTimer(hWnd, EDIT_CARET_TIMER_ID, EDIT_CARET_BLINK_MS, NULL);
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;

        case WM_KILLFOCUS:
            if (St)
            {
                St->IsFocused    = FALSE;
                St->CaretVisible = FALSE;
                KillTimer(hWnd, EDIT_CARET_TIMER_ID);
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;

        case WM_TIMER:
            if (wParam == EDIT_CARET_TIMER_ID)
            {
                if (St && St->IsFocused)
                {
                    St->CaretVisible = !St->CaretVisible;
                    InvalidateRect(hWnd, NULL, FALSE);
                }
                return 0;
            }
            break;

        case WM_CHAR:
        {
            if (!St) return 0;
            CHAR Ch = (CHAR)wParam;
            BOOLEAN TextChanged = FALSE;

            if (Ch == '\b' || Ch == 0x08)
            {
                /* Backspace */
                if (St->Text && St->TextLen > 0)
                {
                    St->TextLen--;
                    St->Text[St->TextLen] = '\0';
                    TextChanged = TRUE;
                }
            }
            else if (Ch >= 32 && Ch <= 126)
            {
                /* Alokasikan / perbesar buffer secara dinamis sebelum menulis */
                if (CtrlEnsureCapacity(St, St->TextLen + 2))
                {
                    St->Text[St->TextLen++] = Ch;
                    St->Text[St->TextLen]   = '\0';
                    TextChanged = TRUE;
                }
            }

            if (TextChanged)
            {
                St->CaretVisible = TRUE;
                SetTimer(hWnd, EDIT_CARET_TIMER_ID, EDIT_CARET_BLINK_MS, NULL);
                InvalidateRect(hWnd, NULL, FALSE);
                TextChanged = FALSE;
            }
            return 0;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            if (hdc)
            {
                INT R = ps.rcPaint.right - 1;
                INT B = ps.rcPaint.bottom - 1;

                /* 1. Latar belakang putih TextBox */
                FillRect(hdc, &ps.rcPaint, (HBRUSH)GetStockObject(WHITE_BRUSH));

                /* 2. Sunken 3D Border (gelap di atas/kiri, terang di bawah/kanan) */
                ULONG TopLeftCol = (St && St->IsFocused) ? 0xFF3D7BAD : 0xFF7A7A7A;
                VeaGdiLineTo(hdc, 0, 0, R, 0, TopLeftCol);
                VeaGdiLineTo(hdc, 0, 0, 0, B, TopLeftCol);
                VeaGdiLineTo(hdc, 0, B, R, B, 0xFFD8E4F0);
                VeaGdiLineTo(hdc, R, 0, R, B, 0xFFD8E4F0);

                /* 3. Siapkan teks tampilan (mask '*' jika ES_PASSWORD) */
                CHAR DisplayBuf[MAX_CTRL_TEXT];
                DisplayBuf[0] = '\0';
                ULONG TextW = 0, TextH = 0;

                if (St && St->Text && St->TextLen > 0)
                {
                    ULONG DispLen = St->TextLen;
                    if (DispLen >= MAX_CTRL_TEXT)
                    {
                        DispLen = MAX_CTRL_TEXT - 1;
                    }

                    if (St->Style & ES_PASSWORD)
                    {
                        for (ULONG i = 0; i < DispLen; i++) DisplayBuf[i] = '*';
                        DisplayBuf[DispLen] = '\0';
                    }
                    else
                    {
                        RtlCopyMemory(DisplayBuf, St->Text, DispLen);
                        DisplayBuf[DispLen] = '\0';
                    }

                    VeaGdiGetTextExtent(DisplayBuf, &TextW, &TextH);

                    INT TextY = ((INT)ps.rcPaint.bottom - (INT)TextH) / 2;
                    if (TextY < 3) TextY = 3;

                    VeaGdiDrawText(hdc, 6, TextY, DisplayBuf, 0xFF000000);
                }

                /* 4. Gambar garis kursor ketik (caret) hanya saat fokus aktif */
                if (St && St->IsFocused && St->CaretVisible)
                {
                    INT CaretX = 6 + (INT)TextW;
                    if (CaretX < R - 4 && B > 8)
                    {
                        VeaGdiLineTo(hdc, CaretX, 4, CaretX, B - 4, 0xFF000000);
                    }
                }

                EndPaint(hWnd, &ps);
            }
            return 0;
        }

        case WM_NCDESTROY:
            KillTimer(hWnd, EDIT_CARET_TIMER_ID);
            CtrlFreeState(hWnd);
            return 0;
    }

    return DefWindowProcA(hWnd, Msg, wParam, lParam);
}