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
static KMUTEX           gFtEngineMutex;
static PGDI_FONT_OBJECT gDefaultFontObj = NULL;

static
BOOLEAN
GdipAsciiEqualIgnoreCase(
    IN CONST CHAR *A,
    IN CONST CHAR *B
)
{
    if (!A || !B) return FALSE;

    while (*A && *B)
    {
        CHAR Ca = *A++;
        CHAR Cb = *B++;
        if (Ca >= 'A' && Ca <= 'Z') Ca += ('a' - 'A');
        if (Cb >= 'A' && Cb <= 'Z') Cb += ('a' - 'A');
        if (Ca != Cb) return FALSE;
    }

    return (*A == '\0' && *B == '\0');
}

static
VOID
GdipReleaseLoadedFace(
    IN PGDI_LOADED_FACE FaceObj
)
{
    if (!FaceObj) return;

    KsWaitForSingleObject(&gFtEngineMutex);
    if (FaceObj->Face)
    {
        FT_Done_Face(FaceObj->Face);
    }
    KsReleaseMutex(&gFtEngineMutex, FALSE);

    if (FaceObj->FileBuffer)
    {
        FONT_POOL_FREE(FaceObj->FileBuffer);
    }
    FONT_POOL_FREE(FaceObj);
}

static
PGDI_LOADED_FACE
GdipRegisterFontFaceFromMemory(
    IN PVOID       FontBuffer,
    IN ULONG       FontSize,
    IN CONST CHAR *FilePath OPTIONAL,
    IN ULONG       Flags,
    IN BOOLEAN     IsMemoryFont
)
{
    FT_Face FtFace = NULL;

    KsWaitForSingleObject(&gFtEngineMutex);
    FT_Error FtErr = FT_New_Memory_Face(
        gFtLibrary,
        (CONST FT_Byte *)FontBuffer,
        (FT_Long)FontSize,
        0,
        &FtFace
    );
    KsReleaseMutex(&gFtEngineMutex, FALSE);

    if (FtErr != 0 || !FtFace)
    {
        DPRINT("[VEAPI/FONT] FT_New_Memory_Face gagal: %d\n", FtErr);
        return NULL;
    }

    PGDI_LOADED_FACE NewFace = (PGDI_LOADED_FACE)FONT_POOL_ALLOC(sizeof(GDI_LOADED_FACE));
    if (!NewFace)
    {
        KsWaitForSingleObject(&gFtEngineMutex);
        FT_Done_Face(FtFace);
        KsReleaseMutex(&gFtEngineMutex, FALSE);
        return NULL;
    }

    RtlZeroMemory(NewFace, sizeof(GDI_LOADED_FACE));
    NewFace->Magic        = GDI_FACE_MAGIC;
    NewFace->RefCount     = 1;
    NewFace->Flags        = Flags;
    NewFace->Registered   = TRUE;
    NewFace->IsMemoryFont = IsMemoryFont;
    NewFace->OwnerProcess = (IsMemoryFont || (Flags & FR_PRIVATE)) ? (PEPROCESS)PtGetCurrentProcess() : NULL;
    NewFace->FileBuffer   = FontBuffer;
    NewFace->FileSize     = FontSize;
    NewFace->Face         = FtFace;
    NewFace->Weight       = (FtFace->style_flags & FT_STYLE_FLAG_BOLD) ? FW_BOLD : FW_NORMAL;
    NewFace->Italic       = ((FtFace->style_flags & FT_STYLE_FLAG_ITALIC) != 0);

    if (FilePath)
    {
        for (ULONG i = 0; i < sizeof(NewFace->FilePath) - 1 && FilePath[i] != '\0'; i++)
        {
            NewFace->FilePath[i] = FilePath[i];
        }
    }

    if (FtFace->family_name)
    {
        for (ULONG i = 0; i < sizeof(NewFace->FamilyName) - 1 && FtFace->family_name[i] != '\0'; i++)
        {
            NewFace->FamilyName[i] = FtFace->family_name[i];
        }
    }

    KIRQL OldIrql;
    KsAcquireSpinLock(&gFontListLock, &OldIrql);
    InsertTailList(&gLoadedFaceList, &NewFace->ListEntry);
    KsReleaseSpinLock(&gFontListLock, OldIrql);

    DPRINT("[VEAPI/FONT] Registered Face='%s' (Weight=%lu, Italic=%d, Private=%d)\n",
           NewFace->FamilyName,
           NewFace->Weight,
           NewFace->Italic,
           (NewFace->OwnerProcess != NULL));

    return NewFace;
}

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
    FT_Face Face = FontObj->ParentFace->Face;

    RtlZeroMemory(FontObj->Glyphs, sizeof(FontObj->Glyphs));
    if (!Face) return;

    KsWaitForSingleObject(&gFtEngineMutex);

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

    KsReleaseMutex(&gFtEngineMutex, FALSE);
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
    OUT PULONG OutHeight,
    IN BOOLEAN FromUser
)
{
    CHAR LocalText[256];
    BOOLEAN IsUser = FromUser;

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

static
HFONT
GdipCreateFontFromFile(
    IN CONST CHAR *FontPath,
    IN ULONG       PixelHeight
)
{
    if (VeaGdiAddFontResourceEx(FontPath, 0, NULL) <= 0)
    {
        return NULL;
    }

    PGDI_LOADED_FACE TargetFace = NULL;
    KIRQL OldIrql;

    KsAcquireSpinLock(&gFontListLock, &OldIrql);
    for (PLIST_ENTRY Entry = gLoadedFaceList.Flink; Entry != &gLoadedFaceList; Entry = Entry->Flink)
    {
        PGDI_LOADED_FACE Face = CONTAINING_RECORD(Entry, GDI_LOADED_FACE, ListEntry);
        if (!Face->IsMemoryFont && RtlRawStringEqual(Face->FilePath, (PCHAR)FontPath))
        {
            TargetFace = Face;
            TargetFace->RefCount++;
            break;
        }
    }
    KsReleaseSpinLock(&gFontListLock, OldIrql);

    if (!TargetFace) return NULL;

    PGDI_FONT_OBJECT NewFont = (PGDI_FONT_OBJECT)FONT_POOL_ALLOC(sizeof(GDI_FONT_OBJECT));
    if (!NewFont) return NULL;

    RtlZeroMemory(NewFont, sizeof(GDI_FONT_OBJECT));
    NewFont->Magic       = GDI_FONT_MAGIC;
    NewFont->ParentFace  = TargetFace;
    NewFont->PixelHeight = PixelHeight;

    GdipBakeFontObject(NewFont);

    KsAcquireSpinLock(&gFontListLock, &OldIrql);
    InsertTailList(&gFontObjectList, &NewFont->ListEntry);
    KsReleaseSpinLock(&gFontListLock, OldIrql);

    return (HFONT)NewFont;
}

HFONT
VEAPI
VeaGdiCreateFont(
    IN INT         nHeight,
    IN INT         fnWeight,
    IN BOOLEAN     bItalic,
    IN CONST CHAR *lpszFace
)
{
    CHAR LocalFace[64];
    BOOLEAN IsUser = (KeGetPreviousMode() == UserMode);
    PEPROCESS CurrProc = (PEPROCESS)PtGetCurrentProcess();

    if (!gFtLibrary)
    {
        return NULL;
    }

    ULONG PixelHeight;
    if (nHeight < 0)
        PixelHeight = (ULONG)(-nHeight);
    else if (nHeight > 0)
        PixelHeight = (ULONG)nHeight;
    else
        PixelHeight = FONT_DEFAULT_PIXEL_SIZE;

    if (PixelHeight > 256) PixelHeight = 256;

    ULONG DesiredWeight = (fnWeight >= 600) ? FW_BOLD : FW_NORMAL;

    RtlZeroMemory(LocalFace, sizeof(LocalFace));
    if (lpszFace)
    {
        _VEA_TRY
        {
            for (ULONG i = 0; i < sizeof(LocalFace) - 1; i++)
            {
                if (IsUser && ((ULONG_PTR)(lpszFace + i) >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS))
                {
                    _VEA_YIELD(return NULL);
                }
                LocalFace[i] = lpszFace[i];
                if (lpszFace[i] == '\0') break;
            }
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            return NULL;
        }
        _VEA_END
    }

    /* 1. Cari GDI_LOADED_FACE yang cocok di gLoadedFaceList */
    PGDI_LOADED_FACE ExactMatch = NULL;
    PGDI_LOADED_FACE FamilyMatch = NULL;
    KIRQL OldIrql;

    KsAcquireSpinLock(&gFontListLock, &OldIrql);
    for (PLIST_ENTRY Entry = gLoadedFaceList.Flink; Entry != &gLoadedFaceList; Entry = Entry->Flink)
    {
        PGDI_LOADED_FACE Face = CONTAINING_RECORD(Entry, GDI_LOADED_FACE, ListEntry);
        if (!Face->Registered) continue;

        /* Hormati isolasi proses untuk FR_PRIVATE & MemResource */
        if (Face->OwnerProcess != NULL && Face->OwnerProcess != CurrProc)
        {
            continue;
        }

        if (LocalFace[0] != '\0' &&
            !GdipAsciiEqualIgnoreCase(Face->FamilyName, LocalFace))
        {
            continue;
        }

        if (!FamilyMatch)
        {
            FamilyMatch = Face;
        }
        if (Face->Weight == DesiredWeight && Face->Italic == bItalic)
        {
            ExactMatch = Face;
            break;
        }
    }

    PGDI_LOADED_FACE TargetFace = ExactMatch ? ExactMatch : FamilyMatch;
    if (!TargetFace && gDefaultFontObj)
    {
        /* Fallback ke default system font jika nama font tidak ditemukan */
        TargetFace = gDefaultFontObj->ParentFace;
    }

    if (!TargetFace)
    {
        KsReleaseSpinLock(&gFontListLock, OldIrql);
        return NULL;
    }

    /* 2. Cek apakah ukuran PixelHeight untuk Face ini sudah pernah di-bake */
    for (PLIST_ENTRY Entry = gFontObjectList.Flink; Entry != &gFontObjectList; Entry = Entry->Flink)
    {
        PGDI_FONT_OBJECT ExistingFont = CONTAINING_RECORD(Entry, GDI_FONT_OBJECT, ListEntry);
        if (ExistingFont->ParentFace == TargetFace &&
            ExistingFont->PixelHeight == PixelHeight)
        {
            KsReleaseSpinLock(&gFontListLock, OldIrql);
            return (HFONT)ExistingFont;
        }
    }

    /* Naikkan RefCount sebelum melepas spinlock agar Face tidak terhapus saat baking */
    TargetFace->RefCount++;
    KsReleaseSpinLock(&gFontListLock, OldIrql);

    /* 3. Bake glyph cache untuk ukuran baru */
    PGDI_FONT_OBJECT NewFont = (PGDI_FONT_OBJECT)FONT_POOL_ALLOC(sizeof(GDI_FONT_OBJECT));
    if (!NewFont)
    {
        KsAcquireSpinLock(&gFontListLock, &OldIrql);
        if (TargetFace->RefCount > 0) TargetFace->RefCount--;
        KsReleaseSpinLock(&gFontListLock, OldIrql);
        return NULL;
    }

    RtlZeroMemory(NewFont, sizeof(GDI_FONT_OBJECT));
    NewFont->Magic       = GDI_FONT_MAGIC;
    NewFont->ParentFace  = TargetFace;
    NewFont->PixelHeight = PixelHeight;

    GdipBakeFontObject(NewFont);

    KsAcquireSpinLock(&gFontListLock, &OldIrql);
    InsertTailList(&gFontObjectList, &NewFont->ListEntry);
    KsReleaseSpinLock(&gFontListLock, OldIrql);

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
        KsWaitForSingleObject(&gFtEngineMutex);
        if (FaceToFree->Face) FT_Done_Face(FaceToFree->Face);
        KsReleaseMutex(&gFtEngineMutex, FALSE);

        if (FaceToFree->FileBuffer) FONT_POOL_FREE(FaceToFree->FileBuffer);
        FONT_POOL_FREE(FaceToFree);
    }

    return TRUE;
}

