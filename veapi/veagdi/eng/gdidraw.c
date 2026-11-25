/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : gdidraw.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : GDI Draw
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veapi.h>

/* Revision History ------------------------------------------------------
 * DATE       : 24-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file gdidraw.c
 * --------------------------------------------------------------------- */

BOOLEAN
VEAPI
VeaGdiMoveToEx(
    IN  HDC     hdc,
    IN  INT     X,
    IN  INT     Y,
    OUT LPPOINT lpPoint OPTIONAL
)
{
    PGDI_DC_OBJECT Dc = VeaGdiGetDc(hdc);
    if (!Dc)
    {
        return FALSE;
    }

    if (lpPoint != NULL)
    {
        POINT OldPt;
        OldPt.x = Dc->PenX;
        OldPt.y = Dc->PenY;

        if (KeGetPreviousMode() == UserMode)
        {
            ULONG_PTR Start = (ULONG_PTR)lpPoint;
            ULONG_PTR High  = (ULONG_PTR)MM_HIGHEST_USER_ADDRESS;
            if (Start >= High || sizeof(POINT) > High - Start)
            {
                return FALSE;
            }

            _VEA_TRY
            {
                RtlCopyMemory(lpPoint, &OldPt, sizeof(POINT));
            }
            _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
            {
                return FALSE;
            }
            _VEA_END
        }
        else
        {
            *lpPoint = OldPt;
        }
    }

    Dc->PenX = X;
    Dc->PenY = Y;
    return TRUE;
}

BOOLEAN
VEAPI
VeaGdiLineToCurrent(
    IN HDC hdc,
    IN INT nXEnd,
    IN INT nYEnd
)
{
    PGDI_DC_OBJECT Dc = VeaGdiGetDc(hdc);
    if (!Dc)
    {
        return FALSE;
    }

    ULONG PenColor = 0;
    ULONG PenWidth = 0;

    if (GdipResolvePenInfo(Dc->hPen, &PenColor, &PenWidth))
    {
        if (PenWidth <= 1)
        {
            VeaGdiLineTo(hdc, Dc->PenX, Dc->PenY, nXEnd, nYEnd, PenColor);
        }
        else
        {
            /* Untuk pen tebal, gambar paralel sesuai arah dominan garis */
            LONG Dx = nXEnd - Dc->PenX;
            LONG Dy = nYEnd - Dc->PenY;
            LONG Half = (LONG)(PenWidth / 2);

            for (ULONG w = 0; w < PenWidth; w++)
            {
                LONG Off = (LONG)w - Half;
                if ((Dx >= 0 ? Dx : -Dx) >= (Dy >= 0 ? Dy : -Dy))
                {
                    VeaGdiLineTo(hdc, Dc->PenX, Dc->PenY + Off, nXEnd, nYEnd + Off, PenColor);
                }
                else
                {
                    VeaGdiLineTo(hdc, Dc->PenX + Off, Dc->PenY, nXEnd + Off, nYEnd, PenColor);
                }
            }
        }
    }

    Dc->PenX = nXEnd;
    Dc->PenY = nYEnd;
    return TRUE;
}

BOOLEAN
VEAPI
VeaGdiFillRectBrush(
    IN HDC         hdc,
    IN CONST PRECT lprc,
    IN HBRUSH      hbr
)
{
    ULONG ColorARGB = 0;
    if (!GdipResolveBrushColor(hbr, &ColorARGB))
    {
        /* NULL_BRUSH / HOLLOW_BRUSH tidak menggambar apa-apa tapi tetap valid */
        return (VeaGdiGetDc(hdc) != NULL);
    }

    return VeaGdiFillRect(hdc, lprc, ColorARGB);
}

