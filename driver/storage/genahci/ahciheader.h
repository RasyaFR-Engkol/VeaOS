// HEADER FOR AHCI DRIVER

#pragma once

#include <veaddk.h>
#include "ahcibits.h"

#define AHCI_MAX_PORTS                 32
#define AHCI_MAX_COMMAND_SLOTS         32
#define AHCI_COMMAND_TABLE_PRDT_COUNT  16
#define AHCI_MAX_PRDT_ENTRIES AHCI_COMMAND_TABLE_PRDT_COUNT
#define AHCI_DEVICE_DIRQL CLOCK_LEVEL - 3

typedef enum _AHCI_DEVICE_TYPE
{
    AhciDeviceTypeUnknown = 0,
    AhciDeviceTypeSata,          // Hard Disk / SSD SATA biasa (0x00000101)
    AhciDeviceTypeSatapi,        // Optical Drive / CD-ROM ATAPI (0xEB140101)
    AhciDeviceTypeEnclosure,     // Enclosure Management Bridge (0xC33C0101)
    AhciDeviceTypePortMultiplier // Port Multiplier (0x96690101)
} AHCI_DEVICE_TYPE, *PAHCI_DEVICE_TYPE;

typedef enum _AHCI_INTERRUPT_MODE {
    AhciInterruptModeNone = 0,
    AhciInterruptModeLegacy,
    AhciInterruptModeMessage // MSI / MSI-X
} AHCI_INTERRUPT_MODE;

typedef struct _AHCI_INTERRUPT_INFO {
    AHCI_INTERRUPT_MODE Mode;
    ULONG Vector;
    UCHAR CapOffset;
} AHCI_INTERRUPT_INFO, *PAHCI_INTERRUPT_INFO;

#pragma pack(push, 1)

typedef struct _AHCI_COMMAND_HEADER {
    UCHAR  Cfl : 5;                  // Command FIS Length dalam DWORD (5-16)
    UCHAR  A : 1;                    // ATAPI Command (1 = ATAPI)
    UCHAR  W : 1;                    // Write (1 = Write to Device, 0 = Read)
    UCHAR  P : 1;                    // Prefetchable
    UCHAR  R : 1;                    // Reset
    UCHAR  B : 1;                    // BIST
    UCHAR  C : 1;                    // Clear Busy upon R_OK
    UCHAR  Reserved0 : 1;
    UCHAR  Pmp : 4;                  // Port Multiplier Port
    USHORT Prdtl;                    // PRDT Length (Jumlah Entry di Table)
    ULONG Prdbc;                    // PRD Byte Count (Total byte yang ter-transfer)
    ULONG Ctba;                     // Command Table Base Address (Low 32-bit, Align 128B)
    ULONG Ctbau;                    // Command Table Base Address (Upper 32-bit)
    ULONG Reserved1[4];
} AHCI_COMMAND_HEADER, *PAHCI_COMMAND_HEADER;

#pragma pack(pop)

typedef struct _AHCI_PORT_REGISTERS {
    ULONG Clb;                      // PxCLB  - Command List Base Address
    ULONG Clbu;                     // PxCLBU - Command List Base Upper
    ULONG Fb;                       // PxFB   - FIS Base Address
    ULONG Fbu;                      // PxFBU  - FIS Base Upper
    ULONG Is;                       // PxIS   - Interrupt Status
    ULONG Ie;                       // PxIE   - Interrupt Enable
    ULONG Cmd;                      // PxCMD  - Command and Status
    ULONG Reserved0;
    ULONG Tfd;                      // PxTFD  - Task File Data
    ULONG Sig;                      // PxSIG  - Signature
    ULONG Ssts;                     // PxSSTS - SATA Status
    ULONG Sctl;                     // PxSCTL - SATA Control
    ULONG Serr;                     // PxSERR - SATA Error
    ULONG Sact;                     // PxSACT - SATA Active
    ULONG Ci;                       // PxCI   - Command Issue
    ULONG Sntf;                     // PxSNTF - SATA Notification
    ULONG Fbs;                      // PxFBS  - FIS-based Switching
    ULONG Reserved1[11];
    ULONG Vendor[4];
} volatile AHCI_PORT_REGISTERS, *PAHCI_PORT_REGISTERS;

