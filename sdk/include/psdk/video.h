#pragma once

#ifndef FILE_DEVICE_VIDEO
#define FILE_DEVICE_VIDEO               0x00000023
#endif

#define IOCTL_VIDEO_QUERY_NUM_AVAIL_MODES \
    CTL_CODE(FILE_DEVICE_VIDEO, 0x100, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_VIDEO_QUERY_AVAIL_MODES \
    CTL_CODE(FILE_DEVICE_VIDEO, 0x101, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_VIDEO_QUERY_CURRENT_MODE \
    CTL_CODE(FILE_DEVICE_VIDEO, 0x102, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_VIDEO_SET_CURRENT_MODE \
    CTL_CODE(FILE_DEVICE_VIDEO, 0x103, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_VIDEO_RESET_DEVICE \
    CTL_CODE(FILE_DEVICE_VIDEO, 0x104, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_VIDEO_MAP_VIDEO_MEMORY \
    CTL_CODE(FILE_DEVICE_VIDEO, 0x105, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_VIDEO_UNMAP_VIDEO_MEMORY \
    CTL_CODE(FILE_DEVICE_VIDEO, 0x106, METHOD_BUFFERED, FILE_ANY_ACCESS)

/* =====================================================================
 * Struktur Payload IOCTL Video
 * ===================================================================== */

/* Input untuk IOCTL_VIDEO_SET_CURRENT_MODE */
typedef struct _VIDEO_MODE {
    ULONG RequestedMode;
} VIDEO_MODE, *PVIDEO_MODE;

/* Output untuk IOCTL_VIDEO_QUERY_NUM_AVAIL_MODES */
typedef struct _VIDEO_NUM_MODES {
    ULONG NumModes;
    ULONG ModeInformationLength;
} VIDEO_NUM_MODES, *PVIDEO_NUM_MODES;

/* Output untuk IOCTL_VIDEO_QUERY_CURRENT_MODE & QUERY_AVAIL_MODES */
typedef struct _VIDEO_MODE_INFORMATION {
    ULONG Length;
    ULONG ModeIndex;
    ULONG VisScreenWidth;
    ULONG VisScreenHeight;
    ULONG ScreenStride;             /* Bytes per scanline (Pitch) */
    ULONG NumberOfPlanes;           /* 1 untuk Linear Framebuffer */
    ULONG BitsPerPlane;             /* 32 untuk ARGB/XRGB */
    ULONG Frequency;
    ULONG XMillimeter;
    ULONG YMillimeter;
    ULONG NumberRedBits;
    ULONG NumberGreenBits;
    ULONG NumberBlueBits;
    ULONG RedMask;
    ULONG GreenMask;
    ULONG BlueMask;
    ULONG AttributeFlags;
} VIDEO_MODE_INFORMATION, *PVIDEO_MODE_INFORMATION;

/* Input untuk IOCTL_VIDEO_MAP_VIDEO_MEMORY & UNMAP_VIDEO_MEMORY */
typedef struct _VIDEO_MEMORY {
    PVOID RequestedVirtualAddress;
} VIDEO_MEMORY, *PVIDEO_MEMORY;

/* Output untuk IOCTL_VIDEO_MAP_VIDEO_MEMORY */
typedef struct _VIDEO_MEMORY_INFORMATION {
    PVOID VideoRamBase;             /* Virtual address dasar VRAM */
    ULONG VideoRamLength;
    PVOID FrameBufferBase;          /* Virtual address Linear Framebuffer aktif */
    ULONG FrameBufferLength;
} VIDEO_MEMORY_INFORMATION, *PVIDEO_MEMORY_INFORMATION;