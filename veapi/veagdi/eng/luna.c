/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : luna.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Window Vea Syscall
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veapi.h>

/* Revision History ------------------------------------------------------
 * DATE       : 23-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file wndapi.c
 * --------------------------------------------------------------------- */

/* PRIVATE ****************************************************************/

static 
BOOLEAN
WndGetCaptionButtonRects(
    IN  LONG   SnapX,
    IN  LONG   SnapY,
    IN  ULONG  SnapW,
    OUT PRECT  OutMinRect,
    OUT PRECT  OutMaxRect,
    OUT PRECT  OutCloseRect
)
{
    LONG BtnSize  = (WND_TITLEBAR_HEIGHT > 10) ? (WND_TITLEBAR_HEIGHT - 7) : 18;
    LONG BtnY     = SnapY + (WND_TITLEBAR_HEIGHT - BtnSize) / 2;
    LONG Gap      = 2;
    LONG RightPad = WND_BORDER_WIDTH + 3;
    LONG CloseX = SnapX + (LONG)SnapW - RightPad - BtnSize;
    LONG MaxX   = CloseX - Gap - BtnSize;
    LONG MinX   = MaxX - Gap - BtnSize;

    if (MinX <= SnapX + 28)
    {
        return FALSE;
    }

    if (OutMinRect)
    {
        OutMinRect->left   = MinX;
        OutMinRect->top    = BtnY;
        OutMinRect->right  = MinX + BtnSize;
        OutMinRect->bottom = BtnY + BtnSize;
    }
    if (OutMaxRect)
    {
        OutMaxRect->left   = MaxX;
        OutMaxRect->top    = BtnY;
        OutMaxRect->right  = MaxX + BtnSize;
        OutMaxRect->bottom = BtnY + BtnSize;
    }
    if (OutCloseRect)
    {
        OutCloseRect->left   = CloseX;
        OutCloseRect->top    = BtnY;
        OutCloseRect->right  = CloseX + BtnSize;
        OutCloseRect->bottom = BtnY + BtnSize;
    }

    return TRUE;
}

