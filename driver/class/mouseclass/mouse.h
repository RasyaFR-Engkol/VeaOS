#pragma once

#include <veaddk.h>
#include <psdk/veamouse.h>

///
///            MOU CLASS (FDO)
///                  |
///     | -------------------------|
/// MOU CLASS1 (PDO)        MOU CLASS2 (PDO)
///     |                          |
///     | ------------------------ |
///     |                          |
/// PS2 MOU (FDO PDO)   XHCI MOU (FDO PDO)
/// 
/// Note: MOU CLASS1 should be forwarded through Ob as DeviceObject
///       as /Device/Mouse0, MOU CLASS2 as /Device/Mouse1
///

#ifndef MOU_BUFFER_SIZE
#define MOU_BUFFER_SIZE 128
#endif

typedef struct _MOUSE_DEVICE_EXTENSION
{
    BOOLEAN IsPdo; 
    PDEVICE_OBJECT Self;
    union {
        struct {
            PDEVICE_OBJECT LowerDevice; 
            ULONG MouseCount;          
            PDEVICE_OBJECT PdoList[16]; 
        } FdoData;

        struct {
            PDEVICE_OBJECT ParentFdo;
            PDEVICE_OBJECT PortDevice;
            PFILE_OBJECT PortFileObject;
            ULONG UnitId;
            MOUSE_INPUT_DATA InputBuffer[MOU_BUFFER_SIZE];
            ULONG ReadIndex;
            ULONG WriteIndex;
            KSPIN_LOCK MouseLock;
            POIP ReaderOip;
            KDPC InputDpc;
            MOUSE_INPUT_DATA RawPacket;
            POIP PendingReadOip;
        } PdoData;
    };
} MOUSE_DEVICE_EXTENSION, *PMOUSE_DEVICE_EXTENSION;

VEASTATUS
VEAPI
MouDeviceChange(
    IN PVOID NotificationStructure,
    IN PVOID Context
);

VEASTATUS
VEAPI
MouCreatePdoMouse(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PortDeviceObject,
    IN PFILE_OBJECT PortFileObject,
    OUT PDEVICE_OBJECT *OutPdo OPTIONAL
);

VEASTATUS
VEAPI
MouPdoHandle(
    IN PDEVICE_OBJECT Pdo,
    IN POIP Oip
);

VEASTATUS
VEAPI
MouFdoHandle(
    IN PDEVICE_OBJECT Fdo,
    IN POIP Oip
);

/* EOF */