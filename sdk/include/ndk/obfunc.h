#pragma once

#include "procbind.h"
#include "obtype.h"
#include "mmtype.h"
#include "veastatus.h"
#include "ultypes.h"

LONG
VEAPI
ObAllocateObject(
    POBJECT_TYPE ObjectType,
    POBJECT_ATTRIBUTES ObjectAttr,
    ULONG ObjectSize,
    PVOID *ReturnedObject // Output: Pointer ke badan objek
);

LONG
VEAPI
ObCreateObjectType(
    PCHAR TypeName,
    POBJECT_TYPE_INITIALIZER Initializer,
    POBJECT_TYPE *ReturnedObjectType // Output pointer
);

BOOLEAN
VEAPI
ObInitSystem(VOID);

VEASTATUS
VEAPI
ObpCreateRootStructure(VOID);

VOID
VEAPI
ObpDumpObjectTreeRecursive(
    PVOID DirectoryObject,
    ULONG Depth
);

VOID
VEAPI
ObDumpObjectTree(VOID);

LONG
VEAPI
ObCreateSymbolicLink(
    PVOID ParentDirectory,
    PCHAR LinkName,
    PCHAR TargetPath
);

VEASTATUS
VEAPI
ObCreateSymbolicLinkToObject(
    PVOID ParentDirectory,
    PCHAR LinkName,
    PVOID TargetObject
);

VEASTATUS
VEAPI
ObDeleteSymbolicLink(
    PVOID ParentDirectory,
    PCHAR LinkName
);


LONG
VEAPI
ObpInsertDirectory(
    PVOID DirectoryObject,
    PVOID Object
);

#define OBJECT_TO_OBJECT_HEADER(Object) (((POBJECT_HEADER)(Object)) - 1)
#define OBJECT_HEADER_TO_OBJECT(Header) ((PVOID)((POBJECT_HEADER)(Header) + 1))

LONG
VEAPI
ObDereferenceObject(
    PVOID Object
);

LONG
VEAPI
ObReferenceObject(
    PVOID Object
);

VEASTATUS
VEAPI
ObQueryNameString(
    IN PVOID Object,
    OUT PANSI_STRING ObjectNameInfo,
    IN ULONG Length,
    OUT PULONG ReturnLength
);

VOID
VEAPI
ObpDeleteSymbolicLink(
    PVOID Object
);

LONG
VEAPI
ObpParseSymbolicLink(
    PVOID ParseObject,
    POB_LOOKUP_CONTEXT LookupContext,
    PVOID Context,
    PVOID *ResolvedObject
);

VEASTATUS
VEAPI
ObReferenceObjectByName(
    POBJECT_ATTRIBUTES ObjectAttributes,
    POBJECT_TYPE ObjectType,
    PVOID *Object
);

#define ObioGetNextOipStackLocation(Oip) \
    ((POIP_STACK)(Oip->CurrentStackLocation - 1))

#define ObioGetCurrentOipStackLocation(Oip) \
    ((POIP_STACK)(Oip->CurrentStackLocation))

VEASTATUS
VEAFAST
ObioCallObject(
    PVOID TargetObject,
    POIP Oip
);

/* COMPATIBILITY */
#define ObioGoInterruptObject ObioCallObject

POIP
VEAPI
ObioAllocateOip(
    ULONG StackCount
);

/* These MDLs describe current mappings; VeaOS does not yet pin their pages. */
PMDL
VEAPI
ObioAllocateMdl(
    IN PVOID VirtualAddress,
    IN ULONG Length,
    IN BOOLEAN HaveSecondaryBuffer,
    IN BOOLEAN ChargeQuote,
    IN POIP Oip OPTIONAL
);

VEASTATUS
VEAPI
ObioBuildPartialMdl(
    IN PMDL SourceMdl,
    IN PMDL TargetMdl,
    IN PVOID VirtualAddress,
    IN ULONG Length
);

VOID
VEAPI
ObioFreeMdl(
    IN PMDL Mdl
);

VOID
VEAPI
ObioCompleteRequest(
    POIP Oip
);

BOOLEAN
VEAPI
ObioInitSubsystem(
    VOID
);