BOOLEAN
VEAPI
GdipDrawTextInternal(
    IN HDC hdc,
    IN INT X,
    IN INT Y,
    IN CONST CHAR *Text,
    IN ULONG ColorARGB,
    IN BOOLEAN FromUser
)
{
    PGDI_DC_OBJECT Dc = VeaGdiGetDc(hdc);
    VEAGDI_SURFACE Views[MAX_VISIBLE_CLIP_RECTS];
    LONG OriginsX[MAX_VISIBLE_CLIP_RECTS];
    LONG OriginsY[MAX_VISIBLE_CLIP_RECTS];
    CHAR LocalText[256];
    BOOLEAN IsUser = FromUser;

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

    ULONG ViewCount = VeaGdiCreateSurfaceViews(
        Dc, Views, OriginsX, OriginsY, MAX_VISIBLE_CLIP_RECTS
    );
    if (ViewCount == 0)
    {
        return (Dc->hWnd != NULL);
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
            LONGLONG ScreenGlyphX = (LONGLONG)Dc->OriginX + PenX + Glyph->BitmapLeft;
            LONGLONG ScreenGlyphY = (LONGLONG)Dc->OriginY + BaselineY - Glyph->BitmapTop;

            for (ULONG v = 0; v < ViewCount; v++)
            {
                LONGLONG ViewX = ScreenGlyphX - OriginsX[v];
                LONGLONG ViewY = ScreenGlyphY - OriginsY[v];

                if (ViewX >= (LONGLONG)Views[v].Width || ViewY >= (LONGLONG)Views[v].Height ||
                    ViewX + (LONGLONG)Glyph->Width <= 0 || ViewY + (LONGLONG)Glyph->Rows <= 0)
                {
                    continue;
                }

                EngDrawGlyphAlpha8(
                    &Views[v],
                    (LONG)ViewX,
                    (LONG)ViewY,
                    Glyph->Width,
                    Glyph->Rows,
                    Glyph->Pitch,
                    Glyph->AlphaBuffer,
                    ColorARGB
                );

                GdiAccumulateDirtyRect(
                    Dc,
                    (LONG)ScreenGlyphX,
                    (LONG)ScreenGlyphY,
                    (LONG)ScreenGlyphX + (LONG)Glyph->Width,
                    (LONG)ScreenGlyphY + (LONG)Glyph->Rows
                );
            }
        }

        PenX += Glyph->AdvanceX;
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
    return GdipDrawTextInternal(
        hdc,
        X,
        Y,
        Text,
        ColorARGB,
        (KeGetPreviousMode() == UserMode)
    );
}

