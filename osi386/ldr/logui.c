/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : logui.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Interactive Log UI
 ----- Effective since MM-YYYY til FOREVER ------------------------------- */

#include <osi386.h>
#include "font8x16_vgabasic.h"

/* Revision History ------------------------------------------------------
 * DATE       : 26-07-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file logui.c
 * --------------------------------------------------------------------- */

// Warna 32-bit (XRGB / ARGB8888)
#define COLOR_BG          0x00000000  // Hitam Pekat (Background Utama)
#define COLOR_BANNER_BG   0x00C0C0C0  // Light Grey / Silver (Header & Footer)
#define COLOR_BANNER_TEXT 0x00000000  // Hitam (Teks di dalam Banner)
#define COLOR_TEXT        0x00FFFFFF  // Putih (Teks Log Console)

// Reserve Row Atas & Bawah (Dalam Pixel)
#define RESERVE_TOP_Y     80   // Header ter-reserve sampai Y2=80
#define RESERVE_BOTTOM_Y  675  // Footer ter-reserve dari Y2=675 ke bawah
#define LOG_MARGIN_X      35   // Margin kiri/kanan di dalam box (20 + 15)
#define LOG_LINE_HEIGHT   20   // Tinggi baris per log (16px font + 4px padding)

#define MAX_FOOTER_KEYS 5

#define MAX_TARGET_X 800
#define MAX_TARGET_Y 600

static BV_LIST_OPTION BvCurrentList;

// Batas Koordinat Scrolling Region (Box Console Utama)
static ULONG BvScrollMinX = 0;
static ULONG BvScrollMaxX = 0;
static ULONG BvScrollMinY = 0;
static ULONG BvScrollMaxY = 0;

static ULONG BvScrollLeft   = 0;
static ULONG BvScrollTop    = 0;
static ULONG BvScrollRight  = 0;
static ULONG BvScrollBottom = 0;

PVIDEO_BOOT BvInfoBlock = NULL;
static ULONG BvLogCursorX = 0;
static ULONG BvLogCursorY = 0;
extern const UCHAR Font8x16[256][16];

static VEA_FOOTER_KEY BvFooterKeys[MAX_FOOTER_KEYS];
static ULONG BvFooterKeyCount = 0;

BOOLEAN BvFooterHeaderState = 0;

/* EXTERN *********************************************************************/
VOID
VEAPI
BvSetScrollRegion(
    IN ULONG Left,
    IN ULONG Top,
    IN ULONG Right,
    IN ULONG Bottom
);

BOOLEAN
VEAPI
BvExtractVbeInfo(VOID);

/* PRIVATE FUNTION ************************************************************/
ULONG
VEAPI
BvGetLogCursorY(VOID)
{
    return BvLogCursorY;
}

ULONG
VEAPI
BvGetLogCursorX(VOID)
{
    return BvLogCursorX;
}

static
VOID
VEAPI
BvResetLogCursorState(VOID)
{
    // Reset hanya posisi kursor log, tanpa merubah state scroll region.
    BvLogCursorX = BvScrollLeft;
    BvLogCursorY = BvScrollTop;
}

VOID
VEAPI
BvPutPixel(
    IN ULONG Xi, 
    IN ULONG Y2, 
    IN ULONG Color)
{
    if (!BvInfoBlock || !BvInfoBlock->VideoBootAddress) return;
    if (Xi >= BvInfoBlock->W || Y2 >= BvInfoBlock->H) return;

    // AdditionalInformation[0] berisi BytesPerScanLine (Pitch)
    ULONG Pitch = (ULONG_PTR)BvInfoBlock->AdditionalInformation[0];
    ULONG_PTR PixelAddr = (ULONG_PTR)BvInfoBlock->VideoBootAddress + (Y2 * Pitch) + (Xi * 4);

    *(volatile ULONG*)PixelAddr = Color;
}

VOID
VEAPI
BvFillRect(
    IN ULONG Xi, 
    IN ULONG Y2, 
    IN ULONG W, 
    IN ULONG H, 
    IN ULONG Color
)
{
    for (ULONG y = Y2; y < Y2 + H; y++)
    {
        for (ULONG x = Xi; x < Xi + W; x++)
        {
            BvPutPixel(x, y, Color);
        }
    }
}

