/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : paint.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Vea GDI Painting 
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veapi.h>

/* Revision History ------------------------------------------------------
 * DATE       : 21-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file paint.c
 * --------------------------------------------------------------------- */

static ULONG
WndSubtractRect(
    IN  CONST RECT *Src,
    IN  CONST RECT *Occ,
    OUT PRECT       Parts
)
{
    if (Src->left >= Occ->right || Src->right <= Occ->left ||
        Src->top >= Occ->bottom || Src->bottom <= Occ->top)
    {
        Parts[0] = *Src;
        return 1;
    }

    LONG InterL = (Src->left > Occ->left)     ? Src->left   : Occ->left;
    LONG InterT = (Src->top > Occ->top)       ? Src->top    : Occ->top;
    LONG InterR = (Src->right < Occ->right)   ? Src->right  : Occ->right;
    LONG InterB = (Src->bottom < Occ->bottom) ? Src->bottom : Occ->bottom;

    ULONG Count = 0;

    /* 1. Strip atas */
    if (InterT > Src->top)
    {
        Parts[Count].left   = Src->left;
        Parts[Count].top    = Src->top;
        Parts[Count].right  = Src->right;
        Parts[Count].bottom = InterT;
        Count++;
    }

    /* 2. Strip bawah */
    if (Src->bottom > InterB)
    {
        Parts[Count].left   = Src->left;
        Parts[Count].top    = InterB;
        Parts[Count].right  = Src->right;
        Parts[Count].bottom = Src->bottom;
        Count++;
    }

    /* 3. Strip tengah kiri */
    if (InterL > Src->left)
    {
        Parts[Count].left   = Src->left;
        Parts[Count].top    = InterT;
        Parts[Count].right  = InterL;
        Parts[Count].bottom = InterB;
        Count++;
    }

    /* 4. Strip tengah kanan */
    if (Src->right > InterR)
    {
        Parts[Count].left   = InterR;
        Parts[Count].top    = InterT;
        Parts[Count].right  = Src->right;
        Parts[Count].bottom = InterB;
        Count++;
    }

    return Count;
}

static FORCEINLINE BOOLEAN
WndHasFrame(
    IN PWND_OBJECT Wnd
)
{
    if (!Wnd || (Wnd->Style & WS_CHILD))
    {
        return FALSE;
    }

    /* Window WS_POPUP tanpa WS_CAPTION digambar borderless (misal fullscreen background) */
    if ((Wnd->Style & WS_POPUP) && ((Wnd->Style & WS_CAPTION) != WS_CAPTION))
    {
        return FALSE;
    }

    return TRUE;
}

static VOID
WndRefreshShadowsOverRect(
    IN PWND_OBJECT PaintedWnd,
    IN CONST RECT *DirtyBounds
)
{
    KIRQL OldIrql;
    PLIST_ENTRY Entry;
    PWND_OBJECT Targets[16];
    ULONG TargetCount = 0;

    if (!PaintedWnd || !DirtyBounds)
    {
        return;
    }

    KsAcquireSpinLock(&gWindowListLock, &OldIrql);

    PWND_OBJECT TopSelf = PaintedWnd;
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
        for (Entry = TopSelf->GlobalListEntry.Flink;
             Entry != &gWindowListHead && TargetCount < 16;
             Entry = Entry->Flink)
        {
            PWND_OBJECT Cand = CONTAINING_RECORD(Entry, WND_OBJECT, GlobalListEntry);
            if (Cand->Magic != WND_OBJECT_MAGIC || !Cand->Used || !WndHasFrame(Cand))
            {
                continue;
            }
            if (Cand->Desktop != TopSelf->Desktop || !(Cand->Style & WS_VISIBLE))
            {
                continue;
            }

            LONG Sx = Cand->X + WND_SHADOW_OFFSET_X;
            LONG Sy = Cand->Y + (LONG)Cand->Height;
            LONG Sr = Sx + (LONG)Cand->Width;
            LONG Sb = Sy + WND_SHADOW_HEIGHT;

            if (Sx < DirtyBounds->right && Sr > DirtyBounds->left &&
                Sy < DirtyBounds->bottom && Sb > DirtyBounds->top)
            {
                WndReference(Cand);
                Targets[TargetCount++] = Cand;
            }
        }
    }

    KsReleaseSpinLock(&gWindowListLock, OldIrql);

    for (ULONG i = 0; i < TargetCount; i++)
    {
        PWND_OBJECT Cand = Targets[i];
        if (Cand->HasShadowDrawn && Cand->ShadowBackup != NULL)
        {
            LONG Cl = (Cand->LastShadowX > DirtyBounds->left) ? Cand->LastShadowX : DirtyBounds->left;
            LONG Ct = (Cand->LastShadowY > DirtyBounds->top)  ? Cand->LastShadowY : DirtyBounds->top;
            LONG Cr = (Cand->LastShadowX + (LONG)Cand->LastShadowW < DirtyBounds->right)
                      ? (Cand->LastShadowX + (LONG)Cand->LastShadowW) : DirtyBounds->right;
            LONG Cb = (Cand->LastShadowY + (LONG)Cand->LastShadowH < DirtyBounds->bottom)
                      ? (Cand->LastShadowY + (LONG)Cand->LastShadowH) : DirtyBounds->bottom;

            for (LONG Y = Ct; Y < Cb; Y++)
            {
                LONG R = Y - Cand->LastShadowY;
                if (R < 0 || R >= (LONG)Cand->LastShadowH) continue;
                for (LONG X = Cl; X < Cr; X++)
                {
                    LONG C = X - Cand->LastShadowX;
                    if (C < 0 || C >= (LONG)Cand->LastShadowW) continue;
                    Cand->ShadowBackup[(SIZE_T)R * Cand->LastShadowW + (SIZE_T)C] = 0;
                }
            }
        }

        WndDrawWindowBottomShadow(Cand, TRUE);
        WndDereference(Cand);
    }
}

