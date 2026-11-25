/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : cursor.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Cursor Management & Resource Parsing
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <userapi.h>
#include "userapi_priv.h"

/* Revision History ------------------------------------------------------
- DATE       : 22-11-2026
- NAME       : RasyaFR-Engkol
- REVISION   : Creating file cursor.c
- DATE       : 24-11-2026
- NAME       : RasyaFR-Engkol
- REVISION   : Migrate default cursor bitmap from .asm to .rsrc
- --------------------------------------------------------------------- */

USER_CURSOR_OBJECT gUserCursors[MAX_USER_CURSORS];
HCURSOR            ghDefaultArrowCursor = NULL;
HCURSOR            ghCurrentCursor      = NULL;

static
PUSER_CURSOR_OBJECT
UserpAllocateCursorSlot(VOID)
{
    for (ULONG i = 0; i < MAX_USER_CURSORS; i++)
    {
        if (!gUserCursors[i].Used)
        {
            RtlZeroMemory(&gUserCursors[i], sizeof(USER_CURSOR_OBJECT));
            gUserCursors[i].Magic = USER_CURSOR_MAGIC;
            gUserCursors[i].Used  = TRUE;
            return &gUserCursors[i];
        }
    }

    return NULL;
}

static
PUSER_CURSOR_OBJECT
UserpValidateCursor(
    IN HCURSOR hCursor
)
{
    if (!hCursor)
    {
        return NULL;
    }

    PUSER_CURSOR_OBJECT Obj = (PUSER_CURSOR_OBJECT)hCursor;
    if (Obj < &gUserCursors[0] || Obj >= &gUserCursors[MAX_USER_CURSORS])
    {
        return NULL;
    }

    if (!Obj->Used || Obj->Magic != USER_CURSOR_MAGIC)
    {
        return NULL;
    }

    return Obj;
}

static
HCURSOR
UserpParseBmpToCursor(
    IN CONST UCHAR *BmpData,
    IN SIZE_T       DataSize,
    IN LONG         HotX,
    IN LONG         HotY,
    IN BOOLEAN      IsSystem
)
{
    if (!BmpData || DataSize == 0)
    {
        return NULL;
    }

    KdPrintf("[USERAPI/CUR] BmpData=%p Size=%lu Magic=%c%c\n",
         BmpData, (ULONG)DataSize,
         DataSize > 0 ? BmpData[0] : '?',
         DataSize > 1 ? BmpData[1] : '?');

    if (DataSize < 54 || BmpData[0] != 'B' || BmpData[1] != 'M')
    {
        return NULL;
    }

    ULONG  PixelOffset = *(const ULONG  *)(BmpData + 0x0A);
    LONG   RawWidth    = *(const LONG   *)(BmpData + 0x12);
    LONG   RawHeight   = *(const LONG   *)(BmpData + 0x16);
    USHORT Bpp         = *(const USHORT *)(BmpData + 0x1C);

    KdPrintf("[USERAPI/CUR] W=%ld H=%ld Bpp=%u Offset=%lu\n",
         RawWidth, RawHeight, Bpp, PixelOffset);

    if (RawWidth <= 0 || RawWidth > MAX_CURSOR_DIM || RawHeight == 0 ||
        (Bpp != 24 && Bpp != 32))
    {
        return NULL;
    }

    BOOLEAN BottomUp = (RawHeight > 0);
    ULONG   Width    = (ULONG)RawWidth;
    ULONG   Height   = (ULONG)(BottomUp ? RawHeight : -RawHeight);

    if (Height > MAX_CURSOR_DIM)
    {
        return NULL;
    }

    ULONG BytesPerPixel = Bpp / 8;
    ULONG RowStride     = ((Width * BytesPerPixel) + 3) & ~3UL;

    if (PixelOffset + (RowStride * Height) > DataSize)
    {
        return NULL;
    }

    PUSER_CURSOR_OBJECT Obj = UserpAllocateCursorSlot();
    if (!Obj)
    {
        return NULL;
    }

    Obj->SystemCursor = IsSystem;
    Obj->Width        = Width;
    Obj->Height       = Height;
    Obj->HotX         = HotX;
    Obj->HotY         = HotY;

    const UCHAR *PixelBase = BmpData + PixelOffset;
    BOOLEAN HasAlphaChannel = FALSE;
    if (Bpp == 32)
    {
        for (ULONG y = 0; y < Height && !HasAlphaChannel; y++)
        {
            const UCHAR *RowPtr = PixelBase + (y * RowStride);
            for (ULONG x = 0; x < Width; x++)
            {
                if (RowPtr[x * 4 + 3] != 0)
                {
                    HasAlphaChannel = TRUE;
                    break;
                }
            }
        }
    }

    for (ULONG y = 0; y < Height; y++)
    {
        ULONG SrcRow = BottomUp ? (Height - 1 - y) : y;
        const UCHAR *RowPtr = PixelBase + (SrcRow * RowStride);

        for (ULONG x = 0; x < Width; x++)
        {
            UCHAR B = RowPtr[x * BytesPerPixel + 0];
            UCHAR G = RowPtr[x * BytesPerPixel + 1];
            UCHAR R = RowPtr[x * BytesPerPixel + 2];
            UCHAR A = (Bpp == 32) ? RowPtr[x * BytesPerPixel + 3] : 0xFF;

            ULONG Rgb = ((ULONG)R << 16) | ((ULONG)G << 8) | (ULONG)B;

            if (Rgb == 0xFF00FF)
            {
                Obj->Pixels[y * Width + x] = 0x00000000;
            }
            else if (Bpp == 32 && HasAlphaChannel)
            {
                Obj->Pixels[y * Width + x] = ((ULONG)A << 24) | Rgb;
            }
            else
            {
                Obj->Pixels[y * Width + x] = 0xFF000000 | Rgb;
            }
        }
    }

    return (HCURSOR)Obj;
}