static VOID
WndDrawSingleCaptionButton(
    IN PVEAGDI_SURFACE Target,
    IN CONST RECT     *VisRects,
    IN ULONG           VisCount,
    IN CONST RECT     *BtnRect,
    IN ULONG           BtnId,
    IN ULONG           BtnState,
    IN BOOLEAN         IsActive,
    IN BOOLEAN         IsMaximized
)
{
    static const LONG BtnCornerInsets[4] = { 3, 2, 1, 1 };

    LONG X = BtnRect->left;
    LONG Y = BtnRect->top;
    LONG W = BtnRect->right - BtnRect->left;
    LONG H = BtnRect->bottom - BtnRect->top;

    if (W < 10 || H < 10)
    {
        return;
    }

    ULONG BorderColor, InnerTopColor, GradTop, GradBottom;

    if (BtnId == CAPTION_BTN_CLOSE)
    {
        if (!IsActive)
        {
            BorderColor   = 0xFF8C4A46;
            InnerTopColor = 0xFFE5A49E;
            GradTop       = 0xFFD6847C;
            GradBottom    = 0xFFB0564E;
        }
        else if (BtnState == CAPTION_STATE_PRESSED)
        {
            BorderColor   = 0xFF660600;
            InnerTopColor = 0xFF9E160B;
            GradTop       = 0xFFA61409;
            GradBottom    = 0xFFD92B1E;
        }
        else if (BtnState == CAPTION_STATE_HOVER)
        {
            BorderColor   = 0xFF8A0A00;
            InnerTopColor = 0xFFFFB2A8;
            GradTop       = 0xFFFF7566;
            GradBottom    = 0xFFDB2416;
        }
        else
        {
            BorderColor   = 0xFF7A0800;
            InnerTopColor = 0xFFF79488;
            GradTop       = 0xFFEB5242;
            GradBottom    = 0xFFC2180C;
        }
    }
    else
    {
        if (!IsActive)
        {
            BorderColor   = 0xFF5A739E;
            InnerTopColor = 0xFFD2E2F7;
            GradTop       = 0xFFA9C3E8;
            GradBottom    = 0xFF7F9ECE;
        }
        else if (BtnState == CAPTION_STATE_PRESSED)
        {
            BorderColor   = 0xFF00328A;
            InnerTopColor = 0xFF1D52B5;
            GradTop       = 0xFF1F57BF;
            GradBottom    = 0xFF4382EB;
        }
        else if (BtnState == CAPTION_STATE_HOVER)
        {
            BorderColor   = 0xFF0048CC;
            InnerTopColor = 0xFFC7E0FF;
            GradTop       = 0xFF8EC0FF;
            GradBottom    = 0xFF3E82F5;
        }
        else
        {
            BorderColor   = 0xFF003FA8;
            InnerTopColor = 0xFFA8CEFF;
            GradTop       = 0xFF6BA6FA;
            GradBottom    = 0xFF2869DE;
        }
    }

    /* 1. Background tombol dengan 4 sudut membulat (squircle) */
    for (LONG Row = 0; Row < H; Row++)
    {
        LONG Inset = 0;
        if (Row < 4)
        {
            Inset = BtnCornerInsets[Row];
        }
        else if (Row >= H - 4)
        {
            Inset = BtnCornerInsets[H - 1 - Row];
        }

        LONG RowX = X + Inset;
        LONG RowW = W - (2 * Inset);
        if (RowW <= 2)
        {
            continue;
        }

        if (Row == 0 || Row == H - 1)
        {
            WndFillClippedFrameRect(Target, VisRects, VisCount,
                                    RowX, Y + Row, (ULONG)RowW, 1, BorderColor);
        }
        else
        {
            /* Outline kiri & kanan 1px */
            WndFillClippedFrameRect(Target, VisRects, VisCount,
                                    RowX, Y + Row, 1, 1, BorderColor);
            WndFillClippedFrameRect(Target, VisRects, VisCount,
                                    RowX + RowW - 1, Y + Row, 1, 1, BorderColor);

            ULONG FillColor = (Row == 1)
                              ? InnerTopColor
                              : WndLerpColorARGB(GradTop, GradBottom, Row - 2, H - 4);

            WndFillClippedFrameRect(Target, VisRects, VisCount,
                                    RowX + 1, Y + Row, (ULONG)(RowW - 2), 1, FillColor);
        }
    }

    /* 2. Gambar glyph ikon putih (geser +1px saat ditekan) */
    LONG Shift = (BtnState == CAPTION_STATE_PRESSED) ? 1 : 0;
    ULONG GlyphColor = IsActive ? 0xFFFFFFFF : 0xFFEBF2FC;

    if (BtnId == CAPTION_BTN_MIN)
    {
        LONG BarW = (W * 48) / 100;
        LONG BarH = 3;
        LONG BarX = X + (W - BarW) / 2 + Shift;
        LONG BarY = Y + (H - BarH) / 2 + Shift;
        WndFillClippedFrameRect(Target, VisRects, VisCount,
                                BarX, BarY, (ULONG)BarW, (ULONG)BarH, GlyphColor);
    }
    else if (BtnId == CAPTION_BTN_MAX)
    {
        if (!IsMaximized)
        {
            LONG BoxW = (W * 48) / 100;
            LONG BoxH = (H * 48) / 100;
            if (BoxW < 8) BoxW = 8;
            if (BoxH < 8) BoxH = 8;
            LONG BoxX = X + (W - BoxW) / 2 + Shift;
            LONG BoxY = Y + (H - BoxH) / 2 + Shift;

            /* Atas (2px), Bawah (2px), Kiri (2px), Kanan (2px) */
            WndFillClippedFrameRect(Target, VisRects, VisCount, BoxX, BoxY, (ULONG)BoxW, 2, GlyphColor);
            WndFillClippedFrameRect(Target, VisRects, VisCount, BoxX, BoxY + BoxH - 2, (ULONG)BoxW, 2, GlyphColor);
            WndFillClippedFrameRect(Target, VisRects, VisCount, BoxX, BoxY, 2, (ULONG)BoxH, GlyphColor);
            WndFillClippedFrameRect(Target, VisRects, VisCount, BoxX + BoxW - 2, BoxY, 2, (ULONG)BoxH, GlyphColor);
        }
        else
        {
            /* Ikon Restore (dua kotak bertumpuk) */
            LONG SubW = 7;
            LONG SubH = 7;
            LONG FrontX = X + (W - 9) / 2 + Shift;
            LONG FrontY = Y + (H - 9) / 2 + 2 + Shift;
            LONG BackX  = FrontX + 2;
            LONG BackY  = FrontY - 2;

            /* Kotak belakang (bagian yang menyembul di atas & kanan) */
            WndFillClippedFrameRect(Target, VisRects, VisCount, BackX, BackY, (ULONG)SubW, 2, GlyphColor);
            WndFillClippedFrameRect(Target, VisRects, VisCount, BackX + SubW - 2, BackY, 2, (ULONG)SubH, GlyphColor);

            /* Kotak depan */
            WndFillClippedFrameRect(Target, VisRects, VisCount, FrontX, FrontY, (ULONG)SubW, 2, GlyphColor);
            WndFillClippedFrameRect(Target, VisRects, VisCount, FrontX, FrontY + SubH - 2, (ULONG)SubW, 2, GlyphColor);
            WndFillClippedFrameRect(Target, VisRects, VisCount, FrontX, FrontY, 2, (ULONG)SubH, GlyphColor);
            WndFillClippedFrameRect(Target, VisRects, VisCount, FrontX + SubW - 2, FrontY, 2, (ULONG)SubH, GlyphColor);
        }
    }
    else if (BtnId == CAPTION_BTN_CLOSE)
    {
        LONG Span = (W * 44) / 100;
        if (Span < 8) Span = 8;
        LONG StartX = X + (W - Span) / 2 + Shift;
        LONG StartY = Y + (H - Span) / 2 + Shift;

        for (LONG i = 0; i < Span; i++)
        {
            /* Diagonal utama (\) tebal 2px */
            WndFillClippedFrameRect(Target, VisRects, VisCount,
                                    StartX + i, StartY + i, 2, 1, GlyphColor);
            /* Diagonal silang (/) tebal 2px */
            WndFillClippedFrameRect(Target, VisRects, VisCount,
                                    StartX + (Span - 1 - i), StartY + i, 2, 1, GlyphColor);
        }
    }
}