static VOID
WndInvalidateOverlappingAbove(
    IN PWND_OBJECT Wnd
)
{
    KIRQL OldIrql;
    PLIST_ENTRY Entry;
    BOOLEAN Notify = FALSE;

    if (!Wnd)
    {
        return;
    }

    KsAcquireSpinLock(&gWindowListLock, &OldIrql);

    /* Cari top-level ancestor dari Wnd */
    PWND_OBJECT TopSelf = Wnd;
    ULONG Depth = 0;
    while (TopSelf && (TopSelf->Style & WS_CHILD) && TopSelf->hWndParent != NULL && Depth < 64)
    {
        PWND_OBJECT Parent = WndLookupHandleLocked(TopSelf->hWndParent);
        if (!Parent) break;
        TopSelf = Parent;
        Depth++;
    }

    if (!TopSelf || TopSelf->Magic != WND_OBJECT_MAGIC || !TopSelf->Used)
    {
        KsReleaseSpinLock(&gWindowListLock, OldIrql);
        return;
    }

    LONG SelfL = TopSelf->X;
    LONG SelfT = TopSelf->Y;
    LONG SelfR = TopSelf->X + (LONG)TopSelf->Width;
    LONG SelfB = TopSelf->Y + (LONG)TopSelf->Height;

    /* Telusuri semua window yang berada DI ATAS TopSelf dalam Z-order (arah Flink) */
    for (Entry = TopSelf->GlobalListEntry.Flink; Entry != &gWindowListHead; Entry = Entry->Flink)
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

        LONG CandL = Cand->X;
        LONG CandT = Cand->Y;
        LONG CandR = Cand->X + (LONG)Cand->Width;
        LONG CandB = Cand->Y + (LONG)Cand->Height;

        if (SelfL < CandR && SelfR > CandL && SelfT < CandB && SelfB > CandT)
        {
            Cand->NeedsPaint = TRUE;
            Notify = TRUE;

            /* Tandai juga child yang terlihat milik Cand */
            PLIST_ENTRY CEntry;
            for (CEntry = Cand->GlobalListEntry.Flink; CEntry != &gWindowListHead; CEntry = CEntry->Flink)
            {
                PWND_OBJECT Child = CONTAINING_RECORD(CEntry, WND_OBJECT, GlobalListEntry);
                if (Child->Magic == WND_OBJECT_MAGIC && Child->Used &&
                    (Child->Style & WS_CHILD) && WndIsEffectivelyVisibleLocked(Child))
                {
                    /* Pastikan child ini memang milik Cand */
                    PWND_OBJECT Anc = Child;
                    ULONG D = 0;
                    while (Anc && (Anc->Style & WS_CHILD) && Anc->hWndParent != NULL && D < 64)
                    {
                        if (Anc->hWndParent == Cand->hWnd)
                        {
                            Child->NeedsPaint = TRUE;
                            break;
                        }
                        Anc = WndLookupHandleLocked(Anc->hWndParent);
                        D++;
                    }
                }
            }
        }
    }

    KsReleaseSpinLock(&gWindowListLock, OldIrql);

    if (Notify)
    {
        MsgNotifyPaintAll();
    }
}