VOID
VEAPI
UserapiInitDefaultCursor(VOID)
{
    if (!ghDefaultArrowCursor)
    {
        CONST RESOURCE_DATA_ENTRY *ResEntry = UserFindResourceEx(
            gUserApiModule,
            MAKEINTRESOURCE(RT_BITMAP),
            MAKEINTRESOURCE(IDB_32X32_CURSOR),
            LANG_NEUTRAL
        );

        if (ResEntry)
        {
            CONST UCHAR *BmpData = (CONST UCHAR *)UserLockResource(gUserApiModule, ResEntry);
            ULONG BmpSize        = UserSizeofResource(gUserApiModule, ResEntry);

            ghDefaultArrowCursor = UserpParseBmpToCursor(
                BmpData,
                BmpSize,
                0,
                0,
                TRUE
            );
        }
        else
        {
            KdPrintf("[USERAPI/CUR] Gagal menemukan resource IDB_DEFAULT_CURSOR (%u)\n",
                     IDB_DEFAULT_CURSOR);
        }
    }

    if (ghDefaultArrowCursor)
    {
        SetCursor(ghDefaultArrowCursor);
    }
}

HCURSOR
WINAPI
LoadCursorA(
    IN HINSTANCE hInstance,
    IN PSTR      lpCursorName
)
{
    if (!lpCursorName)
    {
        return NULL;
    }

    /* Resource sistem bawaan (hInstance == NULL) */
    if (hInstance == NULL && IS_INTRESOURCE(lpCursorName))
    {
        ULONG_PTR ResId = (ULONG_PTR)lpCursorName;

        switch (ResId)
        {
            case 32512: /* IDC_ARROW */
                return ghDefaultArrowCursor;

            default:
                /* Fallback ke arrow default jika varian cursor lain belum tersedia */
                return ghDefaultArrowCursor;
        }
    }

    /* Load cursor BMP kustom dari section .rsrc milik modul hInstance */
    if (hInstance != NULL)
    {
        CONST RESOURCE_DATA_ENTRY *ResEntry = UserFindResourceEx(
            (HMODULE)hInstance,
            MAKEINTRESOURCE(RT_BITMAP),
            lpCursorName,
            LANG_NEUTRAL
        );

        if (!ResEntry)
        {
            ResEntry = UserFindResourceEx(
                (HMODULE)hInstance,
                MAKEINTRESOURCE(RT_CURSOR),
                lpCursorName,
                LANG_NEUTRAL
            );
        }

        if (ResEntry)
        {
            CONST UCHAR *ResData = (CONST UCHAR *)UserLockResource((HMODULE)hInstance, ResEntry);
            ULONG ResSize        = UserSizeofResource((HMODULE)hInstance, ResEntry);
            return UserpParseBmpToCursor(ResData, ResSize, 0, 0, FALSE);
        }
    }

    return NULL;
}

