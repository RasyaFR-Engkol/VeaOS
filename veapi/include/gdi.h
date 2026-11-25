#pragma once

#define TRANSPARENT 1
#define OPAQUE      2

typedef struct _VEAGDI_SURFACE {
    ULONG   Width;              /* Lebar dalam pixel (misal: 800) */
    ULONG   Height;             /* Tinggi dalam pixel (misal: 600) */
    ULONG   Pitch;              /* Bytes per scanline (misal: 3200) */
    ULONG   BitsPerPixel;       /* 32 bpp */
    ULONG   BytesPerPixel;      /* 4 byte (BitsPerPixel / 8) */
    PVOID   Scan0;              /* Pointer ke baris pixel pertama (LFB / RAM buffer) */
    SIZE_T  BufferSize;         /* Total ukuran buffer dalam byte */
    BOOLEAN IsPrimary;          /* TRUE kalau nunjuk langsung ke VRAM/LFB */

    /* Mask warna dari VIDEO_MODE_INFORMATION */
    ULONG   RedMask;
    ULONG   GreenMask;
    ULONG   BlueMask;
} VEAGDI_SURFACE, *PVEAGDI_SURFACE;

typedef struct _VEAPI_DISPLAY_DEVICE {
    BOOLEAN                  IsInitialized;
    BOOLEAN                  ScreenEnabled;

    /* Koneksi Object Manager ke /Device/Display0 */
    PFILE_OBJECT             DisplayFileObject;
    PDEVICE_OBJECT           DisplayDeviceObject;

    /* Data mentah hasil query dari vbedisplay via graphicclass */
    VIDEO_MODE_INFORMATION   ModeInfo;
    VIDEO_MEMORY_INFORMATION MemoryInfo;

    /* Surface aktif untuk rendering GDI */
    VEAGDI_SURFACE           PrimarySurface;
    PVEAGDI_SURFACE          BackBuffer;        /* Opsional: buat double-buffering nanti */

    /* State desktop & kursor dasar */
    LONG                     CursorX;
    LONG                     CursorY;
    BOOLEAN                  CursorVisible;
} VEAPI_DISPLAY_DEVICE, *PVEAPI_DISPLAY_DEVICE;
extern VEAPI_DISPLAY_DEVICE GpPrimaryDisplay;

#define GDI_DC_MAGIC  0x43444947  /* 'GIDC' */

typedef struct _GDI_DC_OBJECT {
    ULONG       Magic;
    LIST_ENTRY  ListEntry;
    BOOLEAN Used;
    HDC     hDC;
    HWND    hWnd;
    LONG    OriginX;
    LONG    OriginY;
    LONG    ClipLeft;
    LONG    ClipTop;
    LONG    ClipRight;
    LONG    ClipBottom;
    ULONG   TextColor;
    ULONG   BkColor;
    INT     BkMode;         /* TRANSPARENT atau OPAQUE */
    LONG    PenX;           /* Posisi pena aktif untuk MoveToEx / LineTo */
    LONG    PenY;
    HFONT   hFont;
    HBRUSH  hBrush;
    HPEN    hPen;
} GDI_DC_OBJECT, *PGDI_DC_OBJECT;

#define WND_BORDER_WIDTH    3
#define WND_TITLEBAR_HEIGHT 28
#define WND_CORNER_RADIUS   6
#define WND_SHADOW_HEIGHT   6
#define WND_SHADOW_OFFSET_X 2

HDC  VEAPI GdiCreateWindowDC(IN HWND hWnd, IN LONG OriginX, IN LONG OriginY, IN ULONG Width, IN ULONG Height);
BOOLEAN VEAPI GdiReleaseWindowDC(IN HDC hdc);

