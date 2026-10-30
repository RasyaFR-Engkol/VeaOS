#pragma once

#include <veaddk.h>

#include <pragmapush1.h>

typedef struct _NVME_COMMAND
{
    UCHAR       Opcode;
    UCHAR       Flags;          /* FUSE [1:0], PSDT [7:6] */
    USHORT      CommandId;
    ULONG       NamespaceId;
    ULONGLONG   Reserved;
    ULONGLONG   MetadataPointer;
    ULONGLONG   Prp1;
    ULONGLONG   Prp2;
    ULONG       Cdw10;
    ULONG       Cdw11;
    ULONG       Cdw12;
    ULONG       Cdw13;
    ULONG       Cdw14;
    ULONG       Cdw15;
} NVME_COMMAND, *PNVME_COMMAND;

typedef struct _NVME_COMPLETION
{
    ULONG       Result;
    ULONG       Reserved;
    USHORT      SubmissionHead;
    USHORT      SubmissionQueueId;
    USHORT      CommandId;
    USHORT      StatusAndPhase; /* bit 0 = phase, bit 1-15 = status */
} NVME_COMPLETION, *PNVME_COMPLETION;

#include <pragmapop.h>

#define NVME_ADMIN_QUEUE_DEPTH  32
#define NVME_IO_QUEUE_DEPTH     64

typedef enum _NVME_STATE
{
    NvmeStateStopped,
    NvmeStateStarted,
    NvmeStateRemoved
} NVME_STATE;

typedef volatile struct _NVME_CONTROLLER_REGISTERS
{
    ULONG CapLow;       /* 0x00 */
    ULONG CapHigh;      /* 0x04 */
    ULONG Version;      /* 0x08 */
    ULONG IntMs;        /* 0x0C */
    ULONG IntMc;        /* 0x10 */
    ULONG Cc;           /* 0x14 */
    ULONG Reserved0;    /* 0x18 */
    ULONG Csts;         /* 0x1C */
    ULONG Nssr;         /* 0x20 */
    ULONG Aqa;          /* 0x24 */
    ULONG AsqLow;       /* 0x28 */
    ULONG AsqHigh;      /* 0x2C */
    ULONG AcqLow;       /* 0x30 */
    ULONG AcqHigh;      /* 0x34 */
} NVME_CONTROLLER_REGISTERS, *PNVME_CONTROLLER_REGISTERS;

typedef struct _NVME_QUEUE
{
    PVOID           SubmissionVa;   /* dari MmAllocateContiguousMemory */
    ULONG           SubmissionPhys;
    PVOID           CompletionVa;
    ULONG           CompletionPhys;
    USHORT          QueueId;
    USHORT          Depth;
    USHORT          SubmissionTail;
    USHORT          CompletionHead;
    UCHAR           Phase;          /* phase bit yang diharapkan, awal 1 */
    volatile ULONG *SubmissionDoorbell;
    volatile ULONG *CompletionDoorbell;
    KSPIN_LOCK      Lock;
} NVME_QUEUE, *PNVME_QUEUE;

typedef struct _NVME_REQUEST_CONTEXT
{
    POIP Oip;
    PVOID PrpListVa;
    ULONG Length;
    BOOLEAN InUse;
} NVME_REQUEST_CONTEXT, *PNVME_REQUEST_CONTEXT;

typedef struct _NVME_DEVICE_EXTENSION
{
    LIST_ENTRY                  ChildList; 
    KSPIN_LOCK                  ChildLock; 
    ULONG                       ChildCount;
    PDEVICE_OBJECT              PhysicalDeviceObject;
    PDEVICE_OBJECT              DeviceObject;      
    PDEVICE_OBJECT              NextLowerDevice;  
    PDRIVER_OBJECT              DriverObject;  
    struct _NVME_DEVICE_EXTENSION *ControllerExtension;
    NVME_STATE                  State;
    ULONG                       BarPhysical;
    ULONG                       BarLength;
    ULONG                       InterruptVector;
    BOOLEAN                     HasInterruptVector;
    PNVME_CONTROLLER_REGISTERS  Registers;
    ULONG                       DoorbellStride;     /* 4 << CAP.DSTRD */
    USHORT                      MaxQueueEntries;    /* CAP.MQES + 1 */
    ULONG                       ReadyTimeoutMs;     /* CAP.TO * 500 */
    NVME_QUEUE                  AdminQueue;
    NVME_QUEUE                  IoQueue;
    USHORT                      NextCommandId;
    USHORT                      ActiveIoCommands;
    NVME_REQUEST_CONTEXT        IoRequests[NVME_IO_QUEUE_DEPTH];
    PKINTERRUPT                 InterruptObject;
    KINTERRUPT_CONNECTION_TYPE  InterruptType;
    ULONG                       MessageCount;
    PKINTERRUPT                 AdminInterrupt;
    PKINTERRUPT                 IoInterrupt;
    UCHAR                       MsixCapOffset;
    volatile ULONG             *MsixTable;
    USHORT                      MsixTableSize;
    KSPIN_LOCK                  InterruptLock;
    ULONG                       NamespaceId;
    ULONGLONG                   NamespaceBlocks;
    ULONG                       BlockSize;
    UCHAR                       BlockSizeShift;
    ANSI_STRING                 DeviceSymbolic;
    KDPC                        CompletionDpc;

    // PER DISK
    BOOLEAN                     PdoDisk;              
    LIST_ENTRY                  ChildEntry;
} NVME_DEVICE_EXTENSION, *PNVME_DEVICE_EXTENSION;

#define NVME_IO_QUEUE_USABLE_ENTRIES (NVME_IO_QUEUE_DEPTH - 1)