VOID
VEAPI
BvDrawRect(
    IN ULONG Xi, 
    IN ULONG Y2, 
    IN ULONG W, 
    IN ULONG H, 
    IN ULONG Color)
{
    for (ULONG x = Xi; x < Xi + W; x++)
    {
        BvPutPixel(x, Y2, Color);         // Top Border
        BvPutPixel(x, Y2 + H - 1, Color); // Bottom Border
    }
    for (ULONG y = Y2; y < Y2 + H; y++)
    {
        BvPutPixel(Xi, y, Color);         // Left Border
        BvPutPixel(Xi + W - 1, y, Color); // Right Border
    }
}

// Font & Text Engine
VOID
VEAPI
BvDrawChar(IN ULONG Xi, IN ULONG Y2, IN CHAR C, IN ULONG Color)
{
    UCHAR CharIndex = (UCHAR)C;

    for (ULONG row = 0; row < 16; row++)
    {
        UCHAR Bits = Font8x16[CharIndex][row];
        for (ULONG col = 0; col < 8; col++)
        {
            if (Bits & (0x80 >> col))
            {
                BvPutPixel(Xi + col, Y2 + row, Color);
            }
        }
    }
}

VOID
VEAPI
BvDrawString(
    IN ULONG Xi, 
    IN ULONG Y2, 
    IN const char *Text, 
    IN ULONG Color
)
{
    ULONG CurrentX = Xi;
    while (*Text)
    {
        BvDrawChar(CurrentX, Y2, *Text, Color);
        CurrentX += 8;
        Text++;
    }
}

/* Layout engine */

VOID
VEAPI
BvRenderLayout(VOID)
{
    if (!BvInfoBlock) return;

    // 1. Clear Screen ke Hitam Pekat (Windows BOOTMGR Style)
    BvFillRect(0, 0, BvInfoBlock->W, BvInfoBlock->H, COLOR_BG);

    // Lebar Banner BOOTMGR (Bisa dibuat sedikit berjarak dari pinggir layar)
    ULONG BannerMarginX = 100; 
    ULONG BannerWidth   = BvInfoBlock->W - (BannerMarginX * 2);

    // 2. Header Banner ("VeaOS Boot Manager")
    ULONG HeaderY = 25;
    ULONG HeaderH = 24;
    BvFillRect(BannerMarginX, HeaderY, BannerWidth, HeaderH, COLOR_BANNER_BG);

    const char *Title = "VeaOS Boot Manager";
    ULONG TitleX = (BvInfoBlock->W - (cstrlen(Title) * 8)) / 2;
    ULONG TitleY = HeaderY + (HeaderH / 2) - 4;
    BvDrawString(TitleX, TitleY, Title, COLOR_BANNER_TEXT);

    // 3. Footer Banner
    ULONG FooterH = 24;
    ULONG FooterY = BvInfoBlock->H - 45;
    BvFillRect(BannerMarginX, FooterY, BannerWidth, FooterH, COLOR_BANNER_BG);

    // 4. Set Scroll Region di Tengah Area Hitam
    ULONG LogMarginX   = BannerMarginX + 10;
    ULONG LogMarginTop = HeaderY + HeaderH + 30;
    ULONG LogMarginBtm = FooterY - 20;

    BvSetScrollRegion(LogMarginX, LogMarginTop, BvInfoBlock->W - LogMarginX, LogMarginBtm);

    BvFooterHeaderState = TRUE;
}