VEASTATUS
ObInitializeLookupContext(
    POB_LOOKUP_CONTEXT LookupContext,
    POBJECT_ATTRIBUTES ObjectAttributes,
    POBJECT_TYPE ExpectedType,
    ACCESS_MASK DesiredAccess,      // <-- PARAMETER BARU
    KPROCESSOR_MODE AccessMode      // <-- PARAMETER BARU (biar AccessMode terisi juga)
);

VEASTATUS
VEAPI
ObCreateObject(
    IN KPROCESSOR_MODE ProcessorMode,
    IN POBJECT_TYPE ObjectType,
    IN POBJECT_ATTRIBUTES ObjectAttributes,
    IN KPROCESSOR_MODE OwnershipMode,
    IN OUT PVOID ParseContext OPTIONAL,
    IN ULONG ObjectSize,
    IN ULONG PagedPoolCharge,
    IN ULONG NonPagedPoolCharge,
    OUT PVOID *ReturnedObject
);

VEASTATUS
VEAPI
ObInsertObject(
    PVOID Object,
    IN PACCESS_STATE AccessState,
    IN ACCESS_MASK DesiredAccess,
    IN ULONG ObjectPointerBias,
    OUT PVOID *NewObject OPTIONAL,
    OUT PHANDLE Handle
);

VEASTATUS
VEAPI
ObioCreateNewDevice(
    PDRIVER_OBJECT DriverObject,
    ULONG DeviceExtSize,
    PCHAR DeviceName,         // Contoh: "serial0"
    ULONG DeviceType,
    PDEVICE_OBJECT *NewDevice // Output pointer
);

VEASTATUS
VEAPI
ObioAttachInterruptToThisObject(
    PVOID Object,
    POBIO_INTERRUPT_HANDLER CustomHandler
);

VEASTATUS
VEAPI
ObioCreateDriver(
    PCHAR DriverName,                  // Contoh: "Serial"
    PDRIVER_INITIALIZE InitializationFunction,
    OUT PDRIVER_OBJECT *DriverObject
);

VEASTATUS
VEAPI
ObioSendWriteRequest(
    PDEVICE_OBJECT DeviceObject,
    PVOID DataBuffer,
    ULONG DataLength
);

VOID
VEAPI
ObioFreeOip(
    POIP Oip
);

VEASTATUS
VEAPI
ObioSendReadRequest(
    PDEVICE_OBJECT DeviceObject,
    PVOID DataBuffer,       // Buffer tujuan (di-alokasi oleh pemanggil)
    ULONG DataLength,       // Berapa byte yang mau dibaca
    PULONG BytesRead        // Output: Berapa byte yang aktual berhasil dibaca
);

VEASTATUS
VEAPI
VeaCreateDirectoryNamespace(
    OUT PHANDLE Handle,
    IN ACCESS_MASK Mask,
    IN POBJECT_ATTRIBUTES ObjectAttributes);

#define OBJECT_HEADER_TO_NAME_INFO(Header) \
    ((POBJECT_HEADER_NAME_INFO)((PUCHAR)(Header) - sizeof(OBJECT_HEADER_NAME_INFO)))

BOOLEAN
VEAPI
ObpCheckUnsecureName(
    PANSI_STRING Name,
    KPROCESSOR_MODE AccessMode
);

VOID
VEAPI
ObpPushCalloutTracker(
    PVOID CurrentThread, // Nanti ganti dengan PKTHREAD/PETHREAD
    POB_CALLOUT_RECORD Tracker
);

VOID
VEAPI
ObpPopCalloutTracker(
    PVOID CurrentThread,
    POB_CALLOUT_RECORD Tracker
);

VEASTATUS
VEAPI
ObReferenceObjectByHandle(
    HANDLE Handle,
    ULONG DesiredAccess,
    POBJECT_TYPE ExpectedType,
    KPROCESSOR_MODE AccessMode,
    PVOID *Object
);

VEASTATUS
VEAPI
ObOpenObjectByName(
    POBJECT_ATTRIBUTES ObjectAttributes,
    POBJECT_TYPE ExpectedType,
    KPROCESSOR_MODE AccessMode,
    ULONG DesiredAccess,
    PVOID ParseContext,
    PHANDLE ReturnedHandle
);

