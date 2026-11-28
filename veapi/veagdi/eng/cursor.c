/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : cursor.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Cursor handling
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veapi.h>

/* Revision History ------------------------------------------------------
- DATE       : 22-11-2026
- NAME       : RasyaFR-Engkol
- REVISION   : Creating file cursor.c
- --------------------------------------------------------------------- */

GDI_CURSOR_STATE gCursor = { .ShowCount = 0 };
KSPIN_LOCK       gCursorLock;
BOOLEAN          gCursorLockInit = FALSE;
ULONG            gCursorStagingBuf[MAX_CURSOR_SIZE * MAX_CURSOR_SIZE];

static VOID
CurEnsureLock(VOID)
{
    if (!gCursorLockInit)
    {
        KsInitializeSpinlock(&gCursorLock);
        gCursorLockInit = TRUE;
    }
}

static VOID
CurPresentClippedRect(
    IN LONG L,
    IN LONG T,
    IN LONG R,
    IN LONG B
)
{
    LONG ScrW = (LONG)GpPrimaryDisplay.PrimarySurface.Width;
    LONG ScrH = (LONG)GpPrimaryDisplay.PrimarySurface.Height;

    if (ScrW <= 0 || ScrH <= 0)
    {
        return;
    }

    if (L < 0)    L = 0;
    if (T < 0)    T = 0;
    if (R > ScrW) R = ScrW;
    if (B > ScrH) B = ScrH;

    if (L < R && T < B)
    {
        EngPresentRect(L, T, (ULONG)(R - L), (ULONG)(B - T));
    }
}

VOID
VEAPI
CurCompositeToPrimaryRect(
    IN LONG  ClipX,
    IN LONG  ClipY,
    IN ULONG ClipW,
    IN ULONG ClipH
)
{
    PVEAGDI_SURFACE Primary = &GpPrimaryDisplay.PrimarySurface;
    KIRQL OldIrql;

    if (!Primary->Scan0 || ClipW == 0 || ClipH == 0)
    {
        return;
    }

    CurEnsureLock();
    KsAcquireSpinLock(&gCursorLock, &OldIrql);

    if (!gCursor.HasShape || gCursor.ShowCount < 0)
    {
        KsReleaseSpinLock(&gCursorLock, OldIrql);
        return;
    }

    LONG CurL = GpPrimaryDisplay.CursorX - gCursor.HotX;
    LONG CurT = GpPrimaryDisplay.CursorY - gCursor.HotY;
    LONG CurR = CurL + (LONG)gCursor.Width;
    LONG CurB = CurT + (LONG)gCursor.Height;
    LONG InterL = (CurL > ClipX) ? CurL : ClipX;
    LONG InterT = (CurT > ClipY) ? CurT : ClipY;
    LONG InterR = (CurR < ClipX + (LONG)ClipW) ? CurR : (ClipX + (LONG)ClipW);
    LONG InterB = (CurB < ClipY + (LONG)ClipH) ? CurB : (ClipY + (LONG)ClipH);
    LONG ScrW = (LONG)Primary->Width;
    LONG ScrH = (LONG)Primary->Height;

    if (InterL < 0)    InterL = 0;
    if (InterT < 0)    InterT = 0;
    if (InterR > ScrW) InterR = ScrW;
    if (InterB > ScrH) InterB = ScrH;

    if (InterL < InterR && InterT < InterB)
    {
        for (LONG Py = InterT; Py < InterB; Py++)
        {
            ULONG Row = (ULONG)(Py - CurT);
            PULONG DstRow = (PULONG)((PUCHAR)Primary->Scan0 + (SIZE_T)Py * Primary->Pitch);

            for (LONG Px = InterL; Px < InterR; Px++)
            {
                ULONG Col      = (ULONG)(Px - CurL);
                ULONG SrcPixel = gCursor.Pixels[Row * gCursor.Width + Col];
                UCHAR Alpha    = (UCHAR)(SrcPixel >> 24);

                if (Alpha == 0) continue;

                if (Alpha == 255)
                {
                    DstRow[Px] = SrcPixel;
                }
                else
                {
                    ULONG DstPx = DstRow[Px];
                    ULONG InvA  = 255 - Alpha;
                    ULONG OutR  = (((SrcPixel >> 16) & 0xFF) * Alpha + ((DstPx >> 16) & 0xFF) * InvA + 127) / 255;
                    ULONG OutG  = (((SrcPixel >> 8)  & 0xFF) * Alpha + ((DstPx >> 8)  & 0xFF) * InvA + 127) / 255;
                    ULONG OutB  = ((SrcPixel         & 0xFF) * Alpha + (DstPx         & 0xFF) * InvA + 127) / 255;
                    DstRow[Px]  = 0xFF000000 | (OutR << 16) | (OutG << 8) | OutB;
                }
            }
        }
    }

    KsReleaseSpinLock(&gCursorLock, OldIrql);
}