BOOLEAN VEAPI VeaGdiFillRect(IN HDC hdc, IN CONST PRECT lprc, IN ULONG ColorARGB);
BOOLEAN VEAPI VeaGdiFillRectInternal(IN HDC hdc, IN CONST RECT *Rect, IN ULONG ColorARGB);
ULONG   VEAPI VeaGdiSetPixel(IN HDC hdc, IN INT X, IN INT Y, IN ULONG ColorARGB);
ULONG   VEAPI VeaGdiGetPixel(IN HDC hdc, IN INT X, IN INT Y);
BOOLEAN VEAPI VeaGdiLineTo(IN HDC hdc, IN INT X1, IN INT Y1, IN INT X2, IN INT Y2, IN ULONG ColorARGB);
BOOLEAN VEAPI VeaGdiInvertRect(IN HDC hdc, IN CONST PRECT lprc);
BOOLEAN VEAPI VeaGdiGradientFill(IN HDC hdc, IN CONST PRECT lprc, IN ULONG ColorStartARGB, IN ULONG ColorEndARGB, IN BOOLEAN Vertical);
BOOLEAN VEAPI VeaGdiBitBlt(IN HDC hdc, IN INT DstX, IN INT DstY, IN ULONG Width, IN ULONG Height, IN CONST PULONG Pixels, IN ULONG SourceWidth, IN ULONG SourceHeight, IN INT SrcX, IN INT SrcY);
BOOLEAN VEAPI VeaGdiTransparentBlt(IN HDC hdc, IN INT DstX, IN INT DstY, IN ULONG Width, IN ULONG Height, IN CONST PULONG Pixels, IN ULONG SourceWidth, IN ULONG SourceHeight, IN INT SrcX, IN INT SrcY, IN ULONG TransparentColorRGB);
BOOLEAN VEAPI VeaGdiAlphaBlend(IN HDC hdc, IN INT DstX, IN INT DstY, IN ULONG Width, IN ULONG Height, IN CONST PULONG Pixels, IN ULONG SourceWidth, IN ULONG SourceHeight, IN INT SrcX, IN INT SrcY, IN UCHAR Alpha);
BOOLEAN VEAPI VeaGdiDrawGlyph(IN HDC hdc, IN INT X, IN INT Y, IN CONST UCHAR GlyphBitmap[16], IN ULONG TextColorARGB, IN ULONG BkColorARGB, IN BOOLEAN TransparentBk);

VOID
VEAPI
EngFillRect32(
    IN PVEAGDI_SURFACE Surface,
    IN LONG X,
    IN LONG Y,
    IN ULONG Width,
    IN ULONG Height,
    IN ULONG ColorARGB
);

VOID
VEAPI
EngSetPixel32(
    IN PVEAGDI_SURFACE Surface,
    IN LONG X,
    IN LONG Y,
    IN ULONG ColorARGB
);

ULONG
VEAPI
EngGetPixel32(
    IN PVEAGDI_SURFACE Surface,
    IN LONG X,
    IN LONG Y
);

VOID
VEAPI
EngLineTo32(
    IN PVEAGDI_SURFACE Surface,
    IN LONG X1,
    IN LONG Y1,
    IN LONG X2,
    IN LONG Y2,
    IN ULONG ColorARGB
);

VOID
VEAPI
EngInvertRect32(
    IN PVEAGDI_SURFACE Surface,
    IN LONG X,
    IN LONG Y,
    IN ULONG Width,
    IN ULONG Height
);

VOID
VEAPI
EngGradientFill32(
    IN PVEAGDI_SURFACE Surface,
    IN LONG X,
    IN LONG Y,
    IN ULONG Width,
    IN ULONG Height,
    IN ULONG ColorStartARGB,
    IN ULONG ColorEndARGB,
    IN BOOLEAN Vertical
);

VOID
VEAPI
EngBitBlt32(
    IN PVEAGDI_SURFACE DstSurface,
    IN LONG DstX,
    IN LONG DstY,
    IN ULONG Width,
    IN ULONG Height,
    IN PVEAGDI_SURFACE SrcSurface,
    IN LONG SrcX,
    IN LONG SrcY
);

VOID
VEAPI
EngTransparentBlt32(
    IN PVEAGDI_SURFACE DstSurface,
    IN LONG DstX,
    IN LONG DstY,
    IN ULONG Width,
    IN ULONG Height,
    IN PVEAGDI_SURFACE SrcSurface,
    IN LONG SrcX,
    IN LONG SrcY,
    IN ULONG TransparentColorRGB
);

VOID
VEAPI
EngAlphaBlend32(
    IN PVEAGDI_SURFACE DstSurface,
    IN LONG DstX,
    IN LONG DstY,
    IN ULONG Width,
    IN ULONG Height,
    IN PVEAGDI_SURFACE SrcSurface,
    IN LONG SrcX,
    IN LONG SrcY,
    IN UCHAR ConstantAlpha
);

