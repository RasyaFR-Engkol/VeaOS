// VEAPM DEFINTION

#pragma once
#include "veaddk.h"

#define PM_ERROR(fmt, ...)   DPRINT("PM_ERROR: " fmt, ##__VA_ARGS__)
#define PM_INFO(fmt, ...)    DPRINT("PM_INFO: " fmt, ##__VA_ARGS__)
#define PM_WARNING(fmt, ...) DPRINT("PM_WARNING: " fmt, ##__VA_ARGS__)

typedef enum _PM_PARTITION_STYLE
{
    PmMasterBootRecord,
    PmGuidPartitionTable
} PM_PARTITION_STYLE;

typedef enum _PM_DEVICE_TYPE {
    PmDeviceTypeDiskFilter = 0, // FDO yang menempel di atas Disk (Filter Driver)
    PmDeviceTypePartition       // PDO yang dibuat PartMgr buat tiap partisi
} PM_DEVICE_TYPE;

typedef enum _PM_PNP_STATE {
    PmPnpStateNotStarted = 0,
    PmPnpStateStarted,
    PmPnpStateStopPending,
    PmPnpStateStopped,
    PmPnpStateRemovePending,
    PmPnpStateSurpriseRemoved,
    PmPnpStateRemoved
} PM_PNP_STATE;

typedef struct _PM_DEVICE_EXTENSION
{    
    PDEVICE_OBJECT SelfDeviceObject;
    PDEVICE_OBJECT LowerDeviceObject;
    PDEVICE_OBJECT PhysicalDeviceObject;

    PM_PNP_STATE PnpState;
    PM_PNP_STATE PreviousPnpState;
    PM_DEVICE_TYPE DeviceType;
    ULONG RemoveLock;
    BOOLEAN IsSystemBootPartition;

    ULONG HarddiskNum;
    ULONG PartitionNum;
    ULONG BytesPerSector;
    DISK_GEOMETRY DiskGeometry;

    LIST_ENTRY PartitionListHead;
    KSPIN_LOCK PartitionListLock;
    ULONG PartitionCount;
    LIST_ENTRY PartitionListEntry;
    PDEVICE_OBJECT ParentDeviceObject;

    ULONGLONG StartingOffset;
    ULONGLONG PartitionLength;

    PM_PARTITION_STYLE PartitionStyle;
    union {
        struct {
            UCHAR PartitionType;        // 0x07 (NTFS/exFAT), 0x83 (Linux), dll.
            BOOLEAN BootIndicator;      // Active/Bootable flag
            ULONG HiddenSectors;
            ULONG MbrSignature;         // Disk ID MBR (4 bytes)
        } Mbr;

        struct {
            GUID PartitionTypeGuid;     // GUID Partition Type
            GUID PartitionIdGuid;       // GUID Unique ID
            ULONGLONG Attributes;       // GPT Attributes (Read-only, Hidden, dll)
            WCHAR PartitionName[36];    // UTF-16 Partition Name from GPT Header
        } Gpt;
    } Info;

    ANSI_STRING DeviceName;
    ANSI_STRING SymbolicLinkName;

    KSPIN_LOCK ExtensionLock;
    LIST_ENTRY PendingIrpQueue;
    ULONG LockCount;
    BOOLEAN IsReadOnly;
    BOOLEAN IsMounted;

    PVOID PnpNotificationHandle;
} PM_DEVICE_EXTENSION, *PPM_DEVICE_EXTENSION;

extern PVOID PmNotificationEntry;

VEASTATUS
VEAPI
PmPnpNotificationCallback(
    IN PVOID NotificationStructure,
    IN PVOID Context OPTIONAL
);

VEASTATUS VEAPI
PmpAskDiskGeometry(
    IN PPM_DEVICE_EXTENSION DevExt
);

VEASTATUS VEAPI
PmpAskHarddiskNumber(
    IN PPM_DEVICE_EXTENSION DevExt
);

VEASTATUS VEAPI
PmpReadSectors(
    IN PPM_DEVICE_EXTENSION DevExt,
    IN ULONGLONG StartLba,
    IN ULONG SectorCount,
    OUT PVOID Buffer);

#pragma pack(push, 1)

typedef struct _MBR_PARTITION_ENTRY {
    UCHAR  BootIndicator;
    UCHAR  StartCHS[3];
    UCHAR  PartitionType;
    UCHAR  EndCHS[3];
    ULONG  StartingLBA;
    ULONG  SectorCount;
} MBR_PARTITION_ENTRY, *PMBR_PARTITION_ENTRY;

typedef struct _MASTER_BOOT_RECORD_ENTRY {
    UCHAR               BootCode[440];
    ULONG               DiskSignature;
    USHORT              Reserved;
    MBR_PARTITION_ENTRY Partition[4];
    USHORT              Signature;
} MASTER_BOOT_RECORD_ENTRY, *PMASTER_BOOT_RECORD_ENTRY;

typedef struct _GUID_PARTITION_TABLE_ENTRY {
    GUID      PartitionTypeGuid;
    GUID      UniquePartitionGuid;
    ULONGLONG StartingLBA;
    ULONGLONG EndingLBA;
    ULONGLONG Attributes;
    USHORT    PartitionName[36];
} GUID_PARTITION_TABLE_ENTRY, *PGUID_PARTITION_TABLE_ENTRY;

typedef struct _GPT_HEADER {
    UCHAR     Signature[8];         /* "EFI PART" */
    ULONG     Revision;
    ULONG     HeaderSize;
    ULONG     HeaderCrc32;
    ULONG     Reserved;
    ULONGLONG MyLBA;
    ULONGLONG AlternateLBA;
    ULONGLONG FirstUsableLBA;
    ULONGLONG LastUsableLBA;
    GUID      DiskGuid;
    ULONGLONG PartitionEntryLBA;
    ULONG     NumberOfPartitionEntries;
    ULONG     SizeOfPartitionEntry;
    ULONG     PartitionEntryArrayCrc32;
} GPT_HEADER, *PGPT_HEADER;

#pragma pack(pop)

typedef struct _PM_PARTITION_ENTRY {
    ULONGLONG StartingOffset;    /* byte */
    ULONGLONG PartitionLength;   /* byte */
    ULONG     PartitionStyle;    /* 0 = MBR, 1 = GPT, sesuaikan enum lu */
    UCHAR     PartitionType;     /* MBR */
    BOOLEAN   BootIndicator;     /* MBR */
    ULONG     HiddenSectors;     /* MBR */
} PM_PARTITION_ENTRY, *PPM_PARTITION_ENTRY;


#define MBR_TYPE_EMPTY       0x00
#define MBR_TYPE_EXTENDED    0x05
#define MBR_TYPE_EXTENDED_LBA 0x0F
#define MBR_TYPE_EXTENDED_LINUX 0x85
#define MBR_TYPE_GPT_PROTECTIVE 0xEE
#define MBR_BOOT_ACTIVE      0x80

#define GPT_HEADER_LBA           1
#define GPT_PARTITION_ENTRY_LBA  2

VEASTATUS
VEAPI
PmpDetectAndCreatePartition(
    IN PPM_DEVICE_EXTENSION DevExt,
    IN PCHAR Buffer
);

VEASTATUS
VEAPI
PmPartitionInt(
    IN PDEVICE_OBJECT DeviceObject,
    IN POIP Oip,
    IN POIP_STACK OipStack,
    IN PPM_DEVICE_EXTENSION DeviceExtension
);

VEASTATUS
VEAPI
PmDeviceControl(
    IN POIP Oip,
    IN POIP_STACK OipStack,
    IN PPM_DEVICE_EXTENSION DevExt
);