typedef struct _AHCI_HBA_REGISTERS {
    ULONG Cap;                      // 0x00: Host Capabilities
    ULONG Ghc;                      // 0x04: Global Host Control
    ULONG Is;                       // 0x08: Interrupt Status
    ULONG Pi;                       // 0x0C: Ports Implemented
    ULONG Vs;                       // 0x10: Version
    ULONG CccCtl;                   // 0x14: Command Completion Coalescing Control
    ULONG CccPorts;                 // 0x18: Command Completion Coalescing Ports
    ULONG EmLoc;                    // 0x1C: Enclosure Management Location
    ULONG EmCtl;                    // 0x20: Enclosure Management Control
    ULONG Cap2;                     // 0x24: Extended Host Capabilities
    ULONG Bohc;                     // 0x28: BIOS/OS Handoff Control
    UCHAR  Reserved[0xA0 - 0x2C];
    UCHAR  Vendor[0x100 - 0xA0];
    AHCI_PORT_REGISTERS Ports[AHCI_MAX_PORTS]; // 0x100+
} volatile AHCI_HBA_REGISTERS, *PAHCI_HBA_REGISTERS;

typedef struct _AHCI_PORT_CAPABILITIES {
    BOOLEAN SupportsNcq;
    BOOLEAN SupportsLba48;
    ULONG MaxQueueDepth; // 1 s/d 32
} AHCI_PORT_CAPABILITIES, *PAHCI_PORT_CAPABILITIES;

typedef struct _AHCI_IO_CHUNK_CONTEXT {
    POIP MasterOip;
    ULONGLONG CurrentStartLba;
    ULONG RemainingBytes;
    ULONG TransferredBytes;
    ULONG BufferByteOffset; 
    BOOLEAN IsWrite;
    BOOLEAN UsesBounceBuffer;
} AHCI_IO_CHUNK_CONTEXT, *PAHCI_IO_CHUNK_CONTEXT;

typedef struct _AHCI_PORT_CONTEXT {
    ULONG PortNumber;
    BOOLEAN IsActive;
    AHCI_DEVICE_TYPE DeviceType;
    AHCI_PORT_CAPABILITIES Capabilities;
    PAHCI_PORT_REGISTERS Registers;
    struct _AHCI_DEVICE_EXTENSION *DevExt;
    ULONGLONG TotalSectors;        
    ULONG SectorSize;                   
    CHAR ModelNumber[41];                  
    PAHCI_COMMAND_HEADER CommandListVirtual;
    PHYSICAL_ADDRESS CommandListPhysical;
    PVOID ReceivedFisVirtual;
    PHYSICAL_ADDRESS ReceivedFisPhysical;
    PVOID CommandTableVirtual[AHCI_MAX_COMMAND_SLOTS];
    PHYSICAL_ADDRESS CommandTablePhysical[AHCI_MAX_COMMAND_SLOTS];
    ULONG SlotAllocBitmap;                   
    PVOID PendingOip[AHCI_MAX_COMMAND_SLOTS];
    AHCI_IO_CHUNK_CONTEXT ChunkContext[AHCI_MAX_COMMAND_SLOTS];
    ULONG SlotTimeoutCount[AHCI_MAX_COMMAND_SLOTS];
    PDEVICE_OBJECT DiskPdo;
    LIST_ENTRY ChildListEntry;
    KSPIN_LOCK PortLock;
    PKEVENT CompletionEvent;
    KDPC CompletionDpc;
    ULONG LastPortIs;
    ANSI_STRING SymbolicLinkName;
    PVOID BounceBufferVirtual;
    PHYSICAL_ADDRESS BounceBufferPhysical;
} AHCI_PORT_CONTEXT, *PAHCI_PORT_CONTEXT;

typedef struct _AHCI_BUS_INTERFACE
{
    PGET_SET_DEVICE_DATA SetBusData;
    PGET_SET_DEVICE_DATA GetBusData;
    PVOID ReferenceInterface;
    PVOID DereferenceInterface;
    PVOID Context;
} AHCI_BUS_INTERFACE, *PAHCI_BUS_INTERFACE;

#pragma pack(push, 1)