VOID
VEAPI
EngDrawGlyph8x16(
    IN PVEAGDI_SURFACE Surface,
    IN LONG X,
    IN LONG Y,
    IN CONST UCHAR GlyphBitmap[16],
    IN ULONG TextColorARGB,
    IN ULONG BkColorARGB,
    IN BOOLEAN TransparentBk
);

BOOLEAN
VEAPI
VeaGdiDrawImage(
    IN HDC          hdc,
    IN INT          DstX,
    IN INT          DstY,
    IN ULONG        Width,
    IN ULONG        Height,
    IN CONST PULONG Pixels,
    IN UCHAR        Alpha
);

VOID
VEAPI
EngDrawGlyphAlpha8(
    IN PVEAGDI_SURFACE Surface,
    IN LONG X,
    IN LONG Y,
    IN ULONG Width,
    IN ULONG Height,
    IN LONG Pitch,
    IN CONST UCHAR *AlphaBitmap,
    IN ULONG TextColorARGB
);

VEASTATUS
VEAPI
GdiInitializeFontEngine(VOID);

PGDI_DC_OBJECT
VEAPI
VeaGdiGetDc(
    IN HDC hdc
);

BOOLEAN
VeaGdiCreateSurfaceView(
    IN PGDI_DC_OBJECT Dc,
    OUT PVEAGDI_SURFACE View,
    OUT PLONG ViewOriginX,
    OUT PLONG ViewOriginY
);

BOOLEAN
VEAPI
VeaGdiDrawText(
    IN HDC hdc,
    IN INT X,
    IN INT Y,
    IN CONST CHAR *Text,
    IN ULONG ColorARGB
);

BOOLEAN
VEAPI
VeaGdiGetTextExtent(
    IN CONST CHAR *Text,
    OUT PULONG OutWidth,
    OUT PULONG OutHeight
);

#define GDI_FACE_MAGIC  0x45434146
#define GDI_FONT_MAGIC  0x544E4647

#define FONT_POOL_ALLOC(Size)   UlAllocatePoolWithTag(NonPagedPool, (Size), 'ft22')
#define FONT_POOL_FREE(Ptr)     UlFreePool((Ptr))

#define FONT_CACHE_FIRST_CHAR   32
#define FONT_CACHE_LAST_CHAR    126
#define FONT_CACHE_COUNT        (FONT_CACHE_LAST_CHAR - FONT_CACHE_FIRST_CHAR + 1)
#define FONT_DEFAULT_PIXEL_SIZE 14

typedef struct FT_FaceRec_*  FT_Face;

typedef struct _GDI_CACHED_GLYPH {
    BOOLEAN Valid;
    ULONG   Width;
    ULONG   Rows;
    LONG    Pitch;
    INT     BitmapLeft;
    INT     BitmapTop;
    INT     AdvanceX;
    PUCHAR  AlphaBuffer;
} GDI_CACHED_GLYPH, *PGDI_CACHED_GLYPH;

typedef struct _GDI_LOADED_FACE {
    ULONG       Magic;
    LIST_ENTRY  ListEntry;
    ULONG       RefCount;
    ULONG       Flags;           
    BOOLEAN     Registered;        
    BOOLEAN     IsMemoryFont;     
    PEPROCESS   OwnerProcess;     
    CHAR        FilePath[256];
    CHAR        FamilyName[64];   
    ULONG       Weight;          
    BOOLEAN     Italic;
    PVOID       FileBuffer;
    ULONG       FileSize;
    FT_Face     Face;
} GDI_LOADED_FACE, *PGDI_LOADED_FACE;

typedef struct _GDI_FONT_OBJECT {
    ULONG            Magic;
    LIST_ENTRY       ListEntry;
    PGDI_LOADED_FACE ParentFace;
    ULONG            PixelHeight;
    INT              Ascender;
    INT              LineHeight;
    GDI_CACHED_GLYPH Glyphs[FONT_CACHE_COUNT];
} GDI_FONT_OBJECT, *PGDI_FONT_OBJECT;

#define DEFAULT_SYSTEM_FONT_PATH "D1P1:/VeaOS/Fonts/VerdanaNow.ttf"

HFONT
VEAPI
VeaGdiCreateFont(
    IN INT         nHeight,
    IN INT         fnWeight,
    IN BOOLEAN     bItalic,
    IN CONST CHAR *lpszFace
);