BOOLEAN
VEAPI
ObioInitSystem1(VOID);

VOID
VEAPI
ObioInitGroupOrderLoad(VOID);

VOID
VEAPI
ObioDumpGroupLoadOrder(VOID);

VEASTATUS
VEAPI
ObioOpenVeaKey(
    IN HANDLE RootDirectory OPTIONAL,
    OUT PHANDLE Handle, 
    IN PANSI_STRING KeyName,
    IN ACCESS_MASK DesiredAccess
);

VEASTATUS
VEAPI
ObioCreateVeaKey(
    IN HANDLE RootDirectory OPTIONAL,   // *** BARU ***
    IN PANSI_STRING StringKey,
    IN ULONG Option,
    IN ACCESS_MASK DesiredAccess,
    OUT PULONG Disposition,
    OUT PHANDLE Handle
);

VEASTATUS
VEAPI
ObioInitializePnPService(VOID);

VOID
VEAPI
ObioPnpChangeState(
    IN PDEVICE_NODE DeviceNode,
    IN DEVNODE_STATE DeviceNodeState
);

PDEVICE_NODE
VEAPI
ObioCreateDeviceNode(IN PDEVICE_OBJECT Pdo);

VEASTATUS
VEAPI
ObioAddDeviceNodeChild(
    IN PDEVICE_NODE Parent,
    IN PDEVICE_NODE Child
);

VEASTATUS
VEAPI
ObioTraversePdoTree(
    IN PDEVICE_NODE RootNode,
    IN POBIO_PDO_TRAVERSE_ROUTINE Routine,
    IN PVOID Context
);

PDEVICE_OBJECT
VEAPI
ObioGetDeviceObjectFromDeviceInstance(
    IN PANSI_STRING DeviceInstance
);

VEASTATUS
VEAPI
ObioPnpRootDriverEntry(PDRIVER_OBJECT DriverObject);

VOID
VEAPI
ObioDetachObject(
    IN PVOID Object
);

PVOID
VEAPI
ObioAttachObject(
    IN PVOID SourceObject,
    IN PVOID TargetObject
);

VEASTATUS
VEAPI
ObioRegisterRootDeviceNode(
    IN PDEVICE_OBJECT Pdo,
    IN PCSTR HardwareId        // <--- Masukkan HardwareID sebagai parameter!
);

USHORT
VEAPI
ObioGetGroupOrderIndex(HANDLE ServiceHandle);

BOOLEAN
VEAPI
ObioInitializeBootDriver(VOID);

VOID
VEAPI
VeaClose(IN HANDLE Handle);

VEASTATUS
VEAPI
ObioGetValueKey(
    IN HANDLE Handle,
    IN PANSI_STRING ValueName,
    OUT PVOID Buffer,
    IN ULONG BufferSize,
    OUT PULONG Type,
    OUT PULONG ResultSizeOut
);

BOOLEAN
VEAPI
ObProcessDebug(ULONG ArgumentCount, PCHAR* Argument);

extern BOOLEAN ObReportLifecycleReady;
extern POB_LIFECYCLE_REPORT_TABLE ObLifecycleTable;
extern ULONG ObLifecycleTableFreeIndex;

VOID
VEAPI
ObpReportLifecycle(
    IN PVOID Object,
    IN ULONG CurrentReference,
    IN BOOLEAN IsReferenced,
    IN BOOLEAN IsHandle,
    IN PVOID OwnerThread,
    IN PVOID TargetObject
);

VEASTATUS
VEAPI
ObioSynchronusCallByStack
(
    IN PDEVICE_OBJECT DeviceObject,
    IN POIP_STACK OipStack,
    IN OUT PVOID *Information
);

PDEVICE_NODE
VEAPI
ObioGetDeviceNode(PDEVICE_OBJECT DeviceObject);

VOID
VEAPI
ObioDevNodeStateMachine(IN PDEVICE_NODE DeviceNode);

VEASTATUS
VEAPI
ObioUpdateDeviceState(PDEVICE_NODE DeviceNode);