VOID
WndFillClippedFrameRect(
    IN PVEAGDI_SURFACE Target,
    IN CONST RECT     *VisRects,
    IN ULONG           VisCount,
    IN LONG            X,
    IN LONG            Y,
    IN ULONG           Width,
    IN ULONG           Height,
    IN ULONG           ColorARGB
)
{
    if (!Target || Width == 0 || Height == 0)
    {
        return;
    }

    LONG R = X + (LONG)Width;
    LONG B = Y + (LONG)Height;

    for (ULONG i = 0; i < VisCount; i++)
    {
        LONG Cl = (X > VisRects[i].left)   ? X : VisRects[i].left;
        LONG Ct = (Y > VisRects[i].top)    ? Y : VisRects[i].top;
        LONG Cr = (R < VisRects[i].right)  ? R : VisRects[i].right;
        LONG Cb = (B < VisRects[i].bottom) ? B : VisRects[i].bottom;

        if (Cl < Cr && Ct < Cb)
        {
            EngFillRect32(Target, Cl, Ct, (ULONG)(Cr - Cl), (ULONG)(Cb - Ct), ColorARGB);
        }
    }
}

ULONG
VEAPI
WndComputeVisibleRects(
    IN  PWND_OBJECT Wnd,
    IN  CONST RECT *ClipBounds,
    OUT PRECT       OutRects,
    IN  ULONG       MaxRects
)
{
    PVEAGDI_SURFACE Primary = EngGetTargetSurface();
    KIRQL OldIrql;
    PLIST_ENTRY Entry;

    if (!Wnd || !ClipBounds || !OutRects || MaxRects == 0 || !Primary)
    {
        return 0;
    }

    RECT Base = *ClipBounds;
    if (Base.left < 0) Base.left = 0;
    if (Base.top < 0)  Base.top  = 0;
    if (Base.right > (LONG)Primary->Width)   Base.right  = (LONG)Primary->Width;
    if (Base.bottom > (LONG)Primary->Height) Base.bottom = (LONG)Primary->Height;

    if (Base.left >= Base.right || Base.top >= Base.bottom)
    {
        return 0;
    }

    OutRects[0] = Base;
    ULONG Count = 1;

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

    if (!TopSelf || TopSelf->Magic != WND_OBJECT_MAGIC || !TopSelf->Used)
    {
        KsReleaseSpinLock(&gWindowListLock, OldIrql);
        return 0;
    }

    /* Telusuri semua top-level window DI ATAS TopSelf dalam Z-order (Flink -> Blink) */
    for (Entry = TopSelf->GlobalListEntry.Flink;
         Entry != &gWindowListHead && Count > 0;
         Entry = Entry->Flink)
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

        RECT Occ;
        Occ.left   = Cand->X;
        Occ.top    = Cand->Y;
        Occ.right  = Cand->X + (LONG)Cand->Width;
        Occ.bottom = Cand->Y + (LONG)Cand->Height;

        RECT NextRects[MAX_VISIBLE_CLIP_RECTS];
        ULONG NextCount = 0;

        for (ULONG i = 0; i < Count; i++)
        {
            RECT Parts[4];
            ULONG PartCount = WndSubtractRect(&OutRects[i], &Occ, Parts);

            if (NextCount + PartCount > MaxRects)
            {
                if (NextCount < MaxRects)
                {
                    NextRects[NextCount++] = OutRects[i];
                }
                continue;
            }

            for (ULONG p = 0; p < PartCount; p++)
            {
                NextRects[NextCount++] = Parts[p];
            }
        }

        Count = NextCount;
        if (Count > 0)
        {
            RtlCopyMemory(OutRects, NextRects, Count * sizeof(RECT));
        }
    }

    KsReleaseSpinLock(&gWindowListLock, OldIrql);
    return Count;
}

VOID
WndGetClientOriginOnScreen(
    IN PWND_OBJECT Wnd,
    OUT PLONG ScreenX,
    OUT PLONG ScreenY
)
{
    LONG X = 0;
    LONG Y = 0;
    PWND_OBJECT Curr = Wnd;

    while (Curr)
    {
        X += Curr->X;
        Y += Curr->Y;

        if (!(Curr->Style & WS_CHILD))
        {
            /* Top-level window punya border & titlebar */
            X += WND_BORDER_WIDTH;
            Y += WND_TITLEBAR_HEIGHT;
            break;
        }

        Curr = WndLookupHandleLocked(Curr->hWndParent);
    }

    *ScreenX = X;
    *ScreenY = Y;
}