typedef struct _PHYSICAL_REGION_DESCRIPTOR_TABLE
{
    ULONG Dba;          // Physical Data Base Address Low (32-bit)
    ULONG Dbau;         // Physical Data Base Address Upper (32-bit)
    ULONG Reserved;     // Reserved
    ULONG Dbc : 22;     // Data Byte Count (0-indexed: 511 = 512 Bytes, Max 4MB)
    ULONG Reserved2 : 9;
    ULONG I : 1;        // Interrupt on Completion (1 = Trigger ISR pas Selesai)
} PHYSICAL_REGION_DESCRIPTOR_TABLE, *PPHYSICAL_REGION_DESCRIPTOR_TABLE;

typedef struct _FIS_REG_H2D
{
    UCHAR FisType;      // Always 0x27 (FIS_TYPE_REG_H2D)
    UCHAR PmPort_C;     // Bit 7: Command Bit (1 = Command, 0 = Control), Bit 0-3: Port Multiplier
    UCHAR Command;      // ATA Command Code (e.g. 0xEC for ATA_CMD_IDENTIFY)
    UCHAR FeaturesLow;  // Features (Low Byte)

    UCHAR LbaLow;       // LBA 0-7
    UCHAR LbaMid;       // LBA 8-15
    UCHAR LbaHigh;      // LBA 16-23
    UCHAR Device;       // Device Register (Bit 6 = 1 for LBA mode)

    UCHAR LbaLowExp;    // LBA 24-31 (LBA48)
    UCHAR LbaMidExp;    // LBA 32-39 (LBA48)
    UCHAR LbaHighExp;   // LBA 40-47 (LBA48)
    UCHAR FeaturesHigh; // Features (High Byte)

    UCHAR SectorCountLow;  // Sector Count (Low Byte)
    UCHAR SectorCountHigh; // Sector Count (High Byte)
    UCHAR Control;         // Control Register
    UCHAR Reserved;

    ULONG Reserved2;       // DWORD 4 Reserved
} FIS_REG_H2D, *PFIS_REG_H2D;

#define AHCI_COMMAND_TABLE_PRDT_COUNT 16

typedef struct _AHCI_COMMAND_TABLE
{
    UCHAR Cfb[64];      // Command FIS Buffer (FIS_REG_H2D ditempel di sini)
    UCHAR Acmd[16];     // ATAPI Command Buffer (12/16 bytes)
    UCHAR Reserved[48]; // Reserved

    // Array PRDT (Descriptor lokasi memori buffer I/O)
    PHYSICAL_REGION_DESCRIPTOR_TABLE Prdt[AHCI_COMMAND_TABLE_PRDT_COUNT];
} AHCI_COMMAND_TABLE, *PAHCI_COMMAND_TABLE;

#define AHCI_CMD_TABLE_SIZE (128 + (AHCI_COMMAND_TABLE_PRDT_COUNT * sizeof(PHYSICAL_REGION_DESCRIPTOR_TABLE)))

#pragma pack(pop)

typedef struct _AHCI_DEVICE_EXTENSION {
    PDEVICE_OBJECT FunctionalDeviceObject; // FDO milik genahci.ko
    PDEVICE_OBJECT PhysicalDeviceObject;   // PDO milik vocpi.ko
    PDEVICE_OBJECT NextDeviceObject;       // Target dari IoAttachDeviceToDeviceStack
    AHCI_BUS_INTERFACE BusInterface;
    PAHCI_HBA_REGISTERS HbaRegisters;      // Virtual Address hasil MmMapIoSpace
    PHYSICAL_ADDRESS HbaPhysicalAddress;   // Physical Address BAR5 dari PCI
    ULONG HbaLength;
    PKINTERRUPT InterruptObject;           // Terhubung ke AhciObIntHandler
    KSPIN_LOCK ControllerLock;
    ULONG Vector;
    KIRQL Irql;
    ULONG PortsImplementedMask;            // Bitmap dari register PI
    ULONG MaxPortsCount;                   // Dihitung dari CAP.NP
    ULONG CommandSlotsCount;               // Dihitung dari CAP.NCS
    BOOLEAN Supports64BitDma;              // Flag CAP.S64A
    PVOID DmaBufferVirtual;
    PHYSICAL_ADDRESS DmaBufferPhysical;
    ULONG DmaBufferSize;
    LIST_ENTRY ChildPdoListHead;
    KSPIN_LOCK ChildPdoListLock;
    ULONG ChildPdoCount;
    AHCI_PORT_CONTEXT Ports[AHCI_MAX_PORTS];
} AHCI_DEVICE_EXTENSION, *PAHCI_DEVICE_EXTENSION;