HFONT
VEAPI
VeaGdiSelectFont(
    IN HDC hdc,
    IN HFONT hFont
);

BOOLEAN
VEAPI
VeaGdiDeleteFont(
    IN HFONT hFont
);

BOOLEAN
VEAPI
VeaGdiGetTextExtentFont(
    IN HDC hdc,
    IN CONST CHAR *Text,
    OUT PULONG OutWidth,
    OUT PULONG OutHeight
);

#define GDI_POOL_ALLOC(_Sizeof) UlAllocatePoolWithTag(PagedPool, _Sizeof, 'GIDC')
#define GDI_POOL_FREE(_Pointer) UlFreePoolWithTag(_Pointer, 'GIDC')

VOID
GdipInitializeDcManager(VOID);

#define MAX_CURSOR_SIZE 64

typedef struct _GDI_CURSOR_STATE {
    BOOLEAN HasShape;
    BOOLEAN DrawnOnScreen;
    INT     ShowCount;      /* Standar Win32: tampil jika ShowCount >= 0 */
    LONG    DrawnX;
    LONG    DrawnY;
    LONG    HotX;
    LONG    HotY;
    ULONG   Width;
    ULONG   Height;
    ULONG   Pixels[MAX_CURSOR_SIZE * MAX_CURSOR_SIZE];
    ULONG   SaveUnder[MAX_CURSOR_SIZE * MAX_CURSOR_SIZE];
} GDI_CURSOR_STATE;

VOID
VEAPI
EngMovePointer(
    IN LONG NewX,
    IN LONG NewY
);

VOID
VEAPI
EngShowPointer(VOID);

VOID
VEAPI
EngHidePointer(VOID);

typedef struct tagPOINT
{
    LONG x;
    LONG y;
} POINT, *PPOINT, *LPPOINT;

HDC
VEAPI
VeaUserGetDCEx(
    IN HWND   hWnd,
    IN HANDLE hrgnClip,
    IN ULONG  Flags
);

BOOLEAN
VEAPI
VeaGdiDeleteDC(
    IN HDC hdc
);

typedef enum _VEA_GDI_BLIT_KIND
{
    VeaGdiBlitCopy,
    VeaGdiBlitTransparent,
    VeaGdiBlitAlpha
} VEA_GDI_BLIT_KIND;

static FORCEINLINE PVEAGDI_SURFACE
EngGetTargetSurface(VOID)
{
    return (GpPrimaryDisplay.BackBuffer != NULL)
           ? GpPrimaryDisplay.BackBuffer
           : &GpPrimaryDisplay.PrimarySurface;
}

VOID
VEAPI
EngPresentRect(
    IN LONG X,
    IN LONG Y,
    IN ULONG Width,
    IN ULONG Height
);

ULONG
VeaGdiCreateSurfaceViews(
    IN  PGDI_DC_OBJECT  Dc,
    OUT PVEAGDI_SURFACE Views,
    OUT PLONG           ViewOriginsX,
    OUT PLONG           ViewOriginsY,
    IN  ULONG           MaxViews
);

FORCEINLINE ULONG
WndBlendShadowPixel(
    IN ULONG BasePixel,
    IN UCHAR Alpha
)
{
    ULONG Inv = 255 - (ULONG)Alpha;
    ULONG R = (((BasePixel >> 16) & 0xFF) * Inv + 127) / 255;
    ULONG G = (((BasePixel >> 8)  & 0xFF) * Inv + 127) / 255;
    ULONG B = ((BasePixel         & 0xFF) * Inv + 127) / 255;
    return 0xFF000000 | (R << 16) | (G << 8) | B;
}

#define MAX_ENUM_WINDOWS    4096

#define WND_CAPBTN_SIZE     21
#define WND_CAPBTN_GAP      3
#define WND_CAPBTN_MARGIN_R 6
#define WND_CAPBTN_TOP      4

#define GDI_BRUSH_MAGIC         0x48524247  /* 'GBRH' */
#define GDI_PEN_MAGIC           0x4E455047  /* 'GPEN' */

typedef struct _GDI_BRUSH_OBJECT
{
    ULONG   Magic;
    ULONG   Style;          /* BS_SOLID, BS_NULL */
    ULONG   ColorARGB;
    BOOLEAN StockObject;
} GDI_BRUSH_OBJECT, *PGDI_BRUSH_OBJECT;