/* GENERAL */
#define NVME_ADMIN_QUEUE_DEPTH      32
#define NVME_IO_QUEUE_DEPTH         64
#define NVME_ADMIN_QUEUE_ID         0
#define NVME_IO_QUEUE_ID            1
#define NVME_SQ_ENTRY_SIZE          64
#define NVME_CQ_ENTRY_SIZE          16
#define NVME_DOORBELL_BASE          0x1000
#define NVME_POOL_TAG               'emVN'
 
/* CAP (Low = bit 0-31, High = bit 32-63) */
#define NVME_CAP_MQES(Low)          ((Low) & 0xFFFF)
#define NVME_CAP_TIMEOUT(Low)       (((Low) >> 24) & 0xFF)          /* satuan 500 ms */
#define NVME_CAP_DSTRD(High)        ((High) & 0xF)
#define NVME_CAP_MPSMIN(High)       (((High) >> 16) & 0xF)
#define NVME_CAP_MPSMAX(High)       (((High) >> 20) & 0xF)
 
/* CC */
#define NVME_CC_ENABLE              (1UL << 0)
#define NVME_CC_CSS_NVM             (0UL << 4)
#define NVME_CC_MPS(n)              ((ULONG)(n) << 7)               /* 0 = 4 KB */
#define NVME_CC_AMS_RR              (0UL << 11)
#define NVME_CC_SHN_NORMAL          (1UL << 14)
#define NVME_CC_IOSQES(n)           ((ULONG)(n) << 16)              /* log2(64) = 6 */
#define NVME_CC_IOCQES(n)           ((ULONG)(n) << 20)              /* log2(16) = 4 */
 
/* CSTS */
#define NVME_CSTS_READY             (1UL << 0)
#define NVME_CSTS_FATAL             (1UL << 1)
#define NVME_CSTS_SHST_MASK         (3UL << 2)
 
/* ADMIN OPCODE */
#define NVME_ADMIN_DELETE_IO_SQ     0x00
#define NVME_ADMIN_CREATE_IO_SQ     0x01
#define NVME_ADMIN_DELETE_IO_CQ     0x04
#define NVME_ADMIN_CREATE_IO_CQ     0x05
#define NVME_ADMIN_IDENTIFY         0x06
#define NVME_ADMIN_SET_FEATURES     0x09
 
/* IO OPCODE */
#define NVME_IO_FLUSH               0x00
#define NVME_IO_WRITE               0x01
#define NVME_IO_READ                0x02
 
/* IDENTIFY CNS & FEATURE */
#define NVME_CNS_NAMESPACE          0x00
#define NVME_CNS_CONTROLLER         0x01
#define NVME_CNS_ACTIVE_NS_LIST     0x02
#define NVME_FEATURE_NUM_QUEUES     0x07
 
VOID
VEAPI
NVMeInitializeQueueState(
    IN PNVME_QUEUE Queue,
    IN USHORT QueueId,
    IN USHORT Depth
);

VEASTATUS
VEAPI
NVMeStartDevice(
    PDEVICE_OBJECT,
    POIP,
    POIP_STACK
);

VEASTATUS
VEAPI
NVMeCallLowerPnp(
    IN PDEVICE_OBJECT LowerDevice,
    IN UCHAR MinorFunction,
    OUT POIP_INFORMATION Information
);

VEASTATUS
VEAPI
NVMeSetupInterrupt(IN PNVME_DEVICE_EXTENSION DevExt);

UCHAR
NVMeFindPciCapability(
    IN PDEVICE_OBJECT LowerDevice,
    IN UCHAR TargetCapId
);

VEASTATUS
NVMeReadPciConfigDword(
    IN PDEVICE_OBJECT LowerDevice,
    IN UCHAR Offset,
    OUT PULONG Value
);

VEASTATUS
NVMeWritePciConfigDword(
    IN PDEVICE_OBJECT LowerDevice,
    IN UCHAR Offset,
    IN ULONG Value
);

VEASTATUS
NVMeReadPciConfigWord(
    IN PDEVICE_OBJECT LowerDevice,
    IN UCHAR Offset,
    OUT PUSHORT Value
);

VEASTATUS
NVMeWritePciConfigWord(
    IN PDEVICE_OBJECT LowerDevice,
    IN UCHAR Offset,
    IN USHORT Value
);

VEASTATUS
VEAPI
NVMeMjDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN POIP Oip,
    IN POIP_STACK OipStack
);

VEASTATUS
VEAPI
NVMeBuildPrp(
    IN PNVME_DEVICE_EXTENSION DevExt,
    IN PVOID Buffer,
    IN PMDL Mdl OPTIONAL,
    IN ULONG Length,
    OUT PULONGLONG Prp1,
    OUT PULONGLONG Prp2,
    OUT PVOID *PrpListVa
);

VOID
VEAPI
NVMeCompletionDpcRoutine(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
);

VEASTATUS
VEAPI
NVMeMjReadWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN POIP Oip,
    IN POIP_STACK OipStack
);

VEASTATUS
VEAPI
NVMeCreateNamespacePdo(
    IN PNVME_DEVICE_EXTENSION DevExt,
    IN ULONG NamespaceId
);

VEASTATUS
VEAPI
NVMeQueryDeviceRelations(
       IN PNVME_DEVICE_EXTENSION DevExt,
       IN DEVICE_RELATION_TYPE RelationType,
       OUT PDEVICE_RELATIONS *DeviceRelations
);

VEASTATUS
VEAPI
NVMeQueryNamespaceId(
       IN PNVME_DEVICE_EXTENSION DevExt,
       IN POIP Oip,
       IN BUS_QUERY_ID_TYPE IdType
);

#define NVME_DISABLE_CONTROLLER(RegCc) \
    RegCc &= ~NVME_CC_ENABLE