static VOID
WndDrawCaptionButtons(
    IN PVEAGDI_SURFACE Target,
    IN PWND_OBJECT     Wnd,
    IN CONST RECT     *FrameVis,
    IN ULONG           FrameVisCount,
    IN LONG            SnapX,
    IN LONG            SnapY,
    IN ULONG           SnapW,
    IN BOOLEAN         IsActive
)
{
    RECT MinRect, MaxRect, CloseRect;
    if (!WndGetCaptionButtonRects(SnapX, SnapY, SnapW, &MinRect, &MaxRect, &CloseRect))
    {
        return;
    }

    BOOLEAN IsMaximized = ((Wnd->Style & WS_MAXIMIZE) != 0);

    WndDrawSingleCaptionButton(
        Target, FrameVis, FrameVisCount, &MinRect,
        CAPTION_BTN_MIN,
        WndGetCaptionButtonVisualState(Wnd->hWnd, CAPTION_BTN_MIN),
        IsActive, IsMaximized
    );
    WndDrawSingleCaptionButton(
        Target, FrameVis, FrameVisCount, &MaxRect,
        CAPTION_BTN_MAX,
        WndGetCaptionButtonVisualState(Wnd->hWnd, CAPTION_BTN_MAX),
        IsActive, IsMaximized
    );
    WndDrawSingleCaptionButton(
        Target, FrameVis, FrameVisCount, &CloseRect,
        CAPTION_BTN_CLOSE,
        WndGetCaptionButtonVisualState(Wnd->hWnd, CAPTION_BTN_CLOSE),
        IsActive, IsMaximized
    );
}

ULONG
WndLerpColorARGB(
    IN ULONG C1,
    IN ULONG C2,
    IN LONG  Num,
    IN LONG  Denom
)
{
    if (Denom <= 0 || Num <= 0) return C1;
    if (Num >= Denom) return C2;

    LONG R1 = (C1 >> 16) & 0xFF, G1 = (C1 >> 8) & 0xFF, B1 = C1 & 0xFF;
    LONG R2 = (C2 >> 16) & 0xFF, G2 = (C2 >> 8) & 0xFF, B2 = C2 & 0xFF;

    ULONG R = (ULONG)(R1 + ((R2 - R1) * Num) / Denom);
    ULONG G = (ULONG)(G1 + ((G2 - G1) * Num) / Denom);
    ULONG B = (ULONG)(B1 + ((B2 - B1) * Num) / Denom);

    return 0xFF000000 | (R << 16) | (G << 8) | B;
}