BOOLEAN
VEAPI
VeaGdiGetTextExtent(
    IN CONST CHAR *Text,
    OUT PULONG OutWidth,
    OUT PULONG OutHeight
)
{
    return GdipMeasureTextWithFont(
        gDefaultFontObj,
        Text,
        OutWidth,
        OutHeight,
        (KeGetPreviousMode() == UserMode)
    );
}

BOOLEAN
VEAPI
GdipGetTextExtentFontInternal(
    IN HDC hdc,
    IN CONST CHAR *Text,
    OUT PULONG OutWidth,
    OUT PULONG OutHeight,
    IN BOOLEAN FromUser
)
{
    PGDI_DC_OBJECT Dc = VeaGdiGetDc(hdc);
    return GdipMeasureTextWithFont(GdipResolveDcFont(Dc), Text, OutWidth, OutHeight, FromUser);
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
    return GdipGetTextExtentFontInternal(
        hdc,
        Text,
        OutWidth,
        OutHeight,
        (KeGetPreviousMode() == UserMode)
    );
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
    KsInitializeMutex(&gFtEngineMutex, 0);
    
    FtErr = FT_Init_FreeType(&gFtLibrary);
    if (FtErr != 0 || !gFtLibrary)
    {
        DPRINT("[VEAPI/FONT] FT_Init_FreeType gagal: %d\n", FtErr);
        return STATUS_UNSUCCESSFUL;
    }

    /* 1. Daftarkan default system font ke gLoadedFaceList secara global (Flags = 0) */
    if (VeaGdiAddFontResourceEx(DEFAULT_SYSTEM_FONT_PATH, 0, NULL) <= 0)
    {
        DPRINT("[VEAPI/FONT] Gagal mendaftarkan default system font '%s'\n", DEFAULT_SYSTEM_FONT_PATH);
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    /* 2. Bake ukuran default-nya (lpszFace = NULL akan mengambil face pertama yang terdaftar) */
    HFONT DefaultHandle = VeaGdiCreateFont(
        -(INT)FONT_DEFAULT_PIXEL_SIZE,
        FW_NORMAL,
        FALSE,
        NULL
    );
    if (!DefaultHandle)
    {
        DPRINT("[VEAPI/FONT] Gagal membuat HFONT default dari '%s'\n", DEFAULT_SYSTEM_FONT_PATH);
        return STATUS_UNSUCCESSFUL;
    }

    gDefaultFontObj = (PGDI_FONT_OBJECT)DefaultHandle;
    gFontReady = TRUE;

    UserInvalidateAllWindows();

    return STATUS_SUCCESS;
}

INT
VEAPI
VeaGdiAddFontResourceEx(
    IN CONST CHAR *FilePath,
    IN ULONG       Flags,
    IN PVOID       Reserved OPTIONAL
)
{
    UNREFERENCED_PARAMETER(Reserved);

    CHAR LocalPath[256];
    BOOLEAN IsUser = (KeGetPreviousMode() == UserMode);
    PEPROCESS CurrProc = (PEPROCESS)PtGetCurrentProcess();
    BOOLEAN IsPrivate = ((Flags & FR_PRIVATE) != 0);

    if (!gFtLibrary || !FilePath)
    {
        return 0;
    }

    RtlZeroMemory(LocalPath, sizeof(LocalPath));
    _VEA_TRY
    {
        for (ULONG i = 0; i < sizeof(LocalPath) - 1; i++)
        {
            if (IsUser && ((ULONG_PTR)(FilePath + i) >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS))
            {
                _VEA_YIELD(return 0);
            }
            LocalPath[i] = FilePath[i];
            if (FilePath[i] == '\0') break;
        }
    }
    _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
    _VEA_END

    if (LocalPath[0] == '\0')
    {
        return 0;
    }

    /* Cek apakah file font ini sudah terdaftar di scope yang sama */
    KIRQL OldIrql;
    KsAcquireSpinLock(&gFontListLock, &OldIrql);
    for (PLIST_ENTRY Entry = gLoadedFaceList.Flink; Entry != &gLoadedFaceList; Entry = Entry->Flink)
    {
        PGDI_LOADED_FACE Existing = CONTAINING_RECORD(Entry, GDI_LOADED_FACE, ListEntry);
        if (!Existing->IsMemoryFont &&
            RtlRawStringEqual(Existing->FilePath, LocalPath) &&
            ((IsPrivate && Existing->OwnerProcess == CurrProc) ||
             (!IsPrivate && Existing->OwnerProcess == NULL)))
        {
            Existing->RefCount++;
            Existing->Registered = TRUE;
            KsReleaseSpinLock(&gFontListLock, OldIrql);
            return 1;
        }
    }
    KsReleaseSpinLock(&gFontListLock, OldIrql);

    PVOID FontBuf = NULL;
    ULONG FontBytes = 0;
    if (!VEA_SUCCESS(GdipReadFileToBuffer(LocalPath, &FontBuf, &FontBytes)))
    {
        return 0;
    }

    PGDI_LOADED_FACE NewFace = GdipRegisterFontFaceFromMemory(
        FontBuf, FontBytes, LocalPath, Flags, FALSE
    );
    if (!NewFace)
    {
        FONT_POOL_FREE(FontBuf);
        return 0;
    }

    return 1;
}

BOOLEAN
VEAPI
VeaGdiRemoveFontResourceEx(
    IN CONST CHAR *FilePath,
    IN ULONG       Flags,
    IN PVOID       Reserved OPTIONAL
)
{
    UNREFERENCED_PARAMETER(Reserved);

    CHAR LocalPath[256];
    BOOLEAN IsUser = (KeGetPreviousMode() == UserMode);
    PEPROCESS CurrProc = (PEPROCESS)PtGetCurrentProcess();
    BOOLEAN IsPrivate = ((Flags & FR_PRIVATE) != 0);

    if (!FilePath) return FALSE;

    RtlZeroMemory(LocalPath, sizeof(LocalPath));
    _VEA_TRY
    {
        for (ULONG i = 0; i < sizeof(LocalPath) - 1; i++)
        {
            if (IsUser && ((ULONG_PTR)(FilePath + i) >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS))
            {
                _VEA_YIELD(return FALSE);
            }
            LocalPath[i] = FilePath[i];
            if (FilePath[i] == '\0') break;
        }
    }
    _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
    {
        return FALSE;
    }
    _VEA_END

    PGDI_LOADED_FACE FaceToFree = NULL;
    BOOLEAN Found = FALSE;
    KIRQL OldIrql;

    KsAcquireSpinLock(&gFontListLock, &OldIrql);
    for (PLIST_ENTRY Entry = gLoadedFaceList.Flink; Entry != &gLoadedFaceList; Entry = Entry->Flink)
    {
        PGDI_LOADED_FACE Face = CONTAINING_RECORD(Entry, GDI_LOADED_FACE, ListEntry);
        if (!Face->IsMemoryFont &&
            Face->Registered &&
            RtlRawStringEqual(Face->FilePath, LocalPath) &&
            ((IsPrivate && Face->OwnerProcess == CurrProc) ||
             (!IsPrivate && Face->OwnerProcess == NULL)))
        {
            Found = TRUE;
            Face->Registered = FALSE;
            if (Face->RefCount > 0) Face->RefCount--;
            if (Face->RefCount == 0)
            {
                RemoveEntryList(&Face->ListEntry);
                Face->Magic = 0;
                FaceToFree = Face;
            }
            break;
        }
    }
    KsReleaseSpinLock(&gFontListLock, OldIrql);

    if (FaceToFree)
    {
        GdipReleaseLoadedFace(FaceToFree);
    }

    return Found;
}

HANDLE
VEAPI
VeaGdiAddFontMemResourceEx(
    IN  PVOID  FontData,
    IN  ULONG  FontSize,
    IN  PVOID  Reserved OPTIONAL,
    OUT PULONG OutNumFonts OPTIONAL
)
{
    UNREFERENCED_PARAMETER(Reserved);

    BOOLEAN IsUser = (KeGetPreviousMode() == UserMode);

    if (!gFtLibrary || !FontData || FontSize == 0 || FontSize > FONT_MAX_FILE_SIZE)
    {
        return NULL;
    }

    if (IsUser)
    {
        ULONG_PTR Start = (ULONG_PTR)FontData;
        ULONG_PTR End   = Start + (ULONG_PTR)FontSize;
        if (Start >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS ||
            End < Start ||
            End >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS)
        {
            return NULL;
        }

        if (OutNumFonts &&
            ((ULONG_PTR)OutNumFonts >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS ||
             ((ULONG_PTR)OutNumFonts + sizeof(ULONG)) >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS))
        {
            return NULL;
        }
    }

    PVOID KernelBuf = FONT_POOL_ALLOC(FontSize);
    if (!KernelBuf)
    {
        return NULL;
    }

    _VEA_TRY
    {
        RtlCopyMemory(KernelBuf, FontData, FontSize);
        if (OutNumFonts)
        {
            *OutNumFonts = 1;
        }
    }
    _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
    {
        FONT_POOL_FREE(KernelBuf);
        return NULL;
    }
    _VEA_END

    PGDI_LOADED_FACE NewFace = GdipRegisterFontFaceFromMemory(
        KernelBuf, FontSize, NULL, FR_PRIVATE, TRUE
    );
    if (!NewFace)
    {
        FONT_POOL_FREE(KernelBuf);
        return NULL;
    }

    return (HANDLE)NewFace;
}

BOOLEAN
VEAPI
VeaGdiRemoveFontMemResourceEx(
    IN HANDLE hFontResource
)
{
    if (!hFontResource || MI_IS_USER_ADDRESS((ULONG_PTR)hFontResource))
    {
        return FALSE;
    }

    PGDI_LOADED_FACE Target = (PGDI_LOADED_FACE)hFontResource;
    PGDI_LOADED_FACE FaceToFree = NULL;
    BOOLEAN Found = FALSE;
    KIRQL OldIrql;

    KsAcquireSpinLock(&gFontListLock, &OldIrql);
    for (PLIST_ENTRY Entry = gLoadedFaceList.Flink; Entry != &gLoadedFaceList; Entry = Entry->Flink)
    {
        PGDI_LOADED_FACE Face = CONTAINING_RECORD(Entry, GDI_LOADED_FACE, ListEntry);
        if (Face == Target && Face->Magic == GDI_FACE_MAGIC && Face->IsMemoryFont)
        {
            Found = TRUE;
            Face->Registered = FALSE;
            if (Face->RefCount > 0) Face->RefCount--;
            if (Face->RefCount == 0)
            {
                RemoveEntryList(&Face->ListEntry);
                Face->Magic = 0;
                FaceToFree = Face;
            }
            break;
        }
    }
    KsReleaseSpinLock(&gFontListLock, OldIrql);

    if (FaceToFree)
    {
        GdipReleaseLoadedFace(FaceToFree);
    }

    return Found;
}

/* EOF */