VOID
VEAPI
BvDrawFooterKeys(VOID)
{
    if (!BvInfoBlock) return;

    ULONG BannerMarginX = 100;
    ULONG BannerWidth   = BvInfoBlock->W - (BannerMarginX * 2);
    ULONG FooterH       = 24;
    ULONG FooterY       = BvInfoBlock->H - 45;
    ULONG PaddingX      = 20; // Padding dari pinggir banner silver

    // 1. Bersihkan / Gambar ulang background banner silver di footer
    BvFillRect(BannerMarginX, FooterY, BannerWidth, FooterH, COLOR_BANNER_BG);

    if (BvFooterKeyCount == 0) return;

    ULONG FooterTextY = FooterY + 4; // Vertikal rata tengah

    // =========================================================================
    // POSISI DENGAN JANGKAR PINTAR (LEFT, CENTER, RIGHT)
    // =========================================================================

    // KASUS 1: Hanya 1 Tombol -> Taruh Tepat di Tengah Banner
    if (BvFooterKeyCount == 1)
    {
        if (BvFooterKeys[0].Text)
        {
            ULONG TextLenPx = cstrlen(BvFooterKeys[0].Text) * 8;
            ULONG TextX = BannerMarginX + (BannerWidth - TextLenPx) / 2;
            BvDrawString(TextX, FooterTextY, BvFooterKeys[0].Text, COLOR_BANNER_TEXT);
        }
    }
    // KASUS 2: Ada 2 Tombol -> 1 di Ujung Kiri, 1 di Ujung Kanan
    else if (BvFooterKeyCount == 2)
    {
        // Tombol 0: Rata Kiri
        if (BvFooterKeys[0].Text)
        {
            ULONG TextX = BannerMarginX + PaddingX;
            BvDrawString(TextX, FooterTextY, BvFooterKeys[0].Text, COLOR_BANNER_TEXT);
        }
        // Tombol 1: Rata Kanan
        if (BvFooterKeys[1].Text)
        {
            ULONG TextLenPx = cstrlen(BvFooterKeys[1].Text) * 8;
            ULONG TextX = BannerMarginX + BannerWidth - PaddingX - TextLenPx;
            BvDrawString(TextX, FooterTextY, BvFooterKeys[1].Text, COLOR_BANNER_TEXT);
        }
    }
    // KASUS 3: Ada 3 Tombol -> Kiri, Tengah, Kanan (Windows BOOTMGR Style!)
    else if (BvFooterKeyCount == 3)
    {
        // Tombol 0: Rata Kiri
        if (BvFooterKeys[0].Text)
        {
            ULONG TextX = BannerMarginX + PaddingX;
            BvDrawString(TextX, FooterTextY, BvFooterKeys[0].Text, COLOR_BANNER_TEXT);
        }
        // Tombol 1: Presisi di Tengah
        if (BvFooterKeys[1].Text)
        {
            ULONG TextLenPx = cstrlen(BvFooterKeys[1].Text) * 8;
            ULONG TextX = BannerMarginX + (BannerWidth - TextLenPx) / 2;
            BvDrawString(TextX, FooterTextY, BvFooterKeys[1].Text, COLOR_BANNER_TEXT);
        }
        // Tombol 2: Rata Kanan
        if (BvFooterKeys[2].Text)
        {
            ULONG TextLenPx = cstrlen(BvFooterKeys[2].Text) * 8;
            ULONG TextX = BannerMarginX + BannerWidth - PaddingX - TextLenPx;
            BvDrawString(TextX, FooterTextY, BvFooterKeys[2].Text, COLOR_BANNER_TEXT);
        }
    }
    // KASUS 4: Jika 4 atau 5 Tombol -> Bagi Rata Grid (Column Mode)
    else
    {
        ULONG SectionWidth = BannerWidth / BvFooterKeyCount;
        for (ULONG i = 0; i < BvFooterKeyCount; i++)
        {
            const char *Text = BvFooterKeys[i].Text;
            if (!Text) continue;

            ULONG TextLenPx = cstrlen(Text) * 8;
            ULONG TextX = BannerMarginX + (i * SectionWidth) + ((SectionWidth - TextLenPx) / 2);

            BvDrawString(TextX, FooterTextY, Text, COLOR_BANNER_TEXT);
        }
    }
}