VOID
VEAPI
ObioQueueDeviceAction(
    IN PDEVICE_OBJECT DeviceObject,
    IN DEVICE_ACTION Action,
    IN PKEVENT CompletionEvent OPTIONAL,
    OUT PVEASTATUS CompletionStatus OPTIONAL
);

VEASTATUS
VEAPI
ObioCallSpecificPnpOip
(
    IN PDEVICE_OBJECT DeviceObject,
    IN UCHAR MinorFunction,
    IN POIP_STACK Stack OPTIONAL,
    IN OUT POIP_INFORMATION Information
);

VEASTATUS
VEAPI
ObioCreateDeviceNodeInstancePath
(
    IN PDEVICE_NODE DeviceNode,
    OUT PANSI_STRING InstancePath
);

VEASTATUS
VEAPI
ObioQueryDeviceRelations(
    IN PDEVICE_NODE DeviceNode,
    IN DEVICE_RELATION_TYPE RelationType);

DEVNODE_STATE
VEAPI
ObioSetDevNodeState(
    IN PDEVICE_NODE DeviceNode,
    IN DEVNODE_STATE DevnodeState
);

PDEVICE_NODE
VEAPI
ObioAllocateZeroDeviceNode(
    IN PDEVICE_OBJECT PhysicalDeviceObject
);

VOID
VEAPI
ObioInsertDeviceNode(
    IN PDEVICE_NODE DeviceNode,
    IN PDEVICE_NODE ParentNode
);

VEASTATUS
VEAPI
ObioEnumerateDeviceNode(
    IN PDEVICE_NODE DeviceNode
);

VOID
VEAPI
ObioDumpDeviceTree(VOID);

VOID
VEAPI
ObioDeleteDevice(IN PDEVICE_OBJECT DeviceObject);

VEASTATUS
VEAPI
ObioAddDeviceByDeviceObject(
    IN PDEVICE_NODE DeviceNode OPTIONAL,
    IN PDEVICE_OBJECT DeviceObject,
    IN PDRIVER_OBJECT TargetDriver,
    OUT POIP_INFORMATION Information
);

VEASTATUS
VEAPI
ObioCallDriverAddDevice(
    IN PDEVICE_NODE DeviceNode
);

DEVNODE_STATE
VEAPI
ObioSetDevNodeState(
    IN PDEVICE_NODE DeviceNode,
    IN DEVNODE_STATE DevnodeState
);

PVOID
VEAPI
ObGetTopAttachedObject(IN PVOID Object);

PDEVICE_OBJECT
VEAPI
ObioGetTopDeviceObject(
    IN PDEVICE_OBJECT DeviceObject
);

PDEVICE_OBJECT
VEAPI
IoGetRelatedDeviceObject(
    IN PFILE_OBJECT FileObject
);

VEASTATUS
VEAPI
ObioAssignDeviceResources(IN PDEVICE_NODE DeviceNode);

VEASTATUS
VEAPI
ObioStartDevice(IN PDEVICE_NODE DeviceNode);

VOID
VEAPI
ObioSetDevNodeProblem(
    IN PDEVICE_NODE DeviceNode,
    IN ULONG Problem
);

VEASTATUS
VEAPI
ObioStartDeviceFinal(
    IN PDEVICE_NODE DeviceNode
);

VOID
ObioSkipCurrentOipStackLocation(
    IN OUT POIP Oip
);

VOID
ObioCopyCurrentOipStackLocationToNext(
    IN OUT POIP Oip
);

VEASTATUS
VEAPI
ObioRegisterDeviceInterface(
    IN PDEVICE_OBJECT PhysicalDeviceObject,
    IN const GUID *InterfaceClassGuid,
    IN PANSI_STRING ReferenceString OPTIONAL,
    /* Must be empty; release a successful result with RtlFreeAnsiString. */
    OUT PANSI_STRING SymbolicLinkName
);

VEASTATUS
VEAPI
ObioGetDeviceInterfaces(
    IN const GUID *InterfaceClassGuid,
    IN PDEVICE_OBJECT PhysicalDeviceObject OPTIONAL,
    IN ULONG Flags,
    /* ANSI MULTI_SZ; release the returned pool buffer with UlFreePoolWithTag. */
    OUT PCHAR *SymbolicLinkList
);

