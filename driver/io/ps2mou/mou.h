#pragma once

#include <veaddk.h>
#include <psdk/veamouse.h>
#include <../lib/gcc/gcc_intrin.h>

#define PS2MOU_RING_BUFFER_SIZE 256
#define PS2MOU_IO_TIMEOUT       100000
#define WHEEL_DELTA             120
#define PS2MOU_POLL_LIMIT  100000

typedef struct _PS2MOU_DEVICE_EXTENSION {
    PVOID Pdo;
    PVOID LowerDevice;
    ULONG Flags;
    ULONG DataPort;
    ULONG CommandPort;
    ULONG IrqVector;
    PKINTERRUPT Interrupt;
    ANSI_STRING SymbolicLinkName;
    BOOLEAN InterfaceRegistered;
    UCHAR PacketBytes[4];
    UCHAR PacketIndex;
    UCHAR PacketLength;
    UCHAR DeviceId;
    BOOLEAN HasWheel;
    UCHAR PreviousButtons;
    KSPIN_LOCK RingLock;
    MOUSE_INPUT_DATA RingBuffer[PS2MOU_RING_BUFFER_SIZE];
    ULONG RingReadIndex;
    ULONG RingWriteIndex;
    ULONG RingCount;
    POIP PendingReadOip;
} PS2MOU_DEVICE_EXTENSION, *PPS2MOU_DEVICE_EXTENSION;

VEASTATUS
VEAPI
PS2MouObjectInterruptHandler(PVOID Object, POIP Oip);

/* EOF */