static VOID
WndDrawDefaultTitleIcon(
    IN PVEAGDI_SURFACE Target,
    IN CONST RECT     *VisRects,
    IN ULONG           VisCount,
    IN LONG            IconX,
    IN LONG            IconY
)
{
    /* Kotak putih 16x16 dengan emblem biru/emas sesuai mockup */
    WndFillClippedFrameRect(Target, VisRects, VisCount, IconX, IconY, 16, 16, 0xFFFFFFFF);
    WndFillClippedFrameRect(Target, VisRects, VisCount, IconX + 2, IconY + 2, 12, 12, 0xFF1F5CE0);
    WndFillClippedFrameRect(Target, VisRects, VisCount, IconX + 3, IconY + 3, 10, 10, 0xFFF5B800);
    WndFillClippedFrameRect(Target, VisRects, VisCount, IconX + 5, IconY + 5, 6, 8, 0xFF1F5CE0);
    WndFillClippedFrameRect(Target, VisRects, VisCount, IconX + 7, IconY + 4, 2, 9, 0xFFF5B800);
}

static ULONG
WndGetCornerBackdropColor(
    IN PWND_OBJECT Wnd,
    IN LONG        ScreenX,
    IN LONG        ScreenY
)
{
    KIRQL OldIrql;
    PLIST_ENTRY Entry;
    PWND_OBJECT BelowWnd = NULL;
    ULONG Color; 
    if (!Wnd)
    {
        return WndGetDesktopBgColor(GpActiveDesktop);
    }

    Color = WndGetDesktopBgColor(Wnd->Desktop ? Wnd->Desktop : GpActiveDesktop);

    KsAcquireSpinLock(&gWindowListLock, &OldIrql);

    PWND_OBJECT TopSelf = Wnd;
    ULONG Depth = 0;
    while (TopSelf && (TopSelf->Style & WS_CHILD) && TopSelf->hWndParent != NULL && Depth < 64)
    {
        PWND_OBJECT Parent = WndLookupHandleLocked(TopSelf->hWndParent);
        if (!Parent) break;
        TopSelf = Parent;
        Depth++;
    }

    if (TopSelf && TopSelf->Magic == WND_OBJECT_MAGIC && TopSelf->Used)
    {
        /* Telusuri window di bawah TopSelf dalam Z-order (arah Blink) */
        for (Entry = TopSelf->GlobalListEntry.Blink;
             Entry != &gWindowListHead;
             Entry = Entry->Blink)
        {
            PWND_OBJECT Cand = CONTAINING_RECORD(Entry, WND_OBJECT, GlobalListEntry);
            if (Cand->Magic != WND_OBJECT_MAGIC || !Cand->Used || (Cand->Style & WS_CHILD))
            {
                continue;
            }
            if (Cand->Desktop != TopSelf->Desktop || !(Cand->Style & WS_VISIBLE))
            {
                continue;
            }

            if (ScreenX >= Cand->X && ScreenX < Cand->X + (LONG)Cand->Width &&
                ScreenY >= Cand->Y && ScreenY < Cand->Y + (LONG)Cand->Height)
            {
                WndReference(Cand);
                BelowWnd = Cand;
                break;
            }
        }
    }

    KsReleaseSpinLock(&gWindowListLock, OldIrql);

    if (BelowWnd)
    {
        BOOLEAN BelowHasFrame = !((BelowWnd->Style & WS_POPUP) &&
                                  ((BelowWnd->Style & WS_CAPTION) != WS_CAPTION));

        if (BelowHasFrame && ScreenY < BelowWnd->Y + WND_TITLEBAR_HEIGHT)
        {
            BOOLEAN BelowActive = (GpActiveDesktop != NULL &&
                                   GpActiveDesktop->ForegroundWindow == BelowWnd);
            Color = BelowActive ? 0xFF3A78E8 : 0xFF82A0CE;
        }
        else if (BelowHasFrame &&
                 (ScreenX < BelowWnd->X + WND_BORDER_WIDTH ||
                  ScreenX >= BelowWnd->X + (LONG)BelowWnd->Width - WND_BORDER_WIDTH ||
                  ScreenY >= BelowWnd->Y + (LONG)BelowWnd->Height - WND_BORDER_WIDTH))
        {
            BOOLEAN BelowActive = (GpActiveDesktop != NULL &&
                                   GpActiveDesktop->ForegroundWindow == BelowWnd);
            Color = BelowActive ? 0xFF245EDC : 0xFF7B99C9;
        }
        else if (BelowWnd->hbrBackground != NULL)
        {
            GdipResolveBrushColor(BelowWnd->hbrBackground, &Color);
        }
        else
        {
            Color = 0xFFFFFFFF;
        }

        WndDereference(BelowWnd);
    }

    return Color;
}