VEASTATUS
VEAPI
ObioSetDeviceInterfaceState(
    IN PANSI_STRING SymbolicLinkName,
    IN BOOLEAN Enable
);

VOID
VEAPI
ObioNotifyDeviceInterfaceChange(
    IN PGUID Event,
    IN PGUID InterfaceClassGuid,
    IN PANSI_STRING SymbolicLinkName
);

VEASTATUS
VEAPI
ObioRegisterPlugPlayNotification(
    IN OBIO_NOTIFICATION_EVENT_CATEGORY EventCategory,
    IN ULONG EventCategoryFlags,
    IN PVOID EventCategoryData OPTIONAL,
    IN PDRIVER_OBJECT DriverObject,
    IN PDRIVER_NOTIFICATION_CALLBACK_ROUTINE CallbackRoutine,
    IN OUT PVOID Context OPTIONAL,
    OUT PVOID *NotificationEntry
);

VEASTATUS
VEAPI
ObioReferenceDeviceObjectByName(
    IN PANSI_STRING SymbolicLinkName,
    OUT PDEVICE_OBJECT *DeviceObject
);

#define ObioIsValidPhysicalDeviceObject(DeviceObject)                       \
    ((DeviceObject) != NULL &&                                              \
     (DeviceObject)->DeviceNode != NULL &&                                  \
     (DeviceObject)->DeviceNode->PhysicalDeviceObject == (DeviceObject))

VOID
VEAPI
ObioInvalidateDeviceRelations(
    IN PDEVICE_OBJECT DeviceObject,
    IN DEVICE_RELATION_TYPE RelationType
);

PIO_WORKITEM
VEAPI
ObioAllocateWorkItem(
    IN PDEVICE_OBJECT DeviceObject
);

VOID
VEAPI
ObioFreeWorkItem(IN PIO_WORKITEM WorkItem);

VOID
VEAPI
ObioQueueWorkItem(
    IN PIO_WORKITEM WorkItem,
    IN PIO_WORKITEM_ROUTINE WorkerRoutine,
    IN WORK_QUEUE_TYPE QueueType,
    IN PVOID Context
);

POIP
VEAPI
ObioBuildAsynchronousFsdRequest(
    ULONG MajorFunction,
    PVOID Object,
    PVOID Buffer,
    ULONG Length,
    PLARGE_INTEGER StartingOffset,
    POIP_INFORMATION StatusInformation
);

POIP
VEAPI
ObioBuildSynchronusFsdRequest(
    ULONG MajorFunction,
    PVOID Object,
    PVOID Buffer,
    ULONG Length,
    PLARGE_INTEGER StartingOffset,
    PKEVENT Event,
    POIP_INFORMATION StatusInformation
);

POIP
VEAPI
ObioBuildDeviceIoControlRequest(
    ULONG IoControlCode,
    PDEVICE_OBJECT DeviceObject,
    PVOID InputBuffer,
    ULONG InputBufferLength,
    PVOID OutputBuffer,
    ULONG OutputBufferLength,
    BOOLEAN InternalDeviceIoControl,
    PKEVENT Event,
    POIP_INFORMATION StatusInformation
);

BOOLEAN
VEAPI
ObioAnalyzeBdiMatchMbr(
    IN PCHAR Buffer,
    IN ULONG PartitionNumber,
    IN ULONG DiskSignature
);

VEASTATUS
VEAPI
ObioCreateBdiSymlink(IN PBLOCK_BOOT_2 BlockBoot2);

VEASTATUS
VEAPI
ObioParseFile(
    IN PVOID ParseObject,
    IN POB_LOOKUP_CONTEXT LookupContext,
    IN PVOID Context,
    OUT PVOID* ResolvedObject
);

PDEVICE_OBJECT
VEAPI
ObioGetRelatedDeviceObject(
    IN PFILE_OBJECT FileObject
);

BOOLEAN
ObioIsOpenPacketValid(IN POPEN_PACKET OpenPacket);

VEASTATUS
VEAPI
ObioParseDevice(
    IN PVOID ParseObject,
    IN POB_LOOKUP_CONTEXT LookupContext,
    IN PVOID Context,
    OUT PVOID *ResolvedObject
);