static VOID
CurHideInternal(VOID)
{
    /* Kalau BackBuffer aktif, kursor tidak pernah digambar ke BackBuffer */
    if (GpPrimaryDisplay.BackBuffer != NULL)
    {
        gCursor.DrawnOnScreen = FALSE;
        return;
    }

    PVEAGDI_SURFACE Primary = &GpPrimaryDisplay.PrimarySurface;
    if (!gCursor.DrawnOnScreen || !Primary->Scan0)
    {
        return;
    }

    LONG ScrW   = (LONG)Primary->Width;
    LONG ScrH   = (LONG)Primary->Height;
    LONG StartX = gCursor.DrawnX - gCursor.HotX;
    LONG StartY = gCursor.DrawnY - gCursor.HotY;

    for (ULONG Row = 0; Row < gCursor.Height; Row++)
    {
        LONG Py = StartY + (LONG)Row;
        if (Py < 0 || Py >= ScrH) continue;

        PULONG DstRow = (PULONG)((PUCHAR)Primary->Scan0 + (Py * Primary->Pitch));

        for (ULONG Col = 0; Col < gCursor.Width; Col++)
        {
            LONG Px = StartX + (LONG)Col;
            if (Px < 0 || Px >= ScrW) continue;

            ULONG Idx = Row * gCursor.Width + Col;
            if ((gCursor.Pixels[Idx] >> 24) != 0)
            {
                DstRow[Px] = gCursor.SaveUnder[Idx];
            }
        }
    }

    gCursor.DrawnOnScreen = FALSE;
}