static VOID
WndDrawLunaTitleBar(
    IN PVEAGDI_SURFACE Target,
    IN PWND_OBJECT     Wnd,
    IN CONST RECT     *FrameVis,
    IN ULONG           FrameVisCount,
    IN LONG            SnapX,
    IN LONG            SnapY,
    IN ULONG           SnapW,
    IN BOOLEAN         IsActive
)
{
    static const LONG CornerInsets[WND_CORNER_RADIUS] = { 5, 3, 2, 1, 1, 0 };

    ULONG OuterOutline = IsActive ? 0xFF15429E : 0xFF5A739E;
    ULONG TopHighlight = IsActive ? 0xFF82BAFF : 0xFFB8CCE8;
    ULONG TitleTop     = IsActive ? 0xFF5498FA : 0xFF96B2DC;
    ULONG TitleBottom  = IsActive ? 0xFF215AD8 : 0xFF6E8EC2;
    ULONG LeftCornerBg  = WndGetCornerBackdropColor(Wnd, SnapX, SnapY);
    ULONG RightCornerBg = WndGetCornerBackdropColor(Wnd, SnapX + (LONG)SnapW - 1, SnapY);

    /* 1. Background Titlebar + Rounded Top Corners */
    for (LONG Row = 0; Row < WND_TITLEBAR_HEIGHT; Row++)
    {
        LONG Inset = (Row < WND_CORNER_RADIUS) ? CornerInsets[Row] : 0;
        if (Inset > 0)
        {
            WndFillClippedFrameRect(Target, FrameVis, FrameVisCount,
                                    SnapX, SnapY + Row, (ULONG)Inset, 1, LeftCornerBg);
            WndFillClippedFrameRect(Target, FrameVis, FrameVisCount,
                                    SnapX + (LONG)SnapW - Inset, SnapY + Row, (ULONG)Inset, 1, RightCornerBg);
        }
        
        LONG RowX  = SnapX + Inset;
        LONG RowW  = (LONG)SnapW - (2 * Inset);
        if (RowW <= 2)
        {
            continue;
        }

        if (Row == 0)
        {
            WndFillClippedFrameRect(Target, FrameVis, FrameVisCount,
                                    RowX, SnapY, (ULONG)RowW, 1, OuterOutline);
        }
        else
        {
            WndFillClippedFrameRect(Target, FrameVis, FrameVisCount,
                                    RowX, SnapY + Row, 1, 1, OuterOutline);
            WndFillClippedFrameRect(Target, FrameVis, FrameVisCount,
                                    RowX + RowW - 1, SnapY + Row, 1, 1, OuterOutline);

            ULONG FillColor;
            if (Row == 1)
            {
                FillColor = TopHighlight;
            }
            else if (Row == WND_TITLEBAR_HEIGHT - 1)
            {
                FillColor = OuterOutline;
            }
            else
            {
                FillColor = WndLerpColorARGB(
                    TitleTop,
                    TitleBottom,
                    Row - 2,
                    WND_TITLEBAR_HEIGHT - 4
                );
            }

            WndFillClippedFrameRect(Target, FrameVis, FrameVisCount,
                                    RowX + 1, SnapY + Row, (ULONG)(RowW - 2), 1, FillColor);
        }
    }

    /* 2. Icon 16x16, Teks Judul, dan Caption Buttons */
    LONG IconX = SnapX + 7;
    LONG IconY = SnapY + (WND_TITLEBAR_HEIGHT - 16) / 2;
    WndDrawDefaultTitleIcon(Target, FrameVis, FrameVisCount, IconX, IconY);

    RECT MinRect, MaxRect, CloseRect;
    BOOLEAN HasButtons = WndGetCaptionButtonRects(SnapX, SnapY, SnapW, &MinRect, &MaxRect, &CloseRect);

    LONG TextX = IconX + 16 + 6;
    LONG TextRightLimit = HasButtons ? (MinRect.left - 6) : (SnapX + (LONG)SnapW - 8);
    ULONG TextAreaW = (TextRightLimit > TextX) ? (ULONG)(TextRightLimit - TextX) : 0;

    WndDrawTitleBarText(Wnd, TextX, SnapY + 1, TextAreaW, WND_TITLEBAR_HEIGHT - 2);

    if (HasButtons)
    {
        WndDrawCaptionButtons(Target, Wnd, FrameVis, FrameVisCount, SnapX, SnapY, SnapW, IsActive);
    }
}