typedef struct _ATA_IDENTIFY_DATA
{
    USHORT GeneralConfiguration;        // Word 0
    USHORT LogicalCylinders;            // Word 1
    USHORT Reserved2;                   // Word 2
    USHORT LogicalHeaders;              // Word 3
    USHORT UnformattedBytesPerTrack;    // Word 4
    USHORT UnformattedBytesPerSector;   // Word 5
    USHORT SectorsPerTrack;             // Word 6
    USHORT VendorUnique1[3];            // Word 7-9
    CHAR   SerialNumber[20];            // Word 10-19
    USHORT BufferType;                  // Word 20
    USHORT BufferSize;                  // Word 21
    USHORT EccBytes;                    // Word 22
    CHAR   FirmwareRevision[8];         // Word 23-26
    CHAR   ModelNumber[40];             // Word 27-46
    USHORT MaximumBlockTransfer;        // Word 47
    USHORT Reserved48;                  // Word 48
    USHORT Capabilities;                // Word 49
    USHORT Capabilities2;               // Word 50
    USHORT Reserved51[9];               // Word 51-59
    ULONG  TotalUserAddressableSectors; // Word 60-61 (LBA28)
    USHORT Reserved62[38];              // Word 62-99
    ULONGLONG Max48BitLbaSectors;       // Word 100-103 (LBA48)
} ATA_IDENTIFY_DATA, *PATA_IDENTIFY_DATA;

#pragma pack(push, 1)
typedef struct _ATA_TRIM_RANGE_ENTRY {
    ULONGLONG StartLba : 48;    // Bit 0-47: Starting LBA (Little Endian)
    ULONGLONG SectorCount : 16; // Bit 48-63: Jumlah Sektor yang di-TRIM
} ATA_TRIM_RANGE_ENTRY, *PATA_TRIM_RANGE_ENTRY;
#pragma pack(pop)

VEASTATUS
VEAPI
AhciObIntHandler(
    IN PVOID Object,
    IN POIP Oip
);

VEASTATUS
VEAPI
AhciAddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT TargetDevice
);

PHYSICAL_ADDRESS
VEAPI
AhciGetAbarPhysicalAddress(
    IN POBIO_RESOURCE_LIST ResourceList,
    OUT PULONG OutAbarLength
);

BOOLEAN
VEAPI
AhciGetInterruptInfo(
    IN POBIO_RESOURCE_LIST ResourceList,
    OUT PAHCI_INTERRUPT_INFO IntrInfo
);

BOOLEAN
VEAPI
AhciIsrRoutine(
    IN PKINTERRUPT Interrupt,
    IN PVOID Context
);

VEASTATUS
VEAPI
AhciEnableMsiCapability(
    IN PBUS_INTERFACE_STANDARD BusInterface,
    IN UCHAR CapOffset,
    IN PKINTERRUPT_MESSAGE MsgInfo
);

VEASTATUS
VEAPI
AhciGetBusInterface(
    IN PAHCI_DEVICE_EXTENSION DevExt,
    IN const PGUID Guid,
    OUT PVOID BusInterface,
    IN ULONG Version,
    IN ULONG Size
);

ULONG
VEAPI
AhciReadConfig(
    IN PBUS_INTERFACE_STANDARD BusInterface,
    IN ULONG Offset,
    IN ULONG Length,
    OUT PVOID Buffer
);

ULONG
VEAPI
AhciWriteConfig(
    IN PBUS_INTERFACE_STANDARD BusInterface,
    IN ULONG Offset,
    IN ULONG Length,
    IN PVOID Buffer
);

USHORT
VEAPI
AhciReadConfigWord(
    IN PBUS_INTERFACE_STANDARD BusInterface,
    IN ULONG Offset
);

VOID
VEAPI
AhciWriteConfigWord(
    IN PBUS_INTERFACE_STANDARD BusInterface,
    IN ULONG Offset,
    IN USHORT Value
);

VOID
VEAPI
AhciWriteConfigDword(
    IN PBUS_INTERFACE_STANDARD BusInterface,
    IN ULONG Offset,
    IN ULONG Value
);

VEASTATUS
VEAPI
AhciEnableLegacyInterrupt(
    IN PBUS_INTERFACE_STANDARD BusInterface
);

VEASTATUS
VEAPI
AhciInitializePorts(IN PAHCI_DEVICE_EXTENSION DevExt);