static VOID
CurShowInternal(VOID)
{
    /* Kalau BackBuffer aktif, kursor di-composite lewat EngPresentRect() */
    if (GpPrimaryDisplay.BackBuffer != NULL)
    {
        return;
    }

    PVEAGDI_SURFACE Primary = &GpPrimaryDisplay.PrimarySurface;
    if (gCursor.DrawnOnScreen || !gCursor.HasShape || gCursor.ShowCount < 0 || !Primary->Scan0)
    {
        return;
    }

    LONG ScrW   = (LONG)Primary->Width;
    LONG ScrH   = (LONG)Primary->Height;
    LONG StartX = GpPrimaryDisplay.CursorX - gCursor.HotX;
    LONG StartY = GpPrimaryDisplay.CursorY - gCursor.HotY;

    for (ULONG Row = 0; Row < gCursor.Height; Row++)
    {
        LONG Py = StartY + (LONG)Row;
        if (Py < 0 || Py >= ScrH) continue;

        PULONG DstRow = (PULONG)((PUCHAR)Primary->Scan0 + (Py * Primary->Pitch));

        for (ULONG Col = 0; Col < gCursor.Width; Col++)
        {
            LONG Px = StartX + (LONG)Col;
            if (Px < 0 || Px >= ScrW) continue;

            ULONG Idx      = Row * gCursor.Width + Col;
            ULONG SrcPixel = gCursor.Pixels[Idx];
            UCHAR Alpha    = (UCHAR)(SrcPixel >> 24);

            if (Alpha != 0)
            {
                gCursor.SaveUnder[Idx] = DstRow[Px];

                if (Alpha == 255)
                {
                    DstRow[Px] = SrcPixel;
                }
                else
                {
                    ULONG DstPx = DstRow[Px];
                    ULONG InvA  = 255 - Alpha;
                    ULONG OutR  = (((SrcPixel >> 16) & 0xFF) * Alpha + ((DstPx >> 16) & 0xFF) * InvA + 127) / 255;
                    ULONG OutG  = (((SrcPixel >> 8)  & 0xFF) * Alpha + ((DstPx >> 8)  & 0xFF) * InvA + 127) / 255;
                    ULONG OutB  = ((SrcPixel         & 0xFF) * Alpha + (DstPx         & 0xFF) * InvA + 127) / 255;
                    DstRow[Px]  = 0xFF000000 | (OutR << 16) | (OutG << 8) | OutB;
                }
            }
        }
    }

    gCursor.DrawnX        = GpPrimaryDisplay.CursorX;
    gCursor.DrawnY        = GpPrimaryDisplay.CursorY;
    gCursor.DrawnOnScreen = TRUE;
}

VOID
VEAPI
EngHidePointer(VOID)
{
    if (GpPrimaryDisplay.BackBuffer != NULL)
    {
        return;
    }

    KIRQL OldIrql;
    CurEnsureLock();
    KsAcquireSpinLock(&gCursorLock, &OldIrql);
    CurHideInternal();
    KsReleaseSpinLock(&gCursorLock, OldIrql);
}

VOID
VEAPI
EngShowPointer(VOID)
{
    if (GpPrimaryDisplay.BackBuffer != NULL)
    {
        return;
    }

    KIRQL OldIrql;
    CurEnsureLock();
    KsAcquireSpinLock(&gCursorLock, &OldIrql);
    CurShowInternal();
    KsReleaseSpinLock(&gCursorLock, OldIrql);
}

VOID
VEAPI
EngMovePointer(
    IN LONG NewX,
    IN LONG NewY
)
{
    KIRQL OldIrql;
    LONG OldLeft, OldTop, NewLeft, NewTop;
    ULONG CurW, CurH;
    LONG ScrW = (LONG)GpPrimaryDisplay.PrimarySurface.Width;
    LONG ScrH = (LONG)GpPrimaryDisplay.PrimarySurface.Height;

    if (NewX < 0) NewX = 0;
    if (NewY < 0) NewY = 0;
    if (ScrW > 0 && NewX >= ScrW) NewX = ScrW - 1;
    if (ScrH > 0 && NewY >= ScrH) NewY = ScrH - 1;

    gCursorX = NewX;
    gCursorY = NewY;

    CurEnsureLock();
    KsAcquireSpinLock(&gCursorLock, &OldIrql);

    if (GpPrimaryDisplay.BackBuffer == NULL)
    {
        CurHideInternal();
        GpPrimaryDisplay.CursorX = NewX;
        GpPrimaryDisplay.CursorY = NewY;
        CurShowInternal();
        KsReleaseSpinLock(&gCursorLock, OldIrql);
        return;
    }

    OldLeft = gCursor.DrawnX - gCursor.HotX;
    OldTop  = gCursor.DrawnY - gCursor.HotY;

    GpPrimaryDisplay.CursorX = NewX;
    GpPrimaryDisplay.CursorY = NewY;
    gCursor.DrawnX           = NewX;
    gCursor.DrawnY           = NewY;

    NewLeft = NewX - gCursor.HotX;
    NewTop  = NewY - gCursor.HotY;
    CurW    = gCursor.Width;
    CurH    = gCursor.Height;

    KsReleaseSpinLock(&gCursorLock, OldIrql);

    if (CurW == 0 || CurH == 0)
    {
        return;
    }

    LONG OldRight = OldLeft + (LONG)CurW;
    LONG OldBot   = OldTop  + (LONG)CurH;
    LONG NewRight = NewLeft + (LONG)CurW;
    LONG NewBot   = NewTop  + (LONG)CurH;

    if (OldLeft < NewRight && OldRight > NewLeft &&
        OldTop  < NewBot   && OldBot   > NewTop)
    {
        LONG UnionL = (OldLeft  < NewLeft)  ? OldLeft  : NewLeft;
        LONG UnionT = (OldTop   < NewTop)   ? OldTop   : NewTop;
        LONG UnionR = (OldRight > NewRight) ? OldRight : NewRight;
        LONG UnionB = (OldBot   > NewBot)   ? OldBot   : NewBot;
        CurPresentClippedRect(UnionL, UnionT, UnionR, UnionB);
    }
    else
    {
        CurPresentClippedRect(OldLeft, OldTop, OldRight, OldBot);
        CurPresentClippedRect(NewLeft, NewTop, NewRight, NewBot);
    }
}

