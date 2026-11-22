#pragma once

#include <veaddk.h>
#include <psdk/video.h>

typedef struct _VBE_DEVICE_EXTENSION {
    PDEVICE_OBJECT Self;
    PDEVICE_OBJECT PhysicalDeviceObject;    /* TargetDevice (PDO dari bus) */
    PDEVICE_OBJECT LowerDeviceObject;       /* Target forward OIP ke bawah */
    ANSI_STRING    SymbolicLinkName;
    BOOLEAN        InterfaceEnabled;
    BOOLEAN        IsInitialized;

    /* Parameter mode & LFB dari BvGetVideoInfo */
    PVOID          FrameBufferBase;
    SIZE_T         FrameBufferSize;
    ULONG          Width;
    ULONG          Height;
    ULONG          Pitch;
    ULONG          BitsPerPixel;
} VBE_DEVICE_EXTENSION, *PVBE_DEVICE_EXTENSION;

VEASTATUS
VEAPI
VbeObIntHandler(
    IN PVOID Object,
    IN POIP Oip
);

VEASTATUS
VEAPI
VbeCreateDevice(
    IN PDRIVER_OBJECT DriverObject
);

/* EOF */