VOID
WndGetClippedClientRect(
    IN  PWND_OBJECT Wnd,
    OUT PLONG OriginX,
    OUT PLONG OriginY,
    OUT PLONG ClipLeft,
    OUT PLONG ClipTop,
    OUT PLONG ClipRight,
    OUT PLONG ClipBottom
)
{
    KIRQL OldIrql;
    LONG Ox = 0;
    LONG Oy = 0;

    KsAcquireSpinLock(&gWindowListLock, &OldIrql);

    WndGetClientOriginOnScreen(Wnd, &Ox, &Oy);

    LONG Cl = Ox;
    LONG Ct = Oy;
    LONG Cr = Ox + (LONG)Wnd->Width;
    LONG Cb = Oy + (LONG)Wnd->Height;

    /* Jika WS_CHILD, potong area gambar terhadap client rect semua parent-nya */
    if (Wnd->Style & WS_CHILD)
    {
        PWND_OBJECT Parent = WndLookupHandleLocked(Wnd->hWndParent);
        ULONG Depth = 0;
        while (Parent && Depth < 64)
        {
            LONG Px = 0;
            LONG Py = 0;
            WndGetClientOriginOnScreen(Parent, &Px, &Py);

            LONG PLeft   = Px;
            LONG PTop    = Py;
            LONG PRight  = Px + (LONG)Parent->Width;
            LONG PBottom = Py + (LONG)Parent->Height;

            if (!(Parent->Style & WS_CHILD) && WndHasFrame(Parent))
            {
                PRight  = Parent->X + (LONG)Parent->Width - WND_BORDER_WIDTH;
                PBottom = Parent->Y + (LONG)Parent->Height - WND_BORDER_WIDTH;
            }

            if (Cl < PLeft)   Cl = PLeft;
            if (Ct < PTop)    Ct = PTop;
            if (Cr > PRight)  Cr = PRight;
            if (Cb > PBottom) Cb = PBottom;

            if (!(Parent->Style & WS_CHILD))
            {
                break;
            }
            Parent = WndLookupHandleLocked(Parent->hWndParent);
            Depth++;
        }
    }

    KsReleaseSpinLock(&gWindowListLock, OldIrql);

    *OriginX    = Ox;
    *OriginY    = Oy;
    *ClipLeft   = Cl;
    *ClipTop    = Ct;
    *ClipRight  = Cr;
    *ClipBottom = Cb;
}

HWND
VEAPI
WndFindPendingPaintWindow(
    IN PETHREAD Thread
)
{
    PLIST_ENTRY Entry;
    KIRQL OldIrql;
    HWND FoundHwnd = NULL;

    WndEnsureListsInitialized();

    KsAcquireSpinLock(&gWindowListLock, &OldIrql);

    /* Iterasi dari bawah ke atas (Flink -> Blink) sesuai Z-order:
       Top-level window selalu berada sebelum child-nya, dan window depan
       selalu berada setelah window belakang beserta seluruh child-nya. */
    for (Entry = gWindowListHead.Flink; Entry != &gWindowListHead; Entry = Entry->Flink)
    {
        PWND_OBJECT Wnd = CONTAINING_RECORD(Entry, WND_OBJECT, GlobalListEntry);
        if (Wnd->Magic == WND_OBJECT_MAGIC &&
            Wnd->Used &&
            Wnd->NeedsPaint &&
            (GpActiveDesktop == NULL || Wnd->Desktop == GpActiveDesktop) &&
            Wnd->OwnerThread == Thread &&
            WndIsEffectivelyVisibleLocked(Wnd))
        {
            FoundHwnd = Wnd->hWnd;
            break;
        }
    }

    KsReleaseSpinLock(&gWindowListLock, OldIrql);

    return FoundHwnd;
}

VOID
WndDrawTitleBarText(
    IN PWND_OBJECT Wnd,
    IN LONG TitleX,
    IN LONG TitleY,
    IN ULONG TitleW,
    IN ULONG TitleH
)
{
    if (!Wnd || Wnd->Title[0] == '\0' || TitleW <= 12 || TitleH <= 4)
    {
        return;
    }

    /* Buat DC sementara yang ter-clip ketat di dalam area teks titlebar */
    HDC TitleDc = GdiCreateWindowDC(
        Wnd->hWnd,
        TitleX + 6,
        TitleY,
        TitleW - 12,
        TitleH
    );
    if (!TitleDc)
    {
        return;
    }

    ULONG TextW = 0;
    ULONG TextH = 16;
    GdipGetTextExtentFontInternal(TitleDc, Wnd->Title, &TextW, &TextH, FALSE);

    INT OffsetY = ((INT)TitleH - (INT)TextH) / 2;
    if (OffsetY < 0)
    {
        OffsetY = 0;
    }

    GdipDrawTextInternal(TitleDc, 0, OffsetY, Wnd->Title, 0xFFFFFFFF, FALSE);
    GdiReleaseWindowDC(TitleDc);
}

