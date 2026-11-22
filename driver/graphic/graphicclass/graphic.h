#pragma once

#include <veaddk.h>
#include <ddk/vbe.h>

typedef struct _GRAPHIC_DEVICE_EXTENSION {
    /* 1. Rantai Device Stack & Object Manager */
    PDEVICE_OBJECT        Self;                 /* Pointer balik ke FDO /Device/DisplayX */
    PDEVICE_OBJECT        LowerDeviceObject;    /* Target IRP forward (vbedriver.ko) */
    PDEVICE_OBJECT        PhysicalDeviceObject; /* PDO asli dari bus/miniport */
    PFILE_OBJECT          LowerFileObject;      /* Referensi file dari ObioGetDeviceObjectPointer */
    ULONG                 DisplayIndex;         /* 0 untuk Display0, 1 untuk Display1, dst. */
    CHAR                  InterfaceName[128];   /* Simpan nama link interface buat pencocokan REMOVAL */
    KSPIN_LOCK            DeviceLock;           /* Pengaman akses bersamaan */
    ULONG                 OpenCount;            /* Jumlah handle aktif (misal dari veapi.ko) */
    BOOLEAN               IsStarted;            /* TRUE kalau device siap nerima IOCTL */
    BOOLEAN               IsRemoved;            /* TRUE kalau lagi proses cabut device */
    BOOLEAN               ModeCached;           /* TRUE kalau info mode sudah ditarik dari vbedriver */
    VEA_DISPLAY_MODE_INFO CurrentMode;          /* Cache resolusi & alamat LFB aktif */
} GRAPHIC_DEVICE_EXTENSION, *PGRAPHIC_DEVICE_EXTENSION;

/* EOF */