VOID
WndDrawLunaWindowFrame(
    IN PVEAGDI_SURFACE Target,
    IN PWND_OBJECT     Wnd,
    IN CONST RECT     *FrameVis,
    IN ULONG           FrameVisCount,
    IN LONG            SnapX,
    IN LONG            SnapY,
    IN ULONG           SnapW,
    IN ULONG           SnapH,
    IN BOOLEAN         IsActive
)
{
    if (SnapW < 16 || SnapH < WND_TITLEBAR_HEIGHT + WND_BORDER_WIDTH)
    {
        return;
    }

    ULONG OuterOutline = IsActive ? 0xFF15429E : 0xFF5A739E;
    ULONG BorderMain   = IsActive ? 0xFF245EDC : 0xFF7B99C9;

    /* 1 & 2. Titlebar */
    WndDrawLunaTitleBar(Target, Wnd, FrameVis, FrameVisCount, SnapX, SnapY, SnapW, IsActive);

    /* 3. Border Kiri, Kanan, dan Bawah */
    LONG BodyY = SnapY + WND_TITLEBAR_HEIGHT;
    ULONG BodyH = (SnapH > WND_TITLEBAR_HEIGHT) ? (SnapH - WND_TITLEBAR_HEIGHT) : 0;

    if (BodyH > 0)
    {
        WndFillClippedFrameRect(Target, FrameVis, FrameVisCount,
                                SnapX, BodyY, 1, BodyH, OuterOutline);
        WndFillClippedFrameRect(Target, FrameVis, FrameVisCount,
                                SnapX + 1, BodyY, WND_BORDER_WIDTH - 1, BodyH, BorderMain);

        LONG RightBorderX = SnapX + (LONG)SnapW - WND_BORDER_WIDTH;
        WndFillClippedFrameRect(Target, FrameVis, FrameVisCount,
                                RightBorderX, BodyY, WND_BORDER_WIDTH - 1, BodyH, BorderMain);
        WndFillClippedFrameRect(Target, FrameVis, FrameVisCount,
                                SnapX + (LONG)SnapW - 1, BodyY, 1, BodyH, OuterOutline);

        LONG BottomBorderY = SnapY + (LONG)SnapH - WND_BORDER_WIDTH;
        ULONG InnerSpanW = (SnapW > 2) ? (SnapW - 2) : 0;
        WndFillClippedFrameRect(Target, FrameVis, FrameVisCount,
                                SnapX + 1, BottomBorderY, InnerSpanW, WND_BORDER_WIDTH - 1, BorderMain);
        WndFillClippedFrameRect(Target, FrameVis, FrameVisCount,
                                SnapX, SnapY + (LONG)SnapH - 1, SnapW, 1, OuterOutline);
    }
}

/* PUBLIC *****************************************************************/

ULONG
WndHitTestCaptionButton(
    IN PWND_OBJECT Wnd,
    IN LONG        ScreenX,
    IN LONG        ScreenY
)
{
    RECT MinRect, MaxRect, CloseRect;

    if (!Wnd || (Wnd->Style & WS_CHILD))
    {
        return CAPTION_BTN_NONE;
    }

    if ((Wnd->Style & WS_POPUP) && ((Wnd->Style & WS_CAPTION) != WS_CAPTION))
    {
        return CAPTION_BTN_NONE;
    }

    if (!WndGetCaptionButtonRects(Wnd->X, Wnd->Y, Wnd->Width, &MinRect, &MaxRect, &CloseRect))
    {
        return CAPTION_BTN_NONE;
    }

    if (ScreenX >= CloseRect.left && ScreenX < CloseRect.right &&
        ScreenY >= CloseRect.top  && ScreenY < CloseRect.bottom)
    {
        return CAPTION_BTN_CLOSE;
    }

    if (ScreenX >= MaxRect.left && ScreenX < MaxRect.right &&
        ScreenY >= MaxRect.top  && ScreenY < MaxRect.bottom)
    {
        return CAPTION_BTN_MAX;
    }

    if (ScreenX >= MinRect.left && ScreenX < MinRect.right &&
        ScreenY >= MinRect.top  && ScreenY < MinRect.bottom)
    {
        return CAPTION_BTN_MIN;
    }

    return CAPTION_BTN_NONE;
}