HDC
VEAPI
VeaUserBeginPaint(
    IN HWND hWnd,
    OUT PPAINTSTRUCT lpPaint
)
{
    PVEAGDI_SURFACE Target = EngGetTargetSurface();
    PWND_OBJECT Wnd = WndLookupHandle(hWnd);
    PAINTSTRUCT LocalPs;
    BOOLEAN IsUser = (KeGetPreviousMode() == UserMode);
    BOOLEAN IsChild;
    LONG ClientX;
    LONG ClientY;
    LONG ClipL, ClipT, ClipR, ClipB;
    ULONG ClientW;
    ULONG ClientH;

    if (!Wnd || !lpPaint)
    {
        if (Wnd) WndDereference(Wnd);
        return NULL;
    }

    if (IsUser)
    {
        if ((ULONG_PTR)lpPaint >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS ||
            ((ULONG_PTR)lpPaint + sizeof(PAINTSTRUCT)) >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS)
        {
            WndDereference(Wnd);
            return NULL;
        }
    }

    Wnd->NeedsPaint = FALSE;

    /* Tolak menggambar ke layar jika desktop window ini sedang tidak aktif */
    if (GpActiveDesktop != NULL && Wnd->Desktop != GpActiveDesktop)
    {
        WndDereference(Wnd);
        return NULL;
    }

    EngHidePointer();

    KIRQL GeomIrql;
    KsAcquireSpinLock(&gWindowListLock, &GeomIrql);
    LONG  SnapX = Wnd->X;
    LONG  SnapY = Wnd->Y;
    ULONG SnapW = Wnd->Width;
    ULONG SnapH = Wnd->Height;
    IsChild     = ((Wnd->Style & WS_CHILD) != 0);
    KsReleaseSpinLock(&gWindowListLock, GeomIrql);

    if (!IsChild)
    {
        RECT FrameBounds = {
            SnapX,
            SnapY,
            SnapX + (LONG)SnapW,
            SnapY + (LONG)SnapH
        };
        RECT FrameVis[MAX_VISIBLE_CLIP_RECTS];
        ULONG FrameVisCount = WndComputeVisibleRects(
            Wnd,
            &FrameBounds,
            FrameVis,
            MAX_VISIBLE_CLIP_RECTS
        );

        /* Assign ke variabel luar (JANGAN tulis LONG/ULONG lagi di depan!) */
        ClientX = WndHasFrame(Wnd) ? (SnapX + WND_BORDER_WIDTH) : SnapX;
        ClientY = WndHasFrame(Wnd) ? (SnapY + WND_TITLEBAR_HEIGHT) : SnapY;
        ClientW = WndHasFrame(Wnd)
                  ? ((SnapW > 2 * WND_BORDER_WIDTH) ? (SnapW - 2 * WND_BORDER_WIDTH) : 0)
                  : SnapW;
        ClientH = WndHasFrame(Wnd)
                  ? ((SnapH > WND_TITLEBAR_HEIGHT + WND_BORDER_WIDTH)
                     ? (SnapH - WND_TITLEBAR_HEIGHT - WND_BORDER_WIDTH) : 0)
                  : SnapH;

        if (WndHasFrame(Wnd))
        {
            WndDrawLunaWindowFrame(Target, Wnd, FrameVis, FrameVisCount, SnapX, SnapY, SnapW, SnapH, TRUE);
        }

        if (ClientW > 0 && ClientH > 0)
        {
            ULONG BgColor = 0xFFFFFFFF;
            if (Wnd->hbrBackground != NULL)
            {
                GdipResolveBrushColor(Wnd->hbrBackground, &BgColor);
            }

            WndFillClippedFrameRect(
                Target,
                FrameVis,
                FrameVisCount,
                ClientX,
                ClientY,
                ClientW,
                ClientH,
                BgColor
            );
        }

        ClipL = ClientX;
        ClipT = ClientY;
        ClipR = ClientX + (LONG)ClientW;
        ClipB = ClientY + (LONG)ClientH;

        WndInvalidateChildWindows(hWnd);
        MsgNotifyPaintAll();
    }
    else
    {
        /* WS_CHILD: hitung posisi layar dan clip ketat terhadap parent */
        WndGetClippedClientRect(Wnd, &ClientX, &ClientY, &ClipL, &ClipT, &ClipR, &ClipB);
        ClientW = SnapW;
        ClientH = SnapH;

        WndInvalidateChildWindows(hWnd);
    }

    HDC hdc = GdiCreateWindowDC(hWnd, ClientX, ClientY, ClientW, ClientH);
    if (!hdc)
    {
        EngShowPointer();
        WndDereference(Wnd);
        return NULL;
    }

    /* Terapkan hasil clipping parent ke DC child */
    PGDI_DC_OBJECT DcObj = VeaGdiGetDc(hdc);
    if (DcObj)
    {
        DcObj->ClipLeft   = ClipL;
        DcObj->ClipTop    = ClipT;
        DcObj->ClipRight  = ClipR;
        DcObj->ClipBottom = ClipB;
    }

    RtlZeroMemory(&LocalPs, sizeof(PAINTSTRUCT));
    LocalPs.hdc            = hdc;
    LocalPs.fErase         = TRUE;
    LocalPs.rcPaint.left   = 0;
    LocalPs.rcPaint.top    = 0;
    LocalPs.rcPaint.right  = (LONG)ClientW;
    LocalPs.rcPaint.bottom = (LONG)ClientH;

    if (IsUser)
    {
        _VEA_TRY
        {
            RtlCopyMemory(lpPaint, &LocalPs, sizeof(PAINTSTRUCT));
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            GdiReleaseWindowDC(hdc);
            EngShowPointer();
            WndDereference(Wnd);
            return NULL;
        }
        _VEA_END
    }
    else
    {
        RtlCopyMemory(lpPaint, &LocalPs, sizeof(PAINTSTRUCT));
    }

    WndDereference(Wnd);
    return hdc;
}

