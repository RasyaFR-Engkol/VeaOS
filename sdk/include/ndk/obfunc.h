#pragma once

#include "procbind.h"
#include "obtype.h"
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

VOID 
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
VEAPI
ObioGoInterruptObject(
    PVOID TargetObject,
    POIP Oip
);

POIP
VEAPI
ObioAllocateOip(
    ULONG StackCount
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
    POBJECT_ATTRIBUTES ObjectAttributes
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
VeaCreateDirectoryNamespace(POBJECT_ATTRIBUTES ObjectAttributes);

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
    PHANDLE_TABLE Table,
    HANDLE Handle,
    ULONG DesiredAccess,
    POBJECT_TYPE ExpectedType,
    KPROCESSOR_MODE AccessMode,
    PVOID *Object
);

VEASTATUS
VEAPI
ObOpenObjectByName(
    PHANDLE_TABLE Table,
    POBJECT_ATTRIBUTES ObjectAttributes,
    POBJECT_TYPE ExpectedType,
    KPROCESSOR_MODE AccessMode,
    ULONG DesiredAccess,
    PHANDLE ReturnedHandle
);

BOOLEAN
VEAPI
ObioInitSystem1(VOID);

VOID
VEAPI
ObioInitGroupOrderLoad(VOID);

VEASTATUS
VEAPI
ObioOpenVeaKey(
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