BOOLEAN
VEAPI
VeaGdiFrameRect(
    IN HDC         hdc,
    IN CONST PRECT lprc,
    IN HBRUSH      hbr
)
{
    PGDI_DC_OBJECT Dc = VeaGdiGetDc(hdc);
    ULONG ColorARGB = 0;
    RECT Rc;

    if (!Dc || !lprc)
    {
        return FALSE;
    }

    if (KeGetPreviousMode() == UserMode)
    {
        ULONG_PTR Start = (ULONG_PTR)lprc;
        ULONG_PTR High  = (ULONG_PTR)MM_HIGHEST_USER_ADDRESS;
        if (Start >= High || sizeof(RECT) > High - Start)
        {
            return FALSE;
        }

        _VEA_TRY
        {
            RtlCopyMemory(&Rc, lprc, sizeof(RECT));
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            return FALSE;
        }
        _VEA_END
    }
    else
    {
        Rc = *lprc;
    }

    if (Rc.left >= Rc.right || Rc.top >= Rc.bottom)
    {
        return FALSE;
    }

    if (!GdipResolveBrushColor(hbr, &ColorARGB))
    {
        return TRUE;
    }

    RECT Top    = { Rc.left,      Rc.top,        Rc.right, Rc.top + 1 };
    RECT Bottom = { Rc.left,      Rc.bottom - 1, Rc.right, Rc.bottom };
    RECT Left   = { Rc.left,      Rc.top + 1,    Rc.left + 1, Rc.bottom - 1 };
    RECT Right  = { Rc.right - 1, Rc.top + 1,    Rc.right, Rc.bottom - 1 };

    VeaGdiFillRectInternal(hdc, &Top, ColorARGB);
    if (Rc.bottom - Rc.top > 1)
    {
        VeaGdiFillRectInternal(hdc, &Bottom, ColorARGB);
    }
    if (Rc.bottom - Rc.top > 2)
    {
        VeaGdiFillRectInternal(hdc, &Left, ColorARGB);
        if (Rc.right - Rc.left > 1)
        {
            VeaGdiFillRectInternal(hdc, &Right, ColorARGB);
        }
    }

    return TRUE;
}

BOOLEAN
VEAPI
VeaGdiRectangle(
    IN HDC hdc,
    IN INT nLeftRect,
    IN INT nTopRect,
    IN INT nRightRect,
    IN INT nBottomRect
)
{
    PGDI_DC_OBJECT Dc = VeaGdiGetDc(hdc);
    if (!Dc || nLeftRect >= nRightRect || nTopRect >= nBottomRect)
    {
        return FALSE;
    }

    ULONG PenColor = 0, PenWidth = 0;
    ULONG BrushColor = 0;
    BOOLEAN HasPen   = GdipResolvePenInfo(Dc->hPen, &PenColor, &PenWidth);
    BOOLEAN HasBrush = GdipResolveBrushColor(Dc->hBrush, &BrushColor);

    LONG W = nRightRect - nLeftRect;
    LONG H = nBottomRect - nTopRect;
    LONG Border = HasPen ? (LONG)PenWidth : 0;

    if (Border * 2 > W) Border = W / 2;
    if (Border * 2 > H) Border = H / 2;

    /* 1. Gambar border HPEN di bagian luar */
    if (HasPen && Border > 0)
    {
        RECT Top    = { nLeftRect, nTopRect, nRightRect, nTopRect + Border };
        RECT Bottom = { nLeftRect, nBottomRect - Border, nRightRect, nBottomRect };
        RECT Left   = { nLeftRect, nTopRect + Border, nLeftRect + Border, nBottomRect - Border };
        RECT Right  = { nRightRect - Border, nTopRect + Border, nRightRect, nBottomRect - Border };

        VeaGdiFillRectInternal(hdc, &Top, PenColor);
        VeaGdiFillRectInternal(hdc, &Bottom, PenColor);
        if (nTopRect + Border < nBottomRect - Border)
        {
            VeaGdiFillRectInternal(hdc, &Left, PenColor);
            VeaGdiFillRectInternal(hdc, &Right, PenColor);
        }
    }

    /* 2. Isi bagian dalam pakai HBRUSH */
    if (HasBrush)
    {
        RECT Inner = {
            nLeftRect + Border,
            nTopRect + Border,
            nRightRect - Border,
            nBottomRect - Border
        };

        if (Inner.left < Inner.right && Inner.top < Inner.bottom)
        {
            VeaGdiFillRectInternal(hdc, &Inner, BrushColor);
        }
    }

    return TRUE;
}

/* EOF */