BOOLEAN
VEAPI
VeaUserEndPaint(
    IN HWND hWnd,
    IN CONST PAINTSTRUCT *lpPaint
)
{
    PAINTSTRUCT LocalPs;
    PWND_OBJECT Wnd;
    BOOLEAN IsUser = (KeGetPreviousMode() == UserMode);
    PGDI_DC_OBJECT Dc;
    LONG PaintOriginX = 0;
    LONG PaintOriginY = 0;
    LONG PaintClipLeft = 0;
    LONG PaintClipTop = 0;
    LONG PaintClipRight = 0;
    LONG PaintClipBottom = 0;
    BOOLEAN HasPaintGeometry = FALSE;

    if (!lpPaint)
    {
        return FALSE;
    }

    if (IsUser)
    {
        if ((ULONG_PTR)lpPaint >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS ||
            ((ULONG_PTR)lpPaint + sizeof(PAINTSTRUCT)) >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS)
        {
            return FALSE;
        }

        _VEA_TRY
        {
            RtlCopyMemory(&LocalPs, (PVOID)lpPaint, sizeof(PAINTSTRUCT));
        }
        _VEA_EXCEPT(1)
        {
            return FALSE;
        }
        _VEA_END
    }
    else
    {
        RtlCopyMemory(&LocalPs, (PVOID)lpPaint, sizeof(PAINTSTRUCT));
    }

    Dc = VeaGdiGetDc(LocalPs.hdc);
    if (Dc && Dc->hWnd == hWnd)
    {
        PaintOriginX = Dc->OriginX;
        PaintOriginY = Dc->OriginY;
        PaintClipLeft = Dc->ClipLeft;
        PaintClipTop = Dc->ClipTop;
        PaintClipRight = Dc->ClipRight;
        PaintClipBottom = Dc->ClipBottom;
        HasPaintGeometry = TRUE;
    }

    GdiReleaseWindowDC(LocalPs.hdc);
    EngShowPointer();

    Wnd = WndLookupHandle(hWnd);
    if (Wnd)
    {
        LONG ClientX;
        LONG ClientY;
        if (Wnd->Style & WS_CHILD)
        {
            WndGetClientOriginOnScreen(Wnd, &ClientX, &ClientY);
        }
        else if (WndHasFrame(Wnd))
        {
            ClientX = Wnd->X + WND_BORDER_WIDTH;
            ClientY = Wnd->Y + WND_TITLEBAR_HEIGHT;
        }
        else
        {
            ClientX = Wnd->X;
            ClientY = Wnd->Y;
        }

        if (HasPaintGeometry &&
            (PaintOriginX != ClientX || PaintOriginY != ClientY))
        {
            Wnd->NeedsPaint = TRUE;
            WndInvalidateChildWindows(hWnd);
            MsgNotifyPaint(Wnd->OwnerThread);
        }
        else
        {
            LONG ScrX = Wnd->X;
            LONG ScrY = Wnd->Y;
            if (Wnd->Style & WS_CHILD)
            {
                ScrX = ClientX;
                ScrY = ClientY;
            }

            RECT PresentBounds = {
                ScrX,
                ScrY,
                ScrX + (LONG)Wnd->Width + (WndHasFrame(Wnd) ? WND_SHADOW_OFFSET_X : 0),
                ScrY + (LONG)Wnd->Height + (WndHasFrame(Wnd) ? WND_SHADOW_HEIGHT : 0)
            };
            RECT PresentVis[MAX_VISIBLE_CLIP_RECTS];
            ULONG PresentCount = WndComputeVisibleRects(
                Wnd,
                &PresentBounds,
                PresentVis,
                MAX_VISIBLE_CLIP_RECTS
            );

            for (ULONG i = 0; i < PresentCount; i++)
            {
                EngPresentRect(
                    PresentVis[i].left,
                    PresentVis[i].top,
                    (ULONG)(PresentVis[i].right - PresentVis[i].left),
                    (ULONG)(PresentVis[i].bottom - PresentVis[i].top)
                );
            }

            WndRefreshShadowsOverRect(Wnd, &PresentBounds);
        }
        WndDereference(Wnd);
    }
    return TRUE;
}

