/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : uiprotocol.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : UI Window Proc
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include  <psdk/userapi.h>

/* Revision History ------------------------------------------------------
 * DATE       : 21-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file uiprotocol.c
 * --------------------------------------------------------------------- */

#define MAX_CTRL_STATES 64
#define MAX_CTRL_TEXT   128

typedef struct _CTRL_STATE {
    BOOLEAN Used;
    HWND    hWnd;
    HWND    hParent;
    USHORT  CtrlId;
    ULONG   Style;
    BOOLEAN IsPressed;
    BOOLEAN IsFocused;
    ULONG   TextLen;
    CHAR    Text[MAX_CTRL_TEXT];
} CTRL_STATE, *PCTRL_STATE;

static CTRL_STATE gCtrlStates[MAX_CTRL_STATES];

static PCTRL_STATE
CtrlGetOrCreateState(
    IN HWND hWnd,
    IN LPARAM lParamCreate
)
{
    /* Cari jika sudah ada */
    for (ULONG i = 0; i < MAX_CTRL_STATES; i++)
    {
        if (gCtrlStates[i].Used && gCtrlStates[i].hWnd == hWnd)
        {
            return &gCtrlStates[i];
        }
    }

    /* Alokasi slot baru */
    for (ULONG i = 0; i < MAX_CTRL_STATES; i++)
    {
        if (!gCtrlStates[i].Used)
        {
            PCTRL_STATE St = &gCtrlStates[i];
            RtlZeroMemory(St, sizeof(CTRL_STATE));
            St->Used = TRUE;
            St->hWnd = hWnd;

            /* Jika kernel mengirim CREATESTRUCT lewat lParam saat WM_CREATE */
            if (lParamCreate != 0)
            {
                LPCREATESTRUCTA cs = (LPCREATESTRUCTA)lParamCreate;
                St->hParent = cs->hwndParent;
                St->CtrlId  = (USHORT)(ULONG_PTR)cs->hMenu;
                St->Style   = (ULONG)cs->style;

                if (cs->lpszName)
                {
                    ULONG Len = 0;
                    while (cs->lpszName[Len] != '\0' && Len < (MAX_CTRL_TEXT - 1))
                    {
                        St->Text[Len] = cs->lpszName[Len];
                        Len++;
                    }
                    St->Text[Len] = '\0';
                    St->TextLen   = Len;
                }
            }
            return St;
        }
    }

    return NULL;
}

static VOID
CtrlSetText(
    IN PCTRL_STATE St,
    IN PSTR NewText
)
{
    if (!St) return;

    ULONG Len = 0;
    if (NewText)
    {
        while (NewText[Len] != '\0' && Len < (MAX_CTRL_TEXT - 1))
        {
            St->Text[Len] = NewText[Len];
            Len++;
        }
    }
    St->Text[Len] = '\0';
    St->TextLen   = Len;
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
    PCTRL_STATE St = CtrlGetOrCreateState(hWnd, (Msg == WM_CREATE) ? lParam : 0);

    switch (Msg)
    {
        case WM_CREATE:
            KdPrintf("[STATIC] Created hWnd=%p text='%s'\n\r",
                     hWnd, St ? St->Text : "");
            return 0;

        case WM_SETTEXT:
            CtrlSetText(St, (PSTR)lParam);
            KdPrintf("[STATIC] WM_SETTEXT hWnd=%p -> '%s'\n\r",
                     hWnd, St ? St->Text : "");
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
                /* Clear background klasik supaya WM_SETTEXT tidak menumpuk teks lama */
                FillRect(hdc, &ps.rcPaint, 0xFFC0C0C0);

                if (St && St->TextLen > 0)
                {
                    VeaGdiDrawText(hdc, 0, 2, St->Text, 0xFF000000);
                }

                EndPaint(hWnd, &ps);
            }
            return 0;
        }
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
    PCTRL_STATE St = CtrlGetOrCreateState(hWnd, (Msg == WM_CREATE) ? lParam : 0);

    switch (Msg)
    {
        case WM_CREATE:
            KdPrintf("[BUTTON] Created hWnd=%p id=%u caption='%s'\n\r",
                     hWnd, St ? St->CtrlId : 0, St ? St->Text : "");
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
    PCTRL_STATE St = CtrlGetOrCreateState(hWnd, (Msg == WM_CREATE) ? lParam : 0);

    switch (Msg)
    {
        case WM_CREATE:
            KdPrintf("[EDIT] Created hWnd=%p id=%u initial='%s'\n\r",
                     hWnd, St ? St->CtrlId : 0, St ? St->Text : "");
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
            RtlCopyMemory(Dst, St->Text, (SIZE_T)CopyLen);
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
                if (St->TextLen > 0)
                {
                    St->TextLen--;
                    St->Text[St->TextLen] = '\0';
                    KdPrintf("[EDIT] Backspace hWnd=%p -> '%s'\n\r", hWnd, St->Text);
                    InvalidateRect(hWnd, NULL, FALSE);
                }
            }
            else if (Ch >= 32 && Ch <= 126)
            {
                /* Karakter ASCII printable */
                if (St->TextLen < (MAX_CTRL_TEXT - 1))
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
                FillRect(hdc, &ps.rcPaint, 0xFFFFFFFF);

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

                if (St && St->TextLen > 0)
                {
                    if (St->Style & ES_PASSWORD)
                    {
                        for (ULONG i = 0; i < St->TextLen; i++) DisplayBuf[i] = '*';
                        DisplayBuf[St->TextLen] = '\0';
                    }
                    else
                    {
                        RtlCopyMemory(DisplayBuf, St->Text, St->TextLen + 1);
                    }

                    VeaGdiGetTextExtent(DisplayBuf, &TextW, &TextH);

                    INT TextY = ((INT)ps.rcPaint.bottom - (INT)TextH) / 2;
                    if (TextY < 3) TextY = 3;

                    VeaGdiDrawText(hdc, 6, TextY, DisplayBuf, 0xFF000000);
                }

                /* 4. Gambar garis kursor ketik (caret) tepat di ujung lebar pixel asli */
                INT CaretX = 6 + (INT)TextW;
                if (CaretX < R - 4 && B > 8)
                {
                    VeaGdiLineTo(hdc, CaretX, 4, CaretX, B - 4, 0xFF000000);
                }

                EndPaint(hWnd, &ps);
            }
            return 0;
        }
    }

    return DefWindowProcA(hWnd, Msg, wParam, lParam);
}