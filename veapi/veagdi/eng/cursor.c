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
 * DATE       : 22-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file cursor.c
 * --------------------------------------------------------------------- */

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
CurHideInternal(VOID)
{
    PVEAGDI_SURFACE Primary = EngGetTargetSurface();

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
    PVEAGDI_SURFACE Primary = EngGetTargetSurface();

    if (gCursor.DrawnOnScreen || !gCursor.HasShape || gCursor.ShowCount < 0)
    {
        return;
    }

    if (!Primary->Scan0)
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
                    /* Alpha blend kalau cursor.bmp punya semi-transparent edge */
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
    LONG OldLeft, OldTop;
    ULONG CurW, CurH;

    CurEnsureLock();
    KsAcquireSpinLock(&gCursorLock, &OldIrql);

    OldLeft = gCursor.DrawnX - gCursor.HotX;
    OldTop  = gCursor.DrawnY - gCursor.HotY;
    CurW    = gCursor.Width;
    CurH    = gCursor.Height;

    CurHideInternal();
    GpPrimaryDisplay.CursorX = NewX;
    GpPrimaryDisplay.CursorY = NewY;
    CurShowInternal();

    KsReleaseSpinLock(&gCursorLock, OldIrql);

    if (CurW > 0 && CurH > 0)
    {
        EngPresentRect(OldLeft, OldTop, CurW, CurH);
        EngPresentRect(NewX - gCursor.HotX, NewY - gCursor.HotY, CurW, CurH);
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

    DPRINT("[VEAPI/CUR] VeaUserSetCursorShape: %lux%lu Hot=(%ld,%ld)\n",
           Width, Height, HotX, HotY);

    if (!PixelsARGB || Width == 0 || Height == 0 ||
        Width > MAX_CURSOR_SIZE || Height > MAX_CURSOR_SIZE)
    {
        return FALSE;
    }

    SIZE_T ByteLen = (SIZE_T)(Width * Height * sizeof(ULONG));

    /* Copy dari user-mode di PASSIVE_LEVEL sebelum mengunci gCursorLock */
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

    CurHideInternal();
    gCursor.Width    = Width;
    gCursor.Height   = Height;
    gCursor.HotX     = HotX;
    gCursor.HotY     = HotY;
    RtlCopyMemory(gCursor.Pixels, gCursorStagingBuf, ByteLen);
    gCursor.HasShape = TRUE;
    CurShowInternal();

    KsReleaseSpinLock(&gCursorLock, OldIrql);
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

    CurEnsureLock();
    KsAcquireSpinLock(&gCursorLock, &OldIrql);

    if (bShow)
    {
        gCursor.ShowCount++;
        if (gCursor.ShowCount == 0)
        {
            CurShowInternal();
        }
    }
    else
    {
        gCursor.ShowCount--;
        if (gCursor.ShowCount == -1)
        {
            CurHideInternal();
        }
    }

    NewCount = gCursor.ShowCount;
    KsReleaseSpinLock(&gCursorLock, OldIrql);
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