VOID
WndRedrawCaptionButtons(
    IN PWND_OBJECT Wnd
)
{
    PVEAGDI_SURFACE Target = EngGetTargetSurface();
    RECT MinRect, MaxRect, CloseRect;

    if (!Target || !Wnd || !(Wnd->Style & WS_VISIBLE))
    {
        return;
    }

    if (GpActiveDesktop != NULL && Wnd->Desktop != GpActiveDesktop)
    {
        return;
    }

    if (!WndGetCaptionButtonRects(Wnd->X, Wnd->Y, Wnd->Width, &MinRect, &MaxRect, &CloseRect))
    {
        return;
    }

    RECT BtnGroupBounds = {
        MinRect.left,
        MinRect.top,
        CloseRect.right,
        CloseRect.bottom
    };

    RECT FrameVis[MAX_VISIBLE_CLIP_RECTS];
    ULONG FrameVisCount = WndComputeVisibleRects(
        Wnd,
        &BtnGroupBounds,
        FrameVis,
        MAX_VISIBLE_CLIP_RECTS
    );

    if (FrameVisCount == 0)
    {
        return;
    }

    BOOLEAN IsActive = (GpActiveDesktop == NULL ||
                        GpActiveDesktop->ForegroundWindow == Wnd);

    EngHidePointer();
    WndDrawCaptionButtons(Target, Wnd, FrameVis, FrameVisCount, Wnd->X, Wnd->Y, Wnd->Width, IsActive);
    EngShowPointer();

    for (ULONG i = 0; i < FrameVisCount; i++)
    {
        EngPresentRect(
            FrameVis[i].left,
            FrameVis[i].top,
            (ULONG)(FrameVis[i].right - FrameVis[i].left),
            (ULONG)(FrameVis[i].bottom - FrameVis[i].top)
        );
    }
}

VOID
WndRedrawTitleBar(
    IN PWND_OBJECT Wnd
)
{
    PVEAGDI_SURFACE Target = EngGetTargetSurface();
    KIRQL OldIrql;

    if (!Target || !Wnd || !(Wnd->Style & WS_VISIBLE) || (Wnd->Style & WS_CHILD))
    {
        return;
    }

    if ((Wnd->Style & WS_POPUP) && ((Wnd->Style & WS_CAPTION) != WS_CAPTION))
    {
        return;
    }

    if (GpActiveDesktop != NULL && Wnd->Desktop != GpActiveDesktop)
    {
        return;
    }

    KsAcquireSpinLock(&gWindowListLock, &OldIrql);
    LONG  SnapX = Wnd->X;
    LONG  SnapY = Wnd->Y;
    ULONG SnapW = Wnd->Width;
    ULONG SnapH = Wnd->Height;
    KsReleaseSpinLock(&gWindowListLock, OldIrql);

    if (SnapW < 16 || SnapH < WND_TITLEBAR_HEIGHT + WND_BORDER_WIDTH)
    {
        return;
    }

    RECT TitleBounds = {
        SnapX,
        SnapY,
        SnapX + (LONG)SnapW,
        SnapY + WND_TITLEBAR_HEIGHT
    };

    RECT FrameVis[MAX_VISIBLE_CLIP_RECTS];
    ULONG FrameVisCount = WndComputeVisibleRects(
        Wnd,
        &TitleBounds,
        FrameVis,
        MAX_VISIBLE_CLIP_RECTS
    );

    if (FrameVisCount == 0)
    {
        return;
    }

    BOOLEAN IsActive = (GpActiveDesktop == NULL ||
                        GpActiveDesktop->ForegroundWindow == Wnd);

    EngHidePointer();
    WndDrawLunaTitleBar(Target, Wnd, FrameVis, FrameVisCount, SnapX, SnapY, SnapW, IsActive);
    EngShowPointer();

    for (ULONG i = 0; i < FrameVisCount; i++)
    {
        EngPresentRect(
            FrameVis[i].left,
            FrameVis[i].top,
            (ULONG)(FrameVis[i].right - FrameVis[i].left),
            (ULONG)(FrameVis[i].bottom - FrameVis[i].top)
        );
    }
}

/* EOF */