VOID
VEAPI
BvDrawListItem(
    IN PBV_LIST_OPTION List,
    IN ULONG Index,
    IN BOOLEAN IsSelected
)
{
    if(!List || Index >= List->ItemCount) return;

    // Lets choose our color
    ULONG ItemY = List->Y + (Index * List->ItemHeight);
    ULONG BgColor = IsSelected ? COLOR_LIST_SEL_BG : COLOR_LIST_BG;
    ULONG TextColor = IsSelected ? COLOR_LIST_SEL_TEXT : COLOR_LIST_TEXT;

    // Draw background block
    BvFillRect(List->X, ItemY, List->Width, List->ItemHeight, BgColor);

    // Draw our text now
    if(List->Items[Index])
    {
        BvDrawString(List->X + 10, ItemY + 4, List->Items[Index], TextColor);
    }

    if(IsSelected)
    {
        ULONG PaddingRight = 20; // Jarak dari batas kanan box (bisa disesuaikan)
        ULONG ArrowX = List->X + List->Width - PaddingRight;

        BvDrawString(ArrowX, ItemY + 4, ">", TextColor);
    }
}

/* Scrolling Logic */
VOID
VEAPI
BvScrollConsole(VOID)
{
    if (!BvInfoBlock || !BvInfoBlock->VideoBootAddress) return;

    ULONG Pitch = (ULONG_PTR)BvInfoBlock->AdditionalInformation[0];
    ULONG RegionWidthBytes = (BvScrollRight - BvScrollLeft) * 4; // 32 bpp = 4 bytes/pixel

    //
    // 1. Copy baris piksel dari Bawah ke Atas HANYA di dalam Scroll Region
    //
    for (ULONG y = BvScrollTop; y < BvScrollBottom - LOG_LINE_HEIGHT; y++)
    {
        ULONG_PTR DstAddr = (ULONG_PTR)BvInfoBlock->VideoBootAddress + (y * Pitch) + (BvScrollLeft * 4);
        ULONG_PTR SrcAddr = (ULONG_PTR)BvInfoBlock->VideoBootAddress + ((y + LOG_LINE_HEIGHT) * Pitch) + (BvScrollLeft * 4);

        cmemcpy((PVOID)DstAddr, (PVOID)SrcAddr, RegionWidthBytes);
    }

    //
    // 2. Bersihkan baris paling bawah Scroll Region saja (Isi Warna Background Console)
    //
    BvFillRect(
        BvScrollLeft, 
        BvScrollBottom - LOG_LINE_HEIGHT, 
        BvScrollRight - BvScrollLeft, 
        LOG_LINE_HEIGHT, 
        COLOR_BG
    );
}

VOID
VEAPI
BvSetScrollRegion(
    IN ULONG Left,
    IN ULONG Top,
    IN ULONG Right,
    IN ULONG Bottom
)
{
    if (!BvInfoBlock) return;

    // Boundary Protection (Mencegah out-of-bounds)
    if (Right > BvInfoBlock->W) Right = BvInfoBlock->W;
    if (Bottom > BvInfoBlock->H) Bottom = BvInfoBlock->H;

    BvScrollLeft   = Left;
    BvScrollTop    = Top;
    BvScrollRight  = Right;
    BvScrollBottom = Bottom;

    // Setel kursor log awal tepat di pojok kiri-atas Scroll Region
    BvLogCursorX = Left;
    BvLogCursorY = Top;
}

VOID
VEAPI
BvSetReservedRows(
    IN ULONG TopReservedPixels,
    IN ULONG BottomReservedPixels)
{
    if (!BvInfoBlock) return;

    ULONG LeftMargin  = 35; // Default margin kiri
    ULONG RightMargin = BvInfoBlock->W - 35;

    ULONG Top    = TopReservedPixels;
    ULONG Bottom = BvInfoBlock->H - BottomReservedPixels;

    BvSetScrollRegion(LeftMargin, Top, RightMargin, Bottom);
}

/* PUBLIC **********************************************************************/

BOOLEAN
VEAPI
BvInitScreen(VOID)
{
    if(!BvExtractVbeInfo())
    {
        LdrError(STATUS_VBE_ERROR);
    }

    // We are not in BIOS Text Mode
    InTextMode = FALSE;
    
    // Render tampilan dasar UI
    BvRenderLayout();

    return TRUE;
}

