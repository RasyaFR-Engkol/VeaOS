#pragma once

#include <veaddk.h>

///
///            KBD CLASS (FDO)
///                  |
///     | -------------------------|
/// KBD CLASS1 (PDO)        KBD CLASS2 (PDO)
///     |                          |
///     | ------------------------ |
///     |                          |
/// PS2 KEY (FDO PDO)   XHCI KEY (FDO PDO)
/// 
/// Note: KBD CLASS1 should be forwarded through Ob as DeviceObject
///       as /Device/Keyboard0, KBD CLASS2 as /Device/Keyboard1
/// 

typedef struct _KBD_DEVICE_EXTENSION {
    BOOLEAN IsPdo; // TRUE = PDO (Keyboard0/1), FALSE = FDO (Class Manager)
    PDEVICE_OBJECT Self;

    union {
        // Data khusus kalau ini FDO (Induk)
        struct {
            PDEVICE_OBJECT LowerDevice; // Kalo di-attach ke bus tertentu
            ULONG KeyboardCount;        // Counter buat penamaan Keyboard0, Keyboard1, dst.
            PDEVICE_OBJECT PdoList[16]; // Nyimpen daftar PDO anak
        } FdoData;

        // Data khusus kalau ini PDO (Anak / Keyboard0, Keyboard1)
        struct {
            PDEVICE_OBJECT ParentFdo;
            PDEVICE_OBJECT PortDevice;
            PFILE_OBJECT PortFileObject;
            ULONG UnitId;
            KEYBOARD_INPUT_DATA InputBuffer[KBD_BUFFER_SIZE];
            ULONG ReadIndex;
            ULONG WriteIndex;
            KSPIN_LOCK KeyboardLock;
            POIP ReaderOip;
            KEYBOARD_INPUT_DATA RawPacket;
            POIP PendingReadOip;
        } PdoData;
    };
} KBD_DEVICE_EXTENSION, *PKBD_DEVICE_EXTENSION;

VEASTATUS
VEAPI
KbdPdoHandle(
    IN PDEVICE_OBJECT Pdo,
    IN POIP Oip
);

VEASTATUS
VEAPI
KbdFdoHandle(
    IN PDEVICE_OBJECT Pdo,
    IN POIP Oip
);

VEASTATUS
VEAPI
KbdCreatePdoKeyboard(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PortDeviceObject,
    IN PFILE_OBJECT PortFileObject,
    OUT PDEVICE_OBJECT *OutPdo OPTIONAL
);

VEASTATUS
VEAPI
KbdPnpNotifyCallback(
    IN PVOID NotificationStructure,
    IN PVOID Context
);

ULONG
KbdPopInputData(
    IN PKBD_DEVICE_EXTENSION DevExt,
    OUT PKEYBOARD_INPUT_DATA OutBuffer,
    IN ULONG MaxPackets
);


/* EOF */