HCURSOR
WINAPI
SetCursor(
    IN HCURSOR hCursor
)
{
    HCURSOR PrevCursor = ghCurrentCursor;

    if (hCursor == NULL)
    {
        if (ghCurrentCursor != NULL)
        {
            VeaUserShowCursor(FALSE);
            ghCurrentCursor = NULL;
        }
        return PrevCursor;
    }

    PUSER_CURSOR_OBJECT Obj = UserpValidateCursor(hCursor);
    if (!Obj)
    {
        return PrevCursor;
    }

    /* Tidak perlu kirim ulang bitmap ke kernel jika handle-nya masih sama */
    if (ghCurrentCursor != hCursor)
    {
        if (!VeaUserSetCursorShape(Obj->Width, Obj->Height, Obj->HotX, Obj->HotY, Obj->Pixels))
        {
            return PrevCursor;
        }

        if (ghCurrentCursor == NULL)
        {
            VeaUserShowCursor(TRUE);
        }

        ghCurrentCursor = hCursor;
    }

    return PrevCursor;
}

HCURSOR
WINAPI
GetCursor(VOID)
{
    return ghCurrentCursor;
}

HCURSOR
WINAPI
CreateCursor(
    IN HINSTANCE   hInst,
    IN INT         xHotSpot,
    IN INT         yHotSpot,
    IN INT         nWidth,
    IN INT         nHeight,
    IN CONST VOID *pvANDPlane,
    IN CONST VOID *pvXORPlane
)
{
    UNREFERENCED_PARAMETER(hInst);

    if (!pvANDPlane || !pvXORPlane ||
        nWidth <= 0 || nHeight <= 0 ||
        nWidth > MAX_CURSOR_DIM || nHeight > MAX_CURSOR_DIM)
    {
        return NULL;
    }

    PUSER_CURSOR_OBJECT Obj = UserpAllocateCursorSlot();
    if (!Obj)
    {
        return NULL;
    }

    Obj->SystemCursor = FALSE;
    Obj->Width        = (ULONG)nWidth;
    Obj->Height       = (ULONG)nHeight;
    Obj->HotX         = (LONG)xHotSpot;
    Obj->HotY         = (LONG)yHotSpot;

    CONST UCHAR *AndBytes = (CONST UCHAR *)pvANDPlane;
    CONST UCHAR *XorBytes = (CONST UCHAR *)pvXORPlane;
    ULONG RowBytes        = ((ULONG)nWidth + 7) / 8;

    /* Konversi tabel bitmask standar Win32 (AND + XOR plane) ke ARGB 32-bit */
    for (ULONG y = 0; y < (ULONG)nHeight; y++)
    {
        for (ULONG x = 0; x < (ULONG)nWidth; x++)
        {
            UCHAR Mask   = (UCHAR)(0x80 >> (x & 7));
            ULONG ByteIx = y * RowBytes + (x >> 3);
            BOOLEAN AndBit = ((AndBytes[ByteIx] & Mask) != 0);
            BOOLEAN XorBit = ((XorBytes[ByteIx] & Mask) != 0);

            if (AndBit && !XorBit)
            {
                /* Transparan */
                Obj->Pixels[y * (ULONG)nWidth + x] = 0x00000000;
            }
            else if (!AndBit && !XorBit)
            {
                /* Hitam solid */
                Obj->Pixels[y * (ULONG)nWidth + x] = 0xFF000000;
            }
            else
            {
                /* Putih solid (termasuk fallback mode invert) */
                Obj->Pixels[y * (ULONG)nWidth + x] = 0xFFFFFFFF;
            }
        }
    }

    return (HCURSOR)Obj;
}

BOOL
WINAPI
DestroyCursor(
    IN HCURSOR hCursor
)
{
    PUSER_CURSOR_OBJECT Obj = UserpValidateCursor(hCursor);
    if (!Obj || Obj->SystemCursor)
    {
        return FALSE;
    }

    if (ghCurrentCursor == hCursor)
    {
        SetCursor(ghDefaultArrowCursor);
    }

    Obj->Magic = 0;
    Obj->Used  = FALSE;
    return TRUE;
}

BOOL
WINAPI
GetCursorPos(
    OUT LPPOINT lpPoint
)
{
    if (!lpPoint)
    {
        return FALSE;
    }

    return VeaUserGetCursorPos(lpPoint) ? TRUE : FALSE;
}

BOOL
WINAPI
SetCursorPos(
    IN INT X,
    IN INT Y
)
{
    return VeaUserSetCursorPos((LONG)X, (LONG)Y) ? TRUE : FALSE;
}

INT
WINAPI
ShowCursor(
    IN BOOL bShow
)
{
    return VeaUserShowCursor(bShow ? TRUE : FALSE);
}

/* EOF */