VOID
VEAPI
BvPrintLog(const char *Text)
{
    if (!BvInfoBlock) return;

    while (*Text)
    {
        // Handle Karakter Newline '\n'
        if (*Text == '\n')
        {
            BvLogCursorX = BvScrollLeft;
            BvLogCursorY += LOG_LINE_HEIGHT;
            Text++;
            continue;
        }

        // Jika Kursor Mencapai Batas Kanan Scroll Region -> Word Wrap
        if (BvLogCursorX + 8 > BvScrollRight)
        {
            BvLogCursorX = BvScrollLeft;
            BvLogCursorY += LOG_LINE_HEIGHT;
        }

        // Jika Kursor Mencapai Batas Bawah Scroll Region (Reserve Row Bawah) -> SCROLL!
        if (BvLogCursorY + LOG_LINE_HEIGHT > BvScrollBottom)
        {
            BvScrollConsole();
            BvLogCursorY = BvScrollBottom - LOG_LINE_HEIGHT;
        }

        // Draw Karakter tunggal
        BvDrawChar(BvLogCursorX, BvLogCursorY, *Text, COLOR_TEXT);
        BvLogCursorX += 8;

        Text++;
    }

    // Cek lagi setelah pindah baris
    if (BvLogCursorY + LOG_LINE_HEIGHT > BvScrollBottom)
    {
        BvScrollConsole();
        BvLogCursorY = BvScrollBottom - LOG_LINE_HEIGHT;
    }
}

VOID
VEAPI
BvClearScreen(VOID)
{
    if (!BvInfoBlock || !BvInfoBlock->VideoBootAddress) return;

    ULONG Pitch = (ULONG_PTR)BvInfoBlock->AdditionalInformation[0];
    ULONG Height = BvInfoBlock->H;
    ULONG Width = BvInfoBlock->W;

    // Tulis warna langsung ke memori Framebuffer agar jauh lebih cepat
    for (ULONG y = 0; y < Height; y++)
    {
        // Hitung alamat awal untuk baris ke-y
        ULONG_PTR RowAddr = (ULONG_PTR)BvInfoBlock->VideoBootAddress + (y * Pitch);
        ULONG *Pixel = (ULONG *)RowAddr;
        
        for (ULONG x = 0; x < Width; x++)
        {
            Pixel[x] = COLOR_BG;
        }
    }

    /* Reset log cursor state after clearing the framebuffer, keep scroll region intact. */
    BvResetLogCursorState();

    /* Set our Display FooterHeader to zero */
    BvFooterHeaderState = FALSE;
}

VOID
VEAPI
BvSetFooterKeyCombination(
    IN PVEA_FOOTER_KEY Keys,
    IN ULONG Count
)
{
    if(Count > MAX_FOOTER_KEYS)
    {
        return;
    }

    BvFooterKeyCount = Count;

    for (ULONG i = 0; i < Count; i++)
    {
        BvFooterKeys[i].Text = Keys[i].Text;
    }

    BvDrawFooterKeys();
}

PBV_LIST_OPTION
VEAPI
BvCreateListOption(
    IN const PCHAR *Items,
    IN ULONG Count,
    IN ULONG Width
)
{
    // If count was more than setted MAX LIST, set it to MAX
    if (Count > MAX_LIST_OPTIONS) Count = MAX_LIST_OPTIONS;

    // Init our item
    BvCurrentList.ItemCount = Count;
    BvCurrentList.X = BvLogCursorX;
    BvCurrentList.Y = BvLogCursorY;
    BvCurrentList.Width = Width;
    BvCurrentList.ItemHeight = 24; // 16px for font + 8px for pad

    // Hitung total ukuran agar kita bisa otomatis turun
    ULONG TotalLastHeight = Count * BvCurrentList.ItemHeight;

    // Disini kita cek scrolling
    while(BvCurrentList.Y + TotalLastHeight > BvScrollBottom)
    {
        BvScrollConsole();
        BvCurrentList.Y -= LOG_LINE_HEIGHT;
    }

    BvLogCursorY = BvCurrentList.Y;

    // Save our item list to global list variable
    for(ULONG i = 0; i < Count; i++)
    {
        BvCurrentList.Items[i] = Items[i];
    }

    // Draw all first
    for(ULONG i = 0; i < Count; i++)
    {
        BvDrawListItem(&BvCurrentList, i, FALSE);
    }
    
    // Dorong log cursor sehabis kita gambar
    BvLogCursorY += TotalLastHeight + LIST_BOTTOM_MARGIN;
    BvLogCursorX = BvScrollLeft;

    return &BvCurrentList;
}