AHCI_DEVICE_TYPE
VEAPI
AhciCheckForSignature(IN PAHCI_PORT_CONTEXT PortCtx);

VOID
VEAPI
AhciCreatePdoByPort(
    IN PAHCI_DEVICE_EXTENSION DevExt,
    IN PAHCI_PORT_CONTEXT PortCtx
);

VEASTATUS
VEAPI
AhciCallLowerDeviceOip(
    IN PDEVICE_OBJECT TargetDeviceObject,
    IN UCHAR MajorFunction,
    IN UCHAR MinorFunction,
    IN POIP_STACK ParametersStack OPTIONAL,
    OUT POIP_INFORMATION Information
);

VEASTATUS
VEAPI
AhciStartHbaEngine(
    IN PAHCI_DEVICE_EXTENSION DevExt
);

VOID
VEAPI
AhciDpcRoutine(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
);

VEASTATUS
VEAPI
AhciSendIdentifyDevice(
    IN PAHCI_DEVICE_EXTENSION DevExt,
    IN PAHCI_PORT_CONTEXT PortCtx,
    OUT PATA_IDENTIFY_DATA OutIdentifyData
);

VOID
VEAPI
AhciFreeCommandSlot(
    IN PAHCI_PORT_CONTEXT PortCtx,
    IN ULONG Slot
);

ULONG
VEAPI
AhciAllocateCommandSlot(
    IN PAHCI_PORT_CONTEXT PortCtx
);

VEASTATUS
VEAPI
AhciExecutePassThroughCommand(
    IN PAHCI_DEVICE_EXTENSION DevExt,
    IN PAHCI_PORT_CONTEXT PortCtx,
    IN OUT PATA_PASS_THROUGH_EX AtaPt,
    IN OUT PVOID DataBuffer OPTIONAL
);

VEASTATUS
VEAPI
AhciSendFlushCache(
    IN PAHCI_DEVICE_EXTENSION DevExt,
    IN PAHCI_PORT_CONTEXT PortCtx
);

VEASTATUS
VEAPI
AhciPdoReadWrite(
    IN PAHCI_PORT_CONTEXT PortCtx,
    IN POIP Oip,
    IN POIP_STACK OipStack
);

VOID
VEAPI
AhciParseDeviceCapabilities(
    IN PAHCI_PORT_CONTEXT PortCtx,
    IN PATA_IDENTIFY_DATA IdentifyData
);

VOID
VEAPI
AhciPortTimerRoutine(
    IN PDEVICE_OBJECT DeviceObject,
    IN PVOID Context
);

VEASTATUS
VEAPI
AhciResetPortEngine(
    IN PAHCI_PORT_CONTEXT PortCtx
);

VEASTATUS
VEAPI
AhciDispatchSubChunk(
    IN PAHCI_PORT_CONTEXT PortCtx,
    IN ULONG Slot
);

VEASTATUS
VEAPI
AhciSendTrimCommand(
    IN PAHCI_PORT_CONTEXT PortCtx,
    IN PDEVICE_DATA_SET_RANGE Ranges,
    IN ULONG RangeCount
);

VEASTATUS
VEAPI
AhciExecuteSyncCommand(
    IN PAHCI_PORT_CONTEXT PortCtx,
    IN ULONG Slot
);

VOID
VEAPI
AhciSetupSinglePrdt(
    IN PAHCI_COMMAND_TABLE CmdTable,
    IN ULONG EntryIndex,
    IN PHYSICAL_ADDRESS PhysAddr,
    IN ULONG ByteCount,
    IN BOOLEAN InterruptOnCompletion
);

VOID
VEAPI
AhciPrepareH2dCommand(
    IN PAHCI_PORT_CONTEXT PortCtx,
    IN ULONG Slot,
    IN BOOLEAN IsWrite,
    IN USHORT PrdtCount,
    OUT PAHCI_COMMAND_HEADER *OutHeader,
    OUT PAHCI_COMMAND_TABLE *OutTable,
    OUT PFIS_REG_H2D *OutFis
);

BOOLEAN
VEAPI
AhciIsBufferAligned(
    IN PVOID VirtualAddress,
    IN ULONG Length
);

#define PCI_COMMAND_REGISTER             0x04
#define PCI_COMMAND_INTERRUPT_DISABLE    (1 << 10)

/* EOF */