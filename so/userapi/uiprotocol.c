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

static LIST_ENTRY gCtrlListHead;
static BOOLEAN    gCtrlListInitialized = FALSE;

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

        case WM_LBUTTONDOWN:
            if (St)
            {
                St->IsPressed = TRUE;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;

        case WM_LBUTTONUP:
            if (St && St->IsPressed)
            {
                St->IsPressed = FALSE;
                InvalidateRect(hWnd, NULL, FALSE);

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
            return 0;

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            if (hdc)
            {
                INT R = ps.rcPaint.right - 1;
                INT B = ps.rcPaint.bottom - 1;
                BOOLEAN Pressed = (St && St->IsPressed);

                /* 1. Isi gradasi permukaan tombol (Aqua/Silver tint) */
                RECT rcFace = { 1, 1, R, B };
                if (!Pressed)
                {
                    VeaGdiGradientFill(hdc, &rcFace, 0xFFF7FAFD, 0xFFD6E2EE, TRUE);
                }
                else
                {
                    VeaGdiGradientFill(hdc, &rcFace, 0xFFBDD3E8, 0xFFE3EEF8, TRUE);
                }

                /* 2. Border luar tombol */
                ULONG BorderCol = Pressed ? 0xFF2C628B : 0xFF707070;
                VeaGdiLineTo(hdc, 0, 0, R, 0, BorderCol);
                VeaGdiLineTo(hdc, 0, B, R, B, BorderCol);
                VeaGdiLineTo(hdc, 0, 0, 0, B, BorderCol);
                VeaGdiLineTo(hdc, R, 0, R, B, BorderCol);

                /* 3. Inner specular highlight di sisi atas (khas Aero) */
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
            return 0;

        case WM_SETFOCUS:
            if (St)
            {
                St->IsFocused = TRUE;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;

        case WM_KILLFOCUS:
            if (St)
            {
                St->IsFocused = FALSE;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;

        case WM_CHAR:
        {
            if (!St) return 0;
            CHAR Ch = (CHAR)wParam;

            if (Ch == '\b' || Ch == 0x08)
            {
                /* Backspace */
                if (St->Text && St->TextLen > 0)
                {
                    St->TextLen--;
                    St->Text[St->TextLen] = '\0';
                    KdPrintf("[EDIT] Backspace hWnd=%p -> '%s'\n\r", hWnd, St->Text);
                    InvalidateRect(hWnd, NULL, FALSE);
                }
            }
            else if (Ch >= 32 && Ch <= 126)
            {
                /* Alokasikan / perbesar buffer secara dinamis sebelum menulis */
                if (CtrlEnsureCapacity(St, St->TextLen + 2))
                {
                    St->Text[St->TextLen++] = Ch;
                    St->Text[St->TextLen]   = '\0';
                    KdPrintf("[EDIT] Input hWnd=%p -> '%s'\n\r", hWnd, St->Text);
                    InvalidateRect(hWnd, NULL, FALSE);
                }
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
                if (St && St->IsFocused)
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
            CtrlFreeState(hWnd);
            return 0;
    }

    return DefWindowProcA(hWnd, Msg, wParam, lParam);
}