VOID
VEAPI
BvUpListOption(
    IN PBV_LIST_OPTION List,
    IN ULONG OldIndex,
    IN ULONG NewIndex
)
{
    if (!List) return;
    
    // Hapus highlight dari index sebelumnya
    BvDrawListItem(List, OldIndex, FALSE);
    
    // Gambar highlight di index yang baru (naik)
    BvDrawListItem(List, NewIndex, TRUE);
}

VOID
VEAPI
BvDownListOption(
    IN PBV_LIST_OPTION List,
    IN ULONG OldIndex,
    IN ULONG NewIndex
)
{
    if (!List) return;
    
    // Secara visual (rendering), Up dan Down punya tugas yang identik bagi si "buta" ini,
    // yaitu sekadar menukar warna dua baris.
    // Tapi memisahkannya secara nama fungsi akan sangat bagus untuk kerapian kode logikamu.
    
    BvDrawListItem(List, OldIndex, FALSE);
    BvDrawListItem(List, NewIndex, TRUE);
}

BOOLEAN
VEAPI
BvpIsBetterMode(
    VBE_MODE_INFO *Current, 
    VBE_MODE_INFO *Best
)
{
    // Syarat wajib: Harus mendukung Linear Framebuffer (bit 7)
    if (!(Current->ModeAttributes & 0x80)) return FALSE;
    
    // Syarat wajib: Harus 32 Bits Per Pixel (bisa ganti 24 kalau butuh fallback)
    if (Current->BitsPerPixel != 32) return FALSE;

    // Buang resolusi yang "Kelebaran"
    if (Current->XResolution > MAX_TARGET_X || Current->YResolution > MAX_TARGET_Y)
    {
        return FALSE;
    }

    // Jika kita belum punya kandidat 'Best', maka ini otomatis yang terbaik
    if (Best->XResolution == 0) return TRUE;

    // Bandingkan luas resolusi (X * Y)
    ULONG CurrentPixels = Current->XResolution * Current->YResolution;
    ULONG BestPixels = Best->XResolution * Best->YResolution;

    return (CurrentPixels > BestPixels);
}