BOOLEAN
VEAPI
VeaUserSetCursorShape(
    IN ULONG        Width,
    IN ULONG        Height,
    IN LONG         HotX,
    IN LONG         HotY,
    IN CONST ULONG *PixelsARGB
)
{
    BOOLEAN IsUser = (KeGetPreviousMode() == UserMode);
    KIRQL OldIrql;
    LONG OldLeft, OldTop, NewLeft, NewTop;
    ULONG OldW, OldH;

    DPRINT("[VEAPI/CUR] VeaUserSetCursorShape: %lux%lu Hot=(%ld,%ld)\n",
           Width, Height, HotX, HotY);

    if (!PixelsARGB || Width == 0 || Height == 0 ||
        Width > MAX_CURSOR_SIZE || Height > MAX_CURSOR_SIZE)
    {
        return FALSE;
    }

    SIZE_T ByteLen = (SIZE_T)(Width * Height * sizeof(ULONG));

    if (IsUser)
    {
        if ((ULONG_PTR)PixelsARGB >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS ||
            ((ULONG_PTR)PixelsARGB + ByteLen) >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS)
        {
            return FALSE;
        }

        _VEA_TRY
        {
            RtlCopyMemory(gCursorStagingBuf, (PVOID)PixelsARGB, ByteLen);
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            return FALSE;
        }
        _VEA_END
    }
    else
    {
        RtlCopyMemory(gCursorStagingBuf, (PVOID)PixelsARGB, ByteLen);
    }

    CurEnsureLock();
    KsAcquireSpinLock(&gCursorLock, &OldIrql);

    OldLeft = gCursor.DrawnX - gCursor.HotX;
    OldTop  = gCursor.DrawnY - gCursor.HotY;
    OldW    = gCursor.Width;
    OldH    = gCursor.Height;

    CurHideInternal();
    gCursor.Width    = Width;
    gCursor.Height   = Height;
    gCursor.HotX     = HotX;
    gCursor.HotY     = HotY;
    RtlCopyMemory(gCursor.Pixels, gCursorStagingBuf, ByteLen);
    gCursor.HasShape = TRUE;
    gCursor.DrawnX   = GpPrimaryDisplay.CursorX;
    gCursor.DrawnY   = GpPrimaryDisplay.CursorY;
    NewLeft          = gCursor.DrawnX - HotX;
    NewTop           = gCursor.DrawnY - HotY;
    CurShowInternal();

    KsReleaseSpinLock(&gCursorLock, OldIrql);

    if (GpPrimaryDisplay.BackBuffer != NULL)
    {
        if (OldW > 0 && OldH > 0)
        {
            CurPresentClippedRect(OldLeft, OldTop, OldLeft + (LONG)OldW, OldTop + (LONG)OldH);
        }
        CurPresentClippedRect(NewLeft, NewTop, NewLeft + (LONG)Width, NewTop + (LONG)Height);
    }

    return TRUE;
}