BOOLEAN
VEAPI
VeaUserInvalidateRect(
    IN HWND hWnd,
    IN CONST PRECT lpRect OPTIONAL,
    IN BOOLEAN bErase
)
{
    UNREFERENCED_PARAMETER(lpRect);
    UNREFERENCED_PARAMETER(bErase);

    if (hWnd == NULL)
    {
        UserInvalidateAllWindows();
        return TRUE;
    }

    PWND_OBJECT Wnd = WndLookupHandle(hWnd);
    if (!Wnd)
    {
        return FALSE;
    }

    KIRQL OldIrql;
    BOOLEAN ShouldPaint = FALSE;

    KsAcquireSpinLock(&gWindowListLock, &OldIrql);
    if (WndIsEffectivelyVisibleLocked(Wnd))
    {
        Wnd->NeedsPaint = TRUE;
        ShouldPaint = TRUE;
    }
    KsReleaseSpinLock(&gWindowListLock, OldIrql);

    if (ShouldPaint)
    {
        MsgNotifyPaint(Wnd->OwnerThread);
    }

    WndDereference(Wnd);
    return TRUE;
}

VOID
VEAPI
WndRestoreWindowBottomShadow(
    IN PWND_OBJECT Wnd,
    IN BOOLEAN     PresentToScreen
)
{
    PVEAGDI_SURFACE Primary = EngGetTargetSurface();

    if (!Wnd || !Wnd->HasShadowDrawn || !Wnd->ShadowBackup || !Primary || !Primary->Scan0)
    {
        return;
    }

    RECT OldBounds = {
        Wnd->LastShadowX,
        Wnd->LastShadowY,
        Wnd->LastShadowX + (LONG)Wnd->LastShadowW,
        Wnd->LastShadowY + (LONG)Wnd->LastShadowH
    };
    RECT VisRects[MAX_VISIBLE_CLIP_RECTS];
    ULONG VisCount = WndComputeVisibleRects(Wnd, &OldBounds, VisRects, MAX_VISIBLE_CLIP_RECTS);

    for (ULONG v = 0; v < VisCount; v++)
    {
        for (LONG Y = VisRects[v].top; Y < VisRects[v].bottom; Y++)
        {
            LONG LocalRow = Y - Wnd->LastShadowY;
            if (LocalRow < 0 || LocalRow >= (LONG)Wnd->LastShadowH) continue;

            PULONG DstRow = (PULONG)((PUCHAR)Primary->Scan0 + (SIZE_T)Y * Primary->Pitch);
            PULONG BakRow = Wnd->ShadowBackup + (SIZE_T)LocalRow * Wnd->LastShadowW;

            for (LONG X = VisRects[v].left; X < VisRects[v].right; X++)
            {
                LONG LocalCol = X - Wnd->LastShadowX;
                if (LocalCol < 0 || LocalCol >= (LONG)Wnd->LastShadowW) continue;

                ULONG Saved = BakRow[LocalCol];
                if (Saved & 0x80000000)
                {
                    DstRow[X] = Saved;
                    BakRow[LocalCol] = 0;
                }
            }
        }

        if (PresentToScreen)
        {
            EngPresentRect(
                VisRects[v].left,
                VisRects[v].top,
                (ULONG)(VisRects[v].right - VisRects[v].left),
                (ULONG)(VisRects[v].bottom - VisRects[v].top)
            );
        }
    }

    Wnd->HasShadowDrawn = FALSE;
}