VOID
VEAPI
ObioRegisterFileSystem(IN PDEVICE_OBJECT FsDevice);

VEASTATUS
VEAPI
ObioMountVolume(
    IN PDEVICE_OBJECT OriginalDevice,
    OUT PVPB *Vpb
);

VEASTATUS
VEAPI
VeaOpenFile(
    OUT PHANDLE FileHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes,
    OUT POIP_INFORMATION IoStatusBlock,
    IN ULONG ShareAccess,
    IN ULONG OpenOptions
);

PVPB
VEAPI
ObioAllocateVpb(IN PDEVICE_OBJECT DeviceObject);

#define ObioDeviceTypeIsStorage(Dev) \
    (BOOLEAN)((Dev->DeviceType == FILE_DEVICE_DISK) || \
     (Dev->DeviceType == FILE_DEVICE_VIRTUAL_DISK) || \
     (Dev->DeviceType == FILE_DEVICE_CD_ROM) || \
     (Dev->DeviceType == FILE_DEVICE_TAPE))

#define ObioDeviceTypeIsFileSystem(CreatedDeviceObject) \
    (CreatedDeviceObject->DeviceType == FILE_DEVICE_DISK_FILE_SYSTEM) || \
        (CreatedDeviceObject->DeviceType == FILE_DEVICE_FILE_SYSTEM) || \
        (CreatedDeviceObject->DeviceType == FILE_DEVICE_CD_ROM_FILE_SYSTEM) || \
        (CreatedDeviceObject->DeviceType == FILE_DEVICE_NETWORK_FILE_SYSTEM) || \
        (CreatedDeviceObject->DeviceType == FILE_DEVICE_TAPE_FILE_SYSTEM)

#define ObpIsKernelHandle(h, mode) \
    ((((ULONG_PTR)(h)) & KERNEL_HANDLE_FLAG) && ((mode) == KernelMode))
#define ObMarkHandleAsKernelHandle(h) ((HANDLE)((ULONG_PTR)(h) | KERNEL_HANDLE_FLAG))
#define ObKernelHandleToHandle(h)     ((HANDLE)((ULONG_PTR)(h) & ~KERNEL_HANDLE_FLAG))

BOOLEAN
VEAPI
ObpInitKernelHandleTable(VOID);

PHANDLE_TABLE
VEAPI
ObpGetHandleTable(IN OUT PHANDLE Handle, IN KPROCESSOR_MODE Mode);

VEASTATUS
VEAPI
ObpCreateHandle(
    IN  PVOID           Object,
    IN  ULONG           ObjAttributes,
    IN  KPROCESSOR_MODE AccessMode,
    IN  ACCESS_MASK     GrantedAccess,
    OUT PHANDLE         ReturnedHandle
);

VOID 
VEAPI 
ObCloseHandle(IN HANDLE Handle, IN KPROCESSOR_MODE Mode);

VOID
VEAPI
ObMakeTemporaryObject(IN PVOID Object);

VOID
VEAPI
ObpRemoveEntryDirectory(
    PVOID Object
);

VOID
VEAPI
ObpDeleteNameCheck(IN PVOID Object);

VOID
VEAPI
ObioTimerDispatch(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
);

VOID
VEAPI
ObioStopTimer(IN PDEVICE_OBJECT DeviceObject);

VOID
VEAPI
ObioStartTimer(IN PDEVICE_OBJECT DeviceObject);

VEASTATUS
VEAPI
ObioInitializeIoTimer(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIO_TIMER_ROUTINE TimerRoutine,
    IN PVOID Context
);

VEASTATUS
VEAPI
VeaCreateFile(
    OUT PHANDLE FileHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes,
    OUT POIP_INFORMATION IoStatusBlock,
    IN PLARGE_INTEGER AllocationSize OPTIONAL,
    IN ULONG FileAttributes,
    IN ULONG ShareAccess,
    IN ULONG CreateDisposition,
    IN ULONG CreateOptions,
    IN PVOID EaBuffer OPTIONAL,
    IN ULONG EaLength
);

VEASTATUS
VEAPI
VeaLoadDriver(IN PANSI_STRING DriverServiceName);

/* EOF */