INT
VEAPI
VeaUserShowCursor(
    IN BOOLEAN bShow
)
{
    KIRQL OldIrql;
    INT NewCount;
    LONG CurLeft, CurTop;
    ULONG CurW, CurH;
    BOOLEAN NeedPresent = FALSE;

    CurEnsureLock();
    KsAcquireSpinLock(&gCursorLock, &OldIrql);

    if (bShow)
    {
        gCursor.ShowCount++;
        if (gCursor.ShowCount == 0)
        {
            CurShowInternal();
            NeedPresent = TRUE;
        }
    }
    else
    {
        gCursor.ShowCount--;
        if (gCursor.ShowCount == -1)
        {
            CurHideInternal();
            NeedPresent = TRUE;
        }
    }

    NewCount = gCursor.ShowCount;
    CurLeft  = GpPrimaryDisplay.CursorX - gCursor.HotX;
    CurTop   = GpPrimaryDisplay.CursorY - gCursor.HotY;
    CurW     = gCursor.Width;
    CurH     = gCursor.Height;
    KsReleaseSpinLock(&gCursorLock, OldIrql);

    if (NeedPresent && GpPrimaryDisplay.BackBuffer != NULL && CurW > 0 && CurH > 0)
    {
        CurPresentClippedRect(CurLeft, CurTop, CurLeft + (LONG)CurW, CurTop + (LONG)CurH);
    }

    return NewCount;
}

BOOLEAN
VEAPI
VeaUserGetCursorPos(
    OUT PPOINT lpPoint
)
{
    BOOLEAN IsUser = (KeGetPreviousMode() == UserMode);
    POINT Pt;

    if (!lpPoint) return FALSE;

    Pt.x = GpPrimaryDisplay.CursorX;
    Pt.y = GpPrimaryDisplay.CursorY;

    if (IsUser)
    {
        if ((ULONG_PTR)lpPoint >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS ||
            ((ULONG_PTR)lpPoint + sizeof(POINT)) >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS)
        {
            return FALSE;
        }

        _VEA_TRY
        {
            RtlCopyMemory(lpPoint, &Pt, sizeof(POINT));
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            return FALSE;
        }
        _VEA_END
    }
    else
    {
        *lpPoint = Pt;
    }

    return TRUE;
}

BOOLEAN
VEAPI
VeaUserSetCursorPos(
    IN LONG X,
    IN LONG Y
)
{
    LONG ScrW = (LONG)GpPrimaryDisplay.PrimarySurface.Width;
    LONG ScrH = (LONG)GpPrimaryDisplay.PrimarySurface.Height;

    if (X < 0) X = 0;
    if (Y < 0) Y = 0;
    if (ScrW > 0 && X >= ScrW) X = ScrW - 1;
    if (ScrH > 0 && Y >= ScrH) Y = ScrH - 1;

    gCursorX = X;
    gCursorY = Y;
    EngMovePointer(X, Y);
    return TRUE;
}

HWND
VEAPI
VeaUserSetCapture(
    IN HWND hWnd
)
{
    HWND OldCapture = gCaptureWindow;

    if (hWnd != NULL)
    {
        PWND_OBJECT Wnd = WndLookupHandle(hWnd);
        if (!Wnd || Wnd->OwnerProcess != (PEPROCESS)PtGetCurrentProcess())
        {
            return NULL;
        }
        WndDereference(Wnd);
    }

    gCaptureWindow = hWnd;
    return OldCapture;
}

BOOLEAN
VEAPI
VeaUserReleaseCapture(VOID)
{
    gCaptureWindow = NULL;
    return TRUE;
}

/* EOF */