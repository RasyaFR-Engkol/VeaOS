/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : font.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : GDI Font
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veapi.h>
#include <ft2build.h>
#include FT_FREETYPE_H

/* Revision History ------------------------------------------------------
 * DATE       : 21-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file font.c
 * --------------------------------------------------------------------- */

static FT_Library       gFtLibrary      = NULL;
static BOOLEAN          gFontReady      = FALSE;
static LIST_ENTRY       gLoadedFaceList;
static LIST_ENTRY       gFontObjectList;
static KSPIN_LOCK       gFontListLock;
static KSPIN_LOCK       gFtEngineLock;
static PGDI_FONT_OBJECT gDefaultFontObj = NULL;

static
VEASTATUS
GdipReadFileToBuffer(
    IN CONST CHAR *FilePath,
    OUT PVOID *OutBuffer,
    OUT PULONG OutSize
)
{
    VEASTATUS Status;
    HANDLE FileHandle = NULL;
    ANSI_STRING PathStr;
    OBJECT_ATTRIBUTES ObjAttr;
    OIP_INFORMATION IoStatus;
    FILE_STANDARD_INFORMATION FileInfo;

    if (!FilePath || !OutBuffer || !OutSize)
    {
        return STATUS_INVALID_PARAMETER;
    }

    *OutBuffer = NULL;
    *OutSize   = 0;

    RtlInitAnsiString(&PathStr, (PCHAR)FilePath);
    InitializeObjectAttributes(&ObjAttr, &PathStr, OBJ_CASE_INSENSITIVE, NULL, NULL);
    RtlZeroMemory(&IoStatus, sizeof(IoStatus));

    Status = VzOpenFile(
        &FileHandle,
        FILE_READ_DATA | VEA_SYNCHRONIZE,
        &ObjAttr,
        &IoStatus,
        FILE_SHARE_READ,
        FILE_SYNCHRONOUS_IO_NONALERT
    );
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[VEAPI/FONT] Gagal buka '%s': 0x%08X\n", FilePath, Status);
        return Status;
    }

    /* Pakai EndOfFile jika didukung veafat, kalau 0 pakai kapasitas awal 512 KB */
    ULONG Capacity = 512 * 1024;
    RtlZeroMemory(&FileInfo, sizeof(FileInfo));
    Status = VzQueryInformationFile(
        FileHandle,
        &IoStatus,
        &FileInfo,
        sizeof(FileInfo),
        FileStandardInformation
    );
    if (VEA_SUCCESS(Status) && FileInfo.EndOfFile.QuadPart > 0)
    {
        if (FileInfo.EndOfFile.QuadPart > (16 * 1024 * 1024))
        {
            VzClose(FileHandle);
            return STATUS_INVALID_IMAGE_FORMAT;
        }
        Capacity = (ULONG)FileInfo.EndOfFile.QuadPart;
    }

    PUCHAR Buffer = (PUCHAR)FONT_POOL_ALLOC(Capacity);
    if (!Buffer)
    {
        VzClose(FileHandle);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    ULONG TotalRead = 0;
    while (TRUE)
    {
        if (TotalRead >= Capacity)
        {
            ULONG NewCap = Capacity * 2;
            if (NewCap > (16 * 1024 * 1024))
            {
                FONT_POOL_FREE(Buffer);
                VzClose(FileHandle);
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            PUCHAR NewBuf = (PUCHAR)FONT_POOL_ALLOC(NewCap);
            if (!NewBuf)
            {
                FONT_POOL_FREE(Buffer);
                VzClose(FileHandle);
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            RtlCopyMemory(NewBuf, Buffer, TotalRead);
            FONT_POOL_FREE(Buffer);
            Buffer   = NewBuf;
            Capacity = NewCap;
        }

        ULONG Chunk = Capacity - TotalRead;
        if (Chunk > 65536) Chunk = 65536;

        LARGE_INTEGER ByteOffset;
        ByteOffset.QuadPart = (LONGLONG)TotalRead;
        RtlZeroMemory(&IoStatus, sizeof(IoStatus));

        Status = VzReadFile(
            FileHandle,
            NULL,
            NULL,
            NULL,
            &IoStatus,
            Buffer + TotalRead,
            Chunk,
            &ByteOffset,
            NULL
        );

        if (Status == STATUS_END_OF_FILE)
        {
            if (IoStatus.Information > 0 && IoStatus.Information <= Chunk)
            {
                TotalRead += (ULONG)IoStatus.Information;
            }
            break;
        }

        if (!VEA_SUCCESS(Status))
        {
            DPRINT("[VEAPI/FONT] VzReadFile error di offset %lu: 0x%08X\n", TotalRead, Status);
            FONT_POOL_FREE(Buffer);
            VzClose(FileHandle);
            return Status;
        }

        if (IoStatus.Information == 0)
        {
            break;
        }

        TotalRead += (ULONG)IoStatus.Information;
    }

    VzClose(FileHandle);

    if (TotalRead == 0)
    {
        FONT_POOL_FREE(Buffer);
        return STATUS_END_OF_FILE;
    }

    DPRINT("[VEAPI/FONT] Read '%s' OK: %lu bytes (Magic: %02X %02X %02X %02X)\n",
           FilePath, TotalRead, Buffer[0], Buffer[1], Buffer[2], Buffer[3]);

    *OutBuffer = Buffer;
    *OutSize   = TotalRead;
    return STATUS_SUCCESS;
}

static
VOID
GdipBakeFontObject(
    IN PGDI_FONT_OBJECT FontObj
)
{
    KIRQL OldIrql;
    FT_Face Face = FontObj->ParentFace->Face;

    RtlZeroMemory(FontObj->Glyphs, sizeof(FontObj->Glyphs));
    if (!Face) return;

    KsAcquireSpinLock(&gFtEngineLock, &OldIrql);

    FT_Set_Pixel_Sizes(Face, 0, FontObj->PixelHeight);

    FontObj->Ascender   = (INT)(Face->size->metrics.ascender >> 6);
    FontObj->LineHeight = (INT)(Face->size->metrics.height >> 6);
    if (FontObj->LineHeight <= 0)
    {
        FontObj->LineHeight = (INT)FontObj->PixelHeight + 4;
    }

    for (ULONG Ch = FONT_CACHE_FIRST_CHAR; Ch <= FONT_CACHE_LAST_CHAR; Ch++)
    {
        FT_Error Err = FT_Load_Char(Face, Ch, FT_LOAD_RENDER | FT_LOAD_NO_BITMAP);
        if (Err != 0) continue;

        FT_GlyphSlot Slot       = Face->glyph;
        FT_Bitmap *Bmp          = &Slot->bitmap;
        PGDI_CACHED_GLYPH Entry = &FontObj->Glyphs[Ch - FONT_CACHE_FIRST_CHAR];

        Entry->Width       = Bmp->width;
        Entry->Rows        = Bmp->rows;
        Entry->Pitch       = (Bmp->pitch < 0) ? -Bmp->pitch : Bmp->pitch;
        Entry->BitmapLeft  = Slot->bitmap_left;
        Entry->BitmapTop   = Slot->bitmap_top;
        Entry->AdvanceX    = (INT)(Slot->advance.x >> 6);
        Entry->AlphaBuffer = NULL;

        if (Entry->Width > 0 && Entry->Rows > 0 && Bmp->buffer != NULL)
        {
            SIZE_T BufBytes = (SIZE_T)Entry->Pitch * Entry->Rows;
            Entry->AlphaBuffer = (PUCHAR)FONT_POOL_ALLOC(BufBytes);
            if (!Entry->AlphaBuffer) continue;

            RtlCopyMemory(Entry->AlphaBuffer, Bmp->buffer, BufBytes);
        }

        Entry->Valid = TRUE;
    }

    KsReleaseSpinLock(&gFtEngineLock, OldIrql);
}

static
PGDI_FONT_OBJECT
GdipValidateFontHandle(
    IN HFONT hFont
)
{
    if (!hFont || MI_IS_USER_ADDRESS((ULONG_PTR)hFont))
    {
        return NULL;
    }

    PGDI_FONT_OBJECT FontObj = (PGDI_FONT_OBJECT)hFont;
    if (FontObj->Magic != GDI_FONT_MAGIC || !FontObj->ParentFace)
    {
        return NULL;
    }

    return FontObj;
}

static
PGDI_FONT_OBJECT
GdipResolveDcFont(
    IN PGDI_DC_OBJECT Dc
)
{
    if (Dc && Dc->hFont)
    {
        PGDI_FONT_OBJECT Selected = GdipValidateFontHandle(Dc->hFont);
        if (Selected) return Selected;
    }
    return gDefaultFontObj;
}

static
BOOLEAN
GdipMeasureTextWithFont(
    IN PGDI_FONT_OBJECT FontObj,
    IN CONST CHAR *Text,
    OUT PULONG OutWidth,
    OUT PULONG OutHeight
)
{
    CHAR LocalText[256];
    BOOLEAN IsUser = (KeGetPreviousMode() == UserMode);

    if (!Text || !OutWidth || !OutHeight)
    {
        return FALSE;
    }

    if (!gFontReady || !FontObj)
    {
        *OutWidth  = 0;
        *OutHeight = 16;
        return FALSE;
    }

    RtlZeroMemory(LocalText, sizeof(LocalText));
    _VEA_TRY
    {
        for (ULONG i = 0; i < sizeof(LocalText) - 1; i++)
        {
            if (IsUser && ((ULONG_PTR)(Text + i) >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS))
            {
                _VEA_YIELD(return FALSE);
            }
            LocalText[i] = Text[i];
            if (Text[i] == '\0') break;
        }
    }
    _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
    {
        return FALSE;
    }
    _VEA_END

    ULONG MaxWidth = 0;
    ULONG CurWidth = 0;
    ULONG Lines    = 1;

    for (ULONG i = 0; LocalText[i] != '\0'; i++)
    {
        UCHAR Ch = (UCHAR)LocalText[i];

        if (Ch == '\n')
        {
            if (CurWidth > MaxWidth) MaxWidth = CurWidth;
            CurWidth = 0;
            Lines++;
            continue;
        }

        if (Ch < FONT_CACHE_FIRST_CHAR || Ch > FONT_CACHE_LAST_CHAR)
        {
            Ch = '?';
        }

        PGDI_CACHED_GLYPH Glyph = &FontObj->Glyphs[Ch - FONT_CACHE_FIRST_CHAR];
        if (Glyph->Valid)
        {
            CurWidth += (ULONG)Glyph->AdvanceX;
        }
    }

    if (CurWidth > MaxWidth) MaxWidth = CurWidth;

    *OutWidth  = MaxWidth;
    *OutHeight = Lines * (ULONG)FontObj->LineHeight;
    return TRUE;
}

HFONT
VEAPI
VeaGdiCreateFont(
    IN CONST CHAR *FontPath,
    IN ULONG PixelHeight
)
{
    CHAR LocalPath[256];
    BOOLEAN IsUser = (KeGetPreviousMode() == UserMode);
    PLIST_ENTRY Entry;
    KIRQL OldIrql;

    if (!gFtLibrary || !FontPath || PixelHeight == 0 || PixelHeight > 256)
    {
        return NULL;
    }

    RtlZeroMemory(LocalPath, sizeof(LocalPath));
    _VEA_TRY
    {
        for (ULONG i = 0; i < sizeof(LocalPath) - 1; i++)
        {
            if (IsUser && ((ULONG_PTR)(FontPath + i) >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS))
            {
                _VEA_YIELD(return NULL);
            }
            LocalPath[i] = FontPath[i];
            if (FontPath[i] == '\0') break;
        }
    }
    _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
    {
        return NULL;
    }
    _VEA_END

    if (LocalPath[0] == '\0')
    {
        return NULL;
    }

    KsAcquireSpinLock(&gFontListLock, &OldIrql);
    for (Entry = gFontObjectList.Flink; Entry != &gFontObjectList; Entry = Entry->Flink)
    {
        PGDI_FONT_OBJECT ExistingFont = CONTAINING_RECORD(Entry, GDI_FONT_OBJECT, ListEntry);
        if (ExistingFont->PixelHeight == PixelHeight &&
            RtlRawStringEqual(ExistingFont->ParentFace->FilePath, LocalPath))
        {
            KsReleaseSpinLock(&gFontListLock, OldIrql);
            return (HFONT)ExistingFont;
        }
    }

    PGDI_LOADED_FACE TargetFace = NULL;
    for (Entry = gLoadedFaceList.Flink; Entry != &gLoadedFaceList; Entry = Entry->Flink)
    {
        PGDI_LOADED_FACE ExistingFace = CONTAINING_RECORD(Entry, GDI_LOADED_FACE, ListEntry);
        if (RtlRawStringEqual(ExistingFace->FilePath, LocalPath))
        {
            TargetFace = ExistingFace;
            TargetFace->RefCount++;
            break;
        }
    }
    KsReleaseSpinLock(&gFontListLock, OldIrql);

    BOOLEAN NewlyLoadedFace = FALSE;
    if (!TargetFace)
    {
        PVOID FontBuf = NULL;
        ULONG FontBytes = 0;
        if (!VEA_SUCCESS(GdipReadFileToBuffer(LocalPath, &FontBuf, &FontBytes)))
        {
            return NULL;
        }

        FT_Face FtFace = NULL;
        KsAcquireSpinLock(&gFtEngineLock, &OldIrql);
        FT_Error FtErr = FT_New_Memory_Face(
            gFtLibrary,
            (CONST FT_Byte *)FontBuf,
            (FT_Long)FontBytes,
            0,
            &FtFace
        );
        KsReleaseSpinLock(&gFtEngineLock, OldIrql);

        if (FtErr != 0 || !FtFace)
        {
            DPRINT("[VEAPI/FONT] FT_New_Memory_Face gagal '%s': %d\n", LocalPath, FtErr);
            FONT_POOL_FREE(FontBuf);
            return NULL;
        }

        TargetFace = (PGDI_LOADED_FACE)FONT_POOL_ALLOC(sizeof(GDI_LOADED_FACE));
        if (!TargetFace)
        {
            KsAcquireSpinLock(&gFtEngineLock, &OldIrql);
            FT_Done_Face(FtFace);
            KsReleaseSpinLock(&gFtEngineLock, OldIrql);
            FONT_POOL_FREE(FontBuf);
            return NULL;
        }

        RtlZeroMemory(TargetFace, sizeof(GDI_LOADED_FACE));
        TargetFace->Magic      = GDI_FACE_MAGIC;
        TargetFace->RefCount   = 1;
        TargetFace->FileBuffer = FontBuf;
        TargetFace->FileSize   = FontBytes;
        TargetFace->Face       = FtFace;
        RtlCopyMemory(TargetFace->FilePath, LocalPath, sizeof(LocalPath));
        NewlyLoadedFace = TRUE;
    }

    PGDI_FONT_OBJECT NewFont = (PGDI_FONT_OBJECT)FONT_POOL_ALLOC(sizeof(GDI_FONT_OBJECT));
    if (!NewFont)
    {
        if (NewlyLoadedFace)
        {
            KsAcquireSpinLock(&gFtEngineLock, &OldIrql);
            FT_Done_Face(TargetFace->Face);
            KsReleaseSpinLock(&gFtEngineLock, OldIrql);
            FONT_POOL_FREE(TargetFace->FileBuffer);
            FONT_POOL_FREE(TargetFace);
        }
        else
        {
            KsAcquireSpinLock(&gFontListLock, &OldIrql);
            if (TargetFace->RefCount > 0) TargetFace->RefCount--;
            KsReleaseSpinLock(&gFontListLock, OldIrql);
        }
        return NULL;
    }

    RtlZeroMemory(NewFont, sizeof(GDI_FONT_OBJECT));
    NewFont->Magic       = GDI_FONT_MAGIC;
    NewFont->ParentFace  = TargetFace;
    NewFont->PixelHeight = PixelHeight;

    GdipBakeFontObject(NewFont);

    KsAcquireSpinLock(&gFontListLock, &OldIrql);
    if (NewlyLoadedFace)
    {
        InsertTailList(&gLoadedFaceList, &TargetFace->ListEntry);
    }
    InsertTailList(&gFontObjectList, &NewFont->ListEntry);
    KsReleaseSpinLock(&gFontListLock, OldIrql);

    DPRINT("[VEAPI/FONT] CreateFont '%s' (%lupx) -> Family='%s', HFONT=%p\n",
           LocalPath,
           PixelHeight,
           TargetFace->Face->family_name ? TargetFace->Face->family_name : "Unknown",
           NewFont);

    return (HFONT)NewFont;
}

HFONT
VEAPI
VeaGdiSelectFont(
    IN HDC hdc,
    IN HFONT hFont
)
{
    PGDI_DC_OBJECT Dc = VeaGdiGetDc(hdc);
    if (!Dc) return NULL;

    HFONT OldFont = Dc->hFont ? Dc->hFont : (HFONT)gDefaultFontObj;

    if (hFont == NULL)
    {
        Dc->hFont = (HFONT)gDefaultFontObj;
        return OldFont;
    }

    PGDI_FONT_OBJECT ValidFont = GdipValidateFontHandle(hFont);
    if (!ValidFont)
    {
        return NULL;
    }

    Dc->hFont = (HFONT)ValidFont;
    return OldFont;
}

BOOLEAN
VEAPI
VeaGdiDeleteFont(
    IN HFONT hFont
)
{
    KIRQL OldIrql;
    PGDI_FONT_OBJECT FontObj = GdipValidateFontHandle(hFont);

    if (!FontObj || FontObj == gDefaultFontObj)
    {
        return FALSE;
    }

    PGDI_LOADED_FACE FaceToFree = NULL;

    KsAcquireSpinLock(&gFontListLock, &OldIrql);
    RemoveEntryList(&FontObj->ListEntry);
    FontObj->Magic = 0;

    PGDI_LOADED_FACE ParentFace = FontObj->ParentFace;
    if (ParentFace->RefCount > 0)
    {
        ParentFace->RefCount--;
    }
    if (ParentFace->RefCount == 0)
    {
        RemoveEntryList(&ParentFace->ListEntry);
        ParentFace->Magic = 0;
        FaceToFree = ParentFace;
    }
    KsReleaseSpinLock(&gFontListLock, OldIrql);

    for (ULONG i = 0; i < FONT_CACHE_COUNT; i++)
    {
        if (FontObj->Glyphs[i].AlphaBuffer)
        {
            FONT_POOL_FREE(FontObj->Glyphs[i].AlphaBuffer);
        }
    }
    FONT_POOL_FREE(FontObj);

    if (FaceToFree)
    {
        KsAcquireSpinLock(&gFtEngineLock, &OldIrql);
        if (FaceToFree->Face) FT_Done_Face(FaceToFree->Face);
        KsReleaseSpinLock(&gFtEngineLock, OldIrql);

        if (FaceToFree->FileBuffer) FONT_POOL_FREE(FaceToFree->FileBuffer);
        FONT_POOL_FREE(FaceToFree);
    }

    return TRUE;
}

BOOLEAN
VEAPI
VeaGdiDrawText(
    IN HDC hdc,
    IN INT X,
    IN INT Y,
    IN CONST CHAR *Text,
    IN ULONG ColorARGB
)
{
    PGDI_DC_OBJECT Dc = VeaGdiGetDc(hdc);
    VEAGDI_SURFACE View;
    LONG ViewOriginX;
    LONG ViewOriginY;
    CHAR LocalText[256];
    BOOLEAN IsUser = (KeGetPreviousMode() == UserMode);

    if (!Dc || !Text || !gFontReady)
    {
        return FALSE;
    }

    PGDI_FONT_OBJECT FontObj = GdipResolveDcFont(Dc);
    if (!FontObj)
    {
        return FALSE;
    }

    RtlZeroMemory(LocalText, sizeof(LocalText));
    _VEA_TRY
    {
        for (ULONG i = 0; i < sizeof(LocalText) - 1; i++)
        {
            if (IsUser && ((ULONG_PTR)(Text + i) >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS))
            {
                _VEA_YIELD(return FALSE);
            }
            LocalText[i] = Text[i];
            if (Text[i] == '\0') break;
        }
    }
    _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
    {
        return FALSE;
    }
    _VEA_END

    if (!VeaGdiCreateSurfaceView(Dc, &View, &ViewOriginX, &ViewOriginY))
    {
        return FALSE;
    }

    INT PenX = X;
    INT BaselineY = Y + FontObj->Ascender;

    for (ULONG i = 0; LocalText[i] != '\0'; i++)
    {
        UCHAR Ch = (UCHAR)LocalText[i];

        if (Ch == '\n')
        {
            PenX = X;
            BaselineY += FontObj->LineHeight;
            continue;
        }

        if (Ch < FONT_CACHE_FIRST_CHAR || Ch > FONT_CACHE_LAST_CHAR)
        {
            Ch = '?';
        }

        PGDI_CACHED_GLYPH Glyph = &FontObj->Glyphs[Ch - FONT_CACHE_FIRST_CHAR];
        if (!Glyph->Valid) continue;

        if (Glyph->AlphaBuffer && Glyph->Width > 0 && Glyph->Rows > 0)
        {
            LONGLONG ViewX = (LONGLONG)Dc->OriginX + PenX + Glyph->BitmapLeft - ViewOriginX;
            LONGLONG ViewY = (LONGLONG)Dc->OriginY + BaselineY - Glyph->BitmapTop - ViewOriginY;

            EngDrawGlyphAlpha8(
                &View,
                (LONG)ViewX,
                (LONG)ViewY,
                Glyph->Width,
                Glyph->Rows,
                Glyph->Pitch,
                Glyph->AlphaBuffer,
                ColorARGB
            );
        }

        PenX += Glyph->AdvanceX;
    }

    return TRUE;
}

BOOLEAN
VEAPI
VeaGdiGetTextExtent(
    IN CONST CHAR *Text,
    OUT PULONG OutWidth,
    OUT PULONG OutHeight
)
{
    return GdipMeasureTextWithFont(gDefaultFontObj, Text, OutWidth, OutHeight);
}

BOOLEAN
VEAPI
VeaGdiGetTextExtentFont(
    IN HDC hdc,
    IN CONST CHAR *Text,
    OUT PULONG OutWidth,
    OUT PULONG OutHeight
)
{
    PGDI_DC_OBJECT Dc = VeaGdiGetDc(hdc);
    return GdipMeasureTextWithFont(GdipResolveDcFont(Dc), Text, OutWidth, OutHeight);
}

VEASTATUS
VEAPI
GdiInitializeFontEngine(VOID)
{
    FT_Error FtErr;

    if (gFontReady)
    {
        return STATUS_SUCCESS;
    }

    InitializeListHead(&gLoadedFaceList);
    InitializeListHead(&gFontObjectList);
    KsInitializeSpinlock(&gFontListLock);
    KsInitializeSpinlock(&gFtEngineLock);

    FtErr = FT_Init_FreeType(&gFtLibrary);
    if (FtErr != 0 || !gFtLibrary)
    {
        DPRINT("[VEAPI/FONT] FT_Init_FreeType gagal: %d\n", FtErr);
        return STATUS_UNSUCCESSFUL;
    }

    HFONT DefaultHandle = VeaGdiCreateFont(DEFAULT_SYSTEM_FONT_PATH, FONT_DEFAULT_PIXEL_SIZE);
    if (!DefaultHandle)
    {
        DPRINT("[VEAPI/FONT] Gagal memuat default system font '%s'\n", DEFAULT_SYSTEM_FONT_PATH);
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    gDefaultFontObj = (PGDI_FONT_OBJECT)DefaultHandle;
    gFontReady = TRUE;

    UserInvalidateAllWindows();

    return STATUS_SUCCESS;
}

/* EOF */