VOID
VEAPI
WndDrawWindowBottomShadow(
    IN PWND_OBJECT Wnd,
    IN BOOLEAN     PresentToScreen
)
{
    static const UCHAR RowAlphas[WND_SHADOW_HEIGHT] = { 72, 52, 34, 20, 10, 4 };
    PVEAGDI_SURFACE Primary = EngGetTargetSurface();

    if (!Wnd || !WndHasFrame(Wnd) || !(Wnd->Style & WS_VISIBLE) ||
        (Wnd->Style & WS_MAXIMIZE) ||
        (GpActiveDesktop != NULL && Wnd->Desktop != GpActiveDesktop) ||
        !Primary || !Primary->Scan0)
    {
        return;
    }

    LONG  ShadowX = Wnd->X + WND_SHADOW_OFFSET_X;
    LONG  ShadowY = Wnd->Y + (LONG)Wnd->Height;
    ULONG ShadowW = Wnd->Width;
    ULONG ShadowH = WND_SHADOW_HEIGHT;

    if (ShadowW == 0)
    {
        return;
    }

    if (Wnd->HasShadowDrawn &&
        (Wnd->LastShadowX != ShadowX || Wnd->LastShadowY != ShadowY ||
         Wnd->LastShadowW != ShadowW || Wnd->LastShadowH != ShadowH))
    {
        WndRestoreWindowBottomShadow(Wnd, PresentToScreen);
    }

    ULONG NeededPixels = ShadowW * ShadowH;
    if (Wnd->ShadowBackup == NULL || Wnd->ShadowBackupCapacity < NeededPixels)
    {
        if (Wnd->ShadowBackup != NULL)
        {
            GDI_POOL_FREE(Wnd->ShadowBackup);
            Wnd->ShadowBackup = NULL;
            Wnd->ShadowBackupCapacity = 0;
        }

        Wnd->ShadowBackup = (PULONG)GDI_POOL_ALLOC((SIZE_T)NeededPixels * sizeof(ULONG));
        if (!Wnd->ShadowBackup)
        {
            return;
        }
        RtlZeroMemory(Wnd->ShadowBackup, (SIZE_T)NeededPixels * sizeof(ULONG));
        Wnd->ShadowBackupCapacity = NeededPixels;
        Wnd->HasShadowDrawn = FALSE;
    }
    else if (!Wnd->HasShadowDrawn)
    {
        RtlZeroMemory(Wnd->ShadowBackup, (SIZE_T)NeededPixels * sizeof(ULONG));
    }

    RECT ShadowBounds = {
        ShadowX,
        ShadowY,
        ShadowX + (LONG)ShadowW,
        ShadowY + (LONG)ShadowH
    };
    RECT VisRects[MAX_VISIBLE_CLIP_RECTS];
    ULONG VisCount = WndComputeVisibleRects(Wnd, &ShadowBounds, VisRects, MAX_VISIBLE_CLIP_RECTS);

    for (ULONG v = 0; v < VisCount; v++)
    {
        for (LONG Y = VisRects[v].top; Y < VisRects[v].bottom; Y++)
        {
            LONG LocalRow = Y - ShadowY;
            if (LocalRow < 0 || LocalRow >= (LONG)ShadowH) continue;

            UCHAR BaseAlpha = RowAlphas[LocalRow];
            PULONG DstRow = (PULONG)((PUCHAR)Primary->Scan0 + (SIZE_T)Y * Primary->Pitch);
            PULONG BakRow = Wnd->ShadowBackup + (SIZE_T)LocalRow * ShadowW;

            for (LONG X = VisRects[v].left; X < VisRects[v].right; X++)
            {
                LONG LocalCol = X - ShadowX;
                if (LocalCol < 0 || LocalCol >= (LONG)ShadowW) continue;

                ULONG CleanPixel = BakRow[LocalCol];
                if ((CleanPixel & 0x80000000) == 0)
                {
                    CleanPixel = DstRow[X] | 0xFF000000;
                    BakRow[LocalCol] = CleanPixel;
                }

                /* Feather halus 2px di ujung kiri & kanan shadow */
                ULONG Alpha = BaseAlpha;
                if (LocalCol == 0 || LocalCol == (LONG)ShadowW - 1)
                {
                    Alpha = Alpha / 3;
                }
                else if (LocalCol == 1 || LocalCol == (LONG)ShadowW - 2)
                {
                    Alpha = (Alpha * 2) / 3;
                }

                DstRow[X] = WndBlendShadowPixel(CleanPixel, (UCHAR)Alpha);
            }
        }

        if (PresentToScreen)
        {
            EngPresentRect(
                VisRects[v].left,
                VisRects[v].top,
                (ULONG)(VisRects[v].right - VisRects[v].left),
                (ULONG)(VisRects[v].bottom - VisRects[v].top)
            );
        }
    }

    Wnd->LastShadowX    = ShadowX;
    Wnd->LastShadowY    = ShadowY;
    Wnd->LastShadowW    = ShadowW;
    Wnd->LastShadowH    = ShadowH;
    Wnd->HasShadowDrawn = TRUE;
}

/* EOF */