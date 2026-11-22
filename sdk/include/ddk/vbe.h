#pragma once

typedef struct _VEA_DISPLAY_MODE_INFO {
    ULONG            Width;
    ULONG            Height;
    ULONG            Pitch;             /* Bytes per scanline */
    ULONG            BitsPerPixel;      /* Misal: 32 (ARGB/XRGB) */
    PVOID            FrameBufferBase;   /* Virtual address LFB yang sudah di-map */
    PHYSICAL_ADDRESS FrameBufferPhys;   /* Alamat fisik LFB */
    SIZE_T           FrameBufferSize;   /* Total ukuran VRAM/LFB dalam byte */
} VEA_DISPLAY_MODE_INFO, *PVEA_DISPLAY_MODE_INFO;