typedef struct _GDI_PEN_OBJECT
{
    ULONG   Magic;
    ULONG   Style;          /* PS_SOLID, PS_NULL */
    ULONG   Width;
    ULONG   ColorARGB;
    BOOLEAN StockObject;
} GDI_PEN_OBJECT, *PGDI_PEN_OBJECT;

FORCEINLINE ULONG
GdipNormalizeArgb(
    IN ULONG Color
)
{
    /* Convert COLORREF 0x00BBGGRR to opaque ARGB for GDI surfaces. */
    if ((Color & 0xFF000000) == 0)
    {
        ULONG Red   = Color & 0xFF;
        ULONG Green = (Color >> 8) & 0xFF;
        ULONG Blue  = (Color >> 16) & 0xFF;
        return 0xFF000000 | (Red << 16) | (Green << 8) | Blue;
    }
    return Color;
}

HPEN
VEAPI
VeaGdiCreatePen(
    IN INT   fnPenStyle,
    IN INT   nWidth,
    IN ULONG crColor
);

HBRUSH
VEAPI
VeaGdiCreateSolidBrush(
    IN ULONG crColor
);

HGDIOBJ
VEAPI
VeaGdiGetStockObject(
    IN INT fnObject
);

VOID
VEAPI
GdipInitializeGdiObjects(VOID);

BOOLEAN
VEAPI
GdipResolvePenInfo(
    IN  HPEN   hpen,
    OUT PULONG OutColorARGB,
    OUT PULONG OutWidth
);

BOOLEAN
VEAPI
GdipResolveBrushColor(
    IN  HBRUSH hbr,
    OUT PULONG OutColorARGB
);


HBRUSH
VEAPI
VeaGdiCreateSolidBrush(
    IN ULONG crColor
);

HPEN
VEAPI
VeaGdiCreatePen(
    IN INT   fnPenStyle,
    IN INT   nWidth,
    IN ULONG crColor
);

HGDIOBJ
VEAPI
VeaGdiGetStockObject(
    IN INT fnObject
);

HGDIOBJ
VEAPI
VeaGdiSelectObject(
    IN HDC     hdc,
    IN HGDIOBJ hgdiobj
);

BOOLEAN
VEAPI
VeaGdiDeleteObject(
    IN HGDIOBJ hObject
);

BOOLEAN
VEAPI
VeaGdiMoveToEx(
    IN  HDC     hdc,
    IN  INT     X,
    IN  INT     Y,
    OUT LPPOINT lpPoint OPTIONAL
);

BOOLEAN
VEAPI
VeaGdiLineToCurrent(
    IN HDC hdc,
    IN INT nXEnd,
    IN INT nYEnd
);

BOOLEAN
VEAPI
VeaGdiRectangle(
    IN HDC hdc,
    IN INT nLeftRect,
    IN INT nTopRect,
    IN INT nRightRect,
    IN INT nBottomRect
);

BOOLEAN
VEAPI
VeaGdiFillRectBrush(
    IN HDC         hdc,
    IN CONST PRECT lprc,
    IN HBRUSH      hbr
);

BOOLEAN
VEAPI
VeaGdiFrameRect(
    IN HDC         hdc,
    IN CONST PRECT lprc,
    IN HBRUSH      hbr
);

INT
VEAPI
VeaGdiAddFontResourceEx(
    IN CONST CHAR *FilePath,
    IN ULONG       Flags,
    IN PVOID       Reserved OPTIONAL
);

BOOLEAN
VEAPI
VeaGdiRemoveFontResourceEx(
    IN CONST CHAR *FilePath,
    IN ULONG       Flags,
    IN PVOID       Reserved OPTIONAL
);

HANDLE
VEAPI
VeaGdiAddFontMemResourceEx(
    IN  PVOID  FontData,
    IN  ULONG  FontSize,
    IN  PVOID  Reserved OPTIONAL,
    OUT PULONG OutNumFonts OPTIONAL
);

BOOLEAN
VEAPI
VeaGdiRemoveFontMemResourceEx(
    IN HANDLE hFontResource
);

#define FONT_MAX_FILE_SIZE (16 * 1024 * 1024)

/* EOF */