BOOLEAN
VEAPI
BvpFindBestVbeMode(
    PUSHORT Modes, 
    PUSHORT OutBestMode, 
    VBE_MODE_INFO *OutBestInfo
)
{
    ULONG MODE_INFO_ADDR = VBE_BUFFER_ADDR + 0x200;
    USHORT ModeEsSeg = (MODE_INFO_ADDR >> 4) & 0xFFFF;
    USHORT ModeDiOff = MODE_INFO_ADDR & 0x000F;
    VBE_MODE_INFO *ModeInfo = (VBE_MODE_INFO*)MODE_INFO_ADDR;
    
    // Reset output
    *OutBestMode = 0;
    cmemset(OutBestInfo, 0, sizeof(VBE_MODE_INFO));

    INT i = 0;
    
    // Level 1: Loop semua mode
    while (Modes[i] != 0xFFFF)
    {
        USHORT Mode = Modes[i];
        ULONG Status = _INT10_CALL_BUFFER(0x4F01, 0, Mode, ModeEsSeg, ModeDiOff);

        // Level 2: Cek jika pemanggilan sukses
        if ((Status & 0xFFFF) == 0x004F)
        {
            // Level 3: Bandingkan mana yang lebih baik
            if (BvpIsBetterMode(ModeInfo, OutBestInfo))
            {
                *OutBestMode = Mode;
                
                // Copy struct info dari buffer memori sementara ke OutBestInfo
                cmemcpy(OutBestInfo, ModeInfo, sizeof(VBE_MODE_INFO));
            }
        }
        i++;
    }

    // Berhasil jika kita setidaknya menemukan 1 mode yang valid
    return (*OutBestMode != 0);
}
BOOLEAN
VEAPI
BvExtractVbeInfo(VOID)
{
    USHORT EsSeg = (VBE_BUFFER_ADDR >> 4) & 0xFFFF;
    USHORT DiOff = VBE_BUFFER_ADDR & 0x000F;
    VBE_INFO_BLOCK *VbeInfo = (VBE_INFO_BLOCK*)VBE_BUFFER_ADDR;

    VbeInfo->VbeSignature[0] = 'V';
    VbeInfo->VbeSignature[1] = 'B';
    VbeInfo->VbeSignature[2] = 'E';
    VbeInfo->VbeSignature[3] = '2';

    ULONG Status = _INT10_CALL_BUFFER(0x4F00, 0, 0, EsSeg, DiOff);
    if ((Status & 0xFFFF) != 0x004F)
    {
        LdrError(STATUS_VBE_ERROR);
        while(1) asm("hlt");
    }

    ULONG ModeListPtr = ((VbeInfo->VideoModePtr >> 16) * 16) + (VbeInfo->VideoModePtr & 0xFFFF);
    PUSHORT Modes = (PUSHORT)ModeListPtr;
    
    USHORT BestMode = 0;
    VBE_MODE_INFO BestModeInfo;

    // Cari mode terbaik via helper
    if (!BvpFindBestVbeMode(Modes, &BestMode, &BestModeInfo))
    {
        // Gagal menemukan satupun mode 32-bpp dengan LFB
        return FALSE; 
    }

    // --- FASE SETUP & ALOKASI ---
    
    ULONG FbSize = BestModeInfo.XResolution * BestModeInfo.YResolution * (BestModeInfo.BitsPerPixel / 8);
    ULONG FbPhys = BestModeInfo.PhysBasePtr;

    ULONG_PTR FbVirt = (ULONG_PTR)LmMapIoSpace(FbPhys, FbSize);
    if (!FbVirt)
    {
        LdrError(STATUS_LOADER_MEMORY_FAILURE);
        while(1) asm("hlt");
    }

    BvInfoBlock = (PVIDEO_BOOT)LmAllocatePool(LdrReclaimablePool, sizeof(VIDEO_BOOT));
    if (!BvInfoBlock)
    {
        LdrError(STATUS_INSUFFICIENT_RESOURCES);
        while(1) asm("hlt");
    }

    BvInfoBlock->VideoBootAddress = (PVOID)FbVirt;
    BvInfoBlock->X = 0;
    BvInfoBlock->Y = 0;
    BvInfoBlock->W = BestModeInfo.XResolution;
    BvInfoBlock->H = BestModeInfo.YResolution;
    BvInfoBlock->AdditionalInformation[0] = (PVOID)(ULONG_PTR)BestModeInfo.BytesPerScanLine;

    // Switch ke VBE mode dengan mode terbaik (Bit 14 = 0x4000 untuk LFB)
    _INT10_CALL_BUFFER(0x4F02, BestMode | 0x4000, 0, 0, 0);

    // FIX: Bersihkan Framebuffer dengan warna hitam agar sisa LogUI tidak "kebawa"
    cmemset((PVOID)FbVirt, 0, FbSize);

    vbe_active = TRUE;
    return TRUE;
}

VOID
VEAPI
BvRerenderLayout(VOID)
{
    BvClearScreen();
    BvRenderLayout();
}

VOID
VEAPI
BvErrorLog(
    IN PCHAR ErrorCode,
    IN PCHAR ErrorFile
)
{
    BvRerenderLayout();

    BvPrintLog("An unexpected error has happened and prevent the OS from booting.\n");
    BvPrintLog("\nPress any key to restart.\n\n\n");

    BvPrintLog("Status: "); BvPrintLog(ErrorCode); BvPrintLog("\n\n");
    BvPrintLog("Caused by: "); 
    if (LdrLastCheckedFile[0] != '\0')
    {
        BvPrintLog(ErrorFile); BvPrintLog("\n\n");
    }
    else
    {
        BvPrintLog("Internal System.\n");
    }
}

VOID
VEAPI
BvMoveVideoInfoToBootBlock(IN OUT BLOCK_BOOT_2 *BlockBoot2)
{
    if(!BvInfoBlock || !BlockBoot2) return;

    // Just reference it instantly
    cmemcpy(&BlockBoot2->VideoBoot, BvInfoBlock, sizeof(VIDEO_BOOT));
}