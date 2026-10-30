#pragma once

#include "internal/x86.h"
#include "ldrtypes.h"
#include "mmtype.h"
#include "procbind.h"
#include "ultypes.h"
#include "veastatus.h"
#include "ks.h"
#include "sffunc.h"
#include <../psdk/veaioctl.h>

/* =========================================================
 * Major Function (OIP_MJ)
 * ========================================================= */
#define OIP_MJ_CREATE               0x01 // Buka/handle baru ke objek (file, device, dll)
#define OIP_MJ_CLOSE                0x02 // Handle ditutup
#define OIP_MJ_READ                 0x03 // Baca data
#define OIP_MJ_WRITE                0x04 // Tulis data
#define OIP_MJ_DEVICE_CONTROL       0x05 // IOCTL ke device (setara DeviceIoControl)
#define OIP_MJ_INTERNAL_DEVICE_CTL  0x06 // IOCTL internal antar-driver (kernel-mode only)
#define OIP_MJ_POWER                0x07 // Request terkait power management
#define OIP_MJ_PNP                  0x08 // Request terkait Plug and Play
 
#define OIP_MJ_QUERY_INFORMATION    0x09 // "Kasih tau info soal objek ini" (size, attribute, dll)
#define OIP_MJ_SET_INFORMATION      0x0A // "Ubah info objek ini"
#define OIP_MJ_FLUSH_BUFFERS        0x0B // Paksa flush cache/buffer ke storage asli
#define OIP_MJ_CLEANUP              0x0C // Handle count ke-0 tapi objek belum di-close penuh
#define OIP_MJ_SHUTDOWN             0x0D // OS mau shutdown, driver sempet beres-beres dulu
#define OIP_MJ_SYSTEM_CONTROL       0x0E // Query/set system-level info (mirip WMI di NT) 
#define OIP_MJ_FILE_SYSTEM_CONTROL  0x0F
#define OIP_MJ_DIRECTORY_CONTROL    0x10
#define OIP_MJ_MAXIMUM_FUNCTION     0x11

/* =========================================================
 * PnP Minor Functions (Dipakai saat Major = OIP_MJ_PNP)
 * ========================================================= */
#define OIP_MN_START_DEVICE                 0x00 // OS nyuruh driver: "Nyalain hardware-nya sekarang!"
#define OIP_MN_QUERY_REMOVE_DEVICE          0x01 // OS nanya: "Boleh nggak hardware ini gw cabut?"
#define OIP_MN_REMOVE_DEVICE                0x02 // OS nyuruh: "Hardware dicabut, hapus semua alokasi memori!"
#define OIP_MN_CANCEL_REMOVE_DEVICE         0x03 // OS bilang: "Gajadi dicabut, lanjut kerja."
#define OIP_MN_STOP_DEVICE                  0x04 // OS nyuruh: "Berhenti bentar, gw mau atur ulang *resource* PCI."
#define OIP_MN_QUERY_STOP_DEVICE            0x05 // OS nanya: "Bisa di-stop bentar nggak?"
#define OIP_MN_CANCEL_STOP_DEVICE           0x06 // OS bilang: "Gajadi di-stop."
#define OIP_MN_QUERY_DEVICE_RELATIONS       0x07 // OS nanya: "Lu punya anak (child device) nggak? (buat enumerator)"
#define OIP_MN_QUERY_INTERFACE              0x08 // OS nanya: "Minta function pointer interface lu dong (buat bus driver)"
#define OIP_MN_QUERY_CAPABILITIES           0x09 // OS nanya: "Hardware lu support apa aja? (misal: bisa sleep nggak?)"
#define OIP_MN_QUERY_RESOURCES              0x0A // OS nanya: "Lu dapet I/O Port & IRQ berapa dari BIOS/PCI?"
#define OIP_MN_QUERY_RESOURCE_REQUIREMENTS  0x0B // OS nanya: "Lu SEBENERNYA butuh resource apa aja (preferensi lu)?"
#define OIP_MN_QUERY_DEVICE_TEXT            0x0C // OS nanya: "Kasih nama/deskripsi device ini buat ditampilin ke user"
#define OIP_MN_FILTER_RESOURCE_REQUIREMENTS 0x0D // Parent bus/filter driver boleh ubah daftar resource requirement sebelum di-assign
#define OIP_MN_READ_CONFIG                  0x0E // Baca config space (misal PCI config space)
#define OIP_MN_WRITE_CONFIG                 0x0F // Tulis config space
#define OIP_MN_EJECT                        0x10 // OS nyuruh: "Keluarin device ini (eject)"
#define OIP_MN_SET_LOCK                     0x11 // Lock/unlock device biar gak bisa di-eject sembarangan
#define OIP_MN_QUERY_ID                     0x12 // OS nanya: "ID lu apa?" (DeviceID/InstanceID/HardwareID, buat enumerator)
#define OIP_MN_QUERY_PNP_DEVICE_STATE       0x13 // OS nanya status PnP device lu (disabled, gak bisa di-stop, dll)
#define OIP_MN_QUERY_BUS_INFORMATION        0x14 // OS nanya info bus tempat device ini nempel
#define OIP_MN_DEVICE_USAGE_NOTIFICATION    0x15 // OS kasih tau: device ini dipake buat paging/hibernate/dump file
#define OIP_MN_SURPRISE_REMOVAL             0x16 // Device dicabut PAKSA tanpa pemberitahuan duluan (misal USB dicabut)
#define OIP_MN_ADD_DEVICE                   0x17 // Tell driver we're gonna add device
 
/* =========================================================
 * Power Minor Functions (Dipakai saat Major = OIP_MJ_POWER)
 * ========================================================= */
#define OIP_MN_WAIT_WAKE                    0x00 // OS nyuruh hardware nunggu sinyal buat bangunin PC
#define OIP_MN_POWER_SEQUENCE                0x01 // Optimalisasi power state
#define OIP_MN_SET_POWER                    0x02 // OS nyuruh: "Ganti ke mode Sleep (D3) atau Nyala (D0)"
#define OIP_MN_QUERY_POWER                  0x03 // OS nanya: "Aman nggak kalau sekarang lu gw suruh Sleep?"

/* File System MN function */
#define OIP_MN_MOUNT_VOLUME                 0x01

#define OIP_IOCTL_CODE(DeviceType, Function, Method, Access) \
    (((ULONG)(DeviceType) << 16) | ((ULONG)(Access) << 14) | \
     ((ULONG)(Function) << 2) | ((ULONG)(Method)))

#define OIP_METHOD_BUFFERED    0x00 // Paling aman: I/O manager alokasi buffer sendiri di kernel pool, lalu di-copy. Dipake kalau ragu.
#define OIP_METHOD_IN_DIRECT   0x01 // Input lewat MDL yang di-lock ke memori fisik (buat transfer besar, arah user->driver)
#define OIP_METHOD_OUT_DIRECT  0x02 // Sama kayak IN_DIRECT tapi arah driver->user (misal baca data gede dari device)
#define OIP_METHOD_NEITHER     0x03 // Paling cepet tapi paling bahaya: driver dikasih pointer user-mode mentah-mentah, harus validasi sendiri

typedef struct _OIP OIP, *POIP;

typedef VEASTATUS (VEAPI *POIP_COMPLETION_ROUTINE)(
    PVOID TargetObject,
    POIP Oip,
    PVOID Context
);

//
// Device RELATION
//
typedef enum _DEVICE_RELATION_TYPE {
    BusRelations = 0,         // Minta daftar semua Child PDO di bus ini
    EjectionRelations,
    PowerRelations,
    RemovalRelations,
    TargetDeviceRelation
} DEVICE_RELATION_TYPE, *PDEVICE_RELATION_TYPE;

typedef enum _BUS_QUERY_ID_TYPE {
    BusQueryDeviceID,
    BusQueryHardwareIDs,
    BusQueryCompatibleIDs,
    BusQueryInstanceID,
    BusQueryDeviceSerialNumber,
    BusQueryContainerID
} BUS_QUERY_ID_TYPE, *PBUS_QUERY_ID_TYPE;

typedef enum _OBIO_RESOURCE_TYPE
{
    PnpResourceTypeNull = 0,
    PnpResourceTypePort = 1,
    PnpResourceTypeInterrupt = 2,
    PnpResourceTypeMemory = 3,
    PnpResourceTypeDma = 4,
    PnpResourceTypeMessageInterrupt = 5
} OBIO_RESOURCE_TYPE;

typedef struct _OBIO_RESOURCE_DESCRIPTOR
{
    UCHAR Type;      
    UCHAR Flags;      
    USHORT Reserved;

    union
    {
        struct
        {
            ULONG Base;
            ULONG Length;
        } Port;

        struct 
        {
            ULONG Vector;
            ULONG Level;
        } Interrupt;

        struct 
        {
            PHYSICAL_ADDRESS Base;
            ULONG Length;
        } Memory;
    };
} OBIO_RESOURCE_DESCRIPTOR, *POBIO_RESOURCE_DESCRIPTOR;

typedef struct _OBIO_RESOURCE_LIST
{
    ULONG Count;
    OBIO_RESOURCE_DESCRIPTOR Descriptors[1];
} OBIO_RESOURCE_LIST, *POBIO_RESOURCE_LIST;

typedef struct _OBIO_REQUIRED_RESOURCES
{
    UCHAR Option;
    UCHAR Type;
    UCHAR ShareDisposition;
    UCHAR Spare1;
    USHORT Flags;
    USHORT Spare2;
    union
    {
        struct 
        {
            ULONG Length;
            ULONG Alignment;
            PHYSICAL_ADDRESS MinimumAddress;
            PHYSICAL_ADDRESS MaximumAddress;
        } Port;
        struct 
        {
            ULONG MinimumVector;
            ULONG MaximumVector;
        } Interrupt;
    };
} OBIO_REQUIRED_RESOURCES, *POBIO_REQUIRED_RESOURCES;

typedef struct _OBIO_REQUIRED_RESOURCES_LIST
{
    USHORT Version;
    USHORT Revision;
    ULONG Count;
    OBIO_REQUIRED_RESOURCES Descriptors[1];
} OBIO_REQUIRED_RESOURCES_LIST, *POBIO_REQUIRED_RESOURCES_LIST;

typedef struct _OBIO_REQUIRED_RESOURCES_LIST_EX
{
    ULONG ListSize;
    ULONG BusNumber;
    ULONG SlotNumber;
    ULONG Reserved[3];
    ULONG AlternativeLists;
    OBIO_REQUIRED_RESOURCES_LIST List[1];
} OBIO_REQUIRED_RESOURCES_LIST_EX, *POBIO_REQUIRED_RESOURCES_LIST_EX;

typedef struct _OIP_STACK {
    UCHAR MajorFunction;       // Perintah utama (misal OIP_MJ_READ)
    UCHAR MinorFunction;       // Sub-perintah spesifik
    UCHAR Flags;               // Flag eksekusi (misal: Async, Non-Blocking)
    UCHAR Control;             // Status stack ini (misal: Invoke Completion Routine)

    // Parameter spesifik per operasi
    union {
        // Untuk Read & Write standar
        struct {
            ULONG Length;          // Berapa byte yang dibaca/ditulis
            ULONG Key;             // Key untuk locking (opsional)
            LARGE_INTEGER Offset;  // Posisi baca/tulis di device/file
        } ReadWrite;

        // Untuk IOCTL (Device Control)
        struct {
            ULONG OutputBufferLength;
            ULONG InputBufferLength;
            ULONG IoControlCode;
            PVOID Type3InputBuffer; // Buat buffer tambahan jika butuh
        } DeviceControl;

        // Untuk Create/Open Object
        struct {
            ULONG Options;
            USHORT ShareAccess;
            USHORT FileAttributes;
            PVOID SecurityContext; 
            PVOID EaBuffer;
            ULONG EaLength;
        } Create;

        // Generic arguments (buat custom operasi yang ga masuk kategori atas)
        struct {
            PVOID Argument1;
            PVOID Argument2;
            PVOID Argument3;
            PVOID Argument4;
        } Others;

        struct {
            DEVICE_RELATION_TYPE Type;
        } QueryDeviceRelation;

        struct {
            BUS_QUERY_ID_TYPE IdType;
        } QueryId;

        struct {
            struct _DRIVER_OBJECT *DriverObject;
            struct _DEVICE_OBJECT *PhysicalDeviceObject;
        } AddDevice;

        struct {
            const GUID *InterfaceType;
            PVOID InterfaceSpecificData;
            ULONG Size;
            ULONG Version;
        } QueryInterface;

        struct {
            struct _VPB *Vpb;
            struct _DEVICE_OBJECT *DeviceObject;
        } MountVolume;
    } Parameters;

    // Target object untuk "layer" stack ini
    PVOID TargetObject;        // Pengganti DeviceObject di NT
    PVOID FileObject;          // Konteks file jika operasi ini terkait file handle terbuka
    
    // Callback kalau operasi di layer ini udah selesai
    POIP_COMPLETION_ROUTINE CompletionRoutine;   // PIO_COMPLETION_ROUTINE
    PVOID Context;             // Konteks untuk completion routine

} OIP_STACK, *POIP_STACK;

typedef struct _OIP_INFORMATION
{
    union
    {
        VEASTATUS Status;
        PVOID DataPointer;
    };
    ULONG Information;
} OIP_INFORMATION, *POIP_INFORMATION;

typedef struct _OIP {
    ULONG Size;                // Ukuran total memori paket ini (Header + Stacks)
    ULONG StackCount;          // Total jumlah layer stack yang dialokasikan
    volatile LONG CurrentLocation; // Indeks stack yang sedang aktif (dimulai dari StackCount, mundur ke 0)

    struct _MDL *MdlAddress;
    union
    {
        struct _OIP *MasterOip;
        volatile LONG OipCount;
        PVOID SystemBuffer;
    } AssociatedOip;
    
    // Status keseluruhan paket
    OIP_INFORMATION StatusInformation;
    
    // Buffer Data Utama (Direct/Buffered I/O)
    PVOID UserBuffer;          // Buffer asli dari user
    ULONG UserTag, SystemTag;
    
    // Pointer ke stack berjalan
    POIP_STACK CurrentStackLocation; 

    UCHAR RequestorMode;
    ULONG Flags;

    BOOLEAN Cancel;                   // Flag kalau paket ini minta dibatalkan
    PVOID CancelRoutine;              // Fungsi callback buat driver ngeberhentiin hardware

    PKEVENT UserEvent;
    POIP_INFORMATION UserInfoBlock;
    struct _FILE_OBJECT *OriginalFileObject;
} OIP, *POIP;

typedef LONG (VEAPI *POB_INTERRUPT_ROUTINE)(
    PVOID Object, 
    POIP Packet
);

typedef VOID (VEAPI *POB_DELETE_ROUTINE)(
    PVOID Object
);

typedef struct _OB_LOOKUP_CONTEXT OB_LOOKUP_CONTEXT, *POB_LOOKUP_CONTEXT;

typedef LONG (VEAPI *POB_PARSE_ROUTINE)(
    PVOID ParseObject,
    POB_LOOKUP_CONTEXT LookupContext,
    PVOID Context,
    PVOID *ResolvedObject  // [OUT] Objek hasil akhir (kalau ketemu)
);

typedef VEASTATUS (VEAPI *POB_QUERYNAME_METHOD)
(
    IN PVOID Object,
    IN BOOLEAN HasObjectName,
    OUT PANSI_STRING ObjectNameInformation,
    IN ULONG Length,
    OUT PULONG ReturnedLength,
    IN KPROCESSOR_MODE ProcessorMode
);

struct _EPROCESS;

typedef BOOLEAN (VEAPI *POB_OKAYTOCLOSE_METHOD)
(
    IN struct _EPROCESS *Process OPTIONAL,
    IN PVOID Object,
    IN HANDLE Handle,
    IN KPROCESSOR_MODE ProcessorMode
);

typedef VEASTATUS (VEAPI *POB_CREATE_METHOD)
(
    IN PVOID Object
);

typedef VOID
(VEAPI *POB_CLOSE_METHOD)
(
    IN struct _EPROCESS *Process,
    IN PVOID Object,
    IN ACCESS_MASK GrantedAccess,
    IN ULONG_PTR ProcessHandleCount,
    IN ULONG_PTR SystemHandleCount
);

typedef struct _OBJECT_TYPE_INITIALIZER
{
    USHORT Length;                  // Wajib diisi sizeof(OBJECT_TYPE_INITIALIZER)
    BOOLEAN AllowAttachByDefault;   // Aturan filter OIP yang lu bahas sebelumnya
    ULONG PoolTag;                  // Tag buat alokasi (misal 'Thrd', 'Driv')
    ULONG ObjectSize;               // Ukuran spesifik body objek di luar header
    ULONG PoolType;
    ULONG ValidAccessMask;
    ULONG DefaultNonPagedPoolCharge;
    GENERIC_MAPPING GenericMapping;
    POB_INTERRUPT_ROUTINE InterruptHandler;
    POB_DELETE_ROUTINE    DeleteRoutine;    
    POB_PARSE_ROUTINE     ParseRoutine;
    POB_QUERYNAME_METHOD  QueryNameRoutine;
    POB_OKAYTOCLOSE_METHOD OkayToCloseRoutine;
    POB_CREATE_METHOD   CreateRoutine;
    POB_CLOSE_METHOD    CloseRoutine;
} OBJECT_TYPE_INITIALIZER, *POBJECT_TYPE_INITIALIZER;

/* Object Struct */
typedef struct _OBJECT_TYPE
{
    LIST_ENTRY TypeList;
    ULONG TypeId;
    CHAR TypeName[64];
    ULONG PoolTag;
    ULONG ObjectSize;
    GENERIC_MAPPING GenericMapping;
    ULONG TotalObjectCount;
    ULONG Flags;
    ULONG ValidAccessMask;
    BOOLEAN AllowAttachByDefault;
    POB_INTERRUPT_ROUTINE InterruptHandler;
    POB_DELETE_ROUTINE    DeleteRoutine;
    POB_PARSE_ROUTINE     ParseRoutine;
    POB_QUERYNAME_METHOD QueryNameMethod;
    POB_CREATE_METHOD CreateRoutine;
    POB_CLOSE_METHOD CloseRoutine;
} OBJECT_TYPE, *POBJECT_TYPE;

typedef struct _OBJECT_ATTRIBUTES {
    ULONG Length;
    HANDLE RootDirectory;        // HANDLE ke objek direktori tempat path ini bermula (Bisa NULL)
    PANSI_STRING ObjectName;    // String nama path (misal: "Device/MyDisk")
    ULONG Attributes;           // Flags (OBJ_CASE_INSENSITIVE, dll)
    PVOID SecurityDescriptor;   // Konteks keamanan (sementara bisa diset NULL)
    PVOID SecurityQualityOfServiceLife;
} OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;

typedef struct _ACCESS_STATE {
    PVEA_TOKEN  Token; 
    ACCESS_MASK OriginalDesiredAccess;
} ACCESS_STATE, *PACCESS_STATE;

typedef struct _OB_CREATE_INFO {
    OBJECT_ATTRIBUTES Attributes;
    ANSI_STRING       Name;
    KPROCESSOR_MODE   ProcessorMode;
    KPROCESSOR_MODE   OwnershipMode;
    PVOID             ParseContext;
    ULONG             PagedPoolCharge;
    ULONG             NonPagedPoolCharge;
} OB_CREATE_INFO, *POB_CREATE_INFO;

typedef struct _OBJECT_HEADER
{
    LONG_PTR ReferenceCount;
    POBJECT_TYPE Type;
    UCHAR Flags;
    PVEA_SECURITY_DESCRIPTOR SecurityDescriptor;
    UCHAR StackDepth;              // Total layer ke bawah (Buat OS alokasi array OIP_STACK)
    PVOID LowerAttachedObject;     // Pointer ke objek di Bawah (A -> B)
    PVOID UpperAttachedObject;     // Pointer ke objek di Atas  (C -> B, B -> A)
    POB_CREATE_INFO CreateInfo;
} OBJECT_HEADER, *POBJECT_HEADER;

typedef struct _OBJECT_DIRECTORY {
    LIST_ENTRY Head; // Kepala rantai untuk mendaftar objek yang ada di dalam direktori ini
    // Lu bisa tambah Mutex/Lock di sini nanti untuk sinkronisasi antarthread
} OBJECT_DIRECTORY, *POBJECT_DIRECTORY;

typedef struct _OBJECT_HEADER_NAME_INFO
{
    POBJECT_DIRECTORY Directory;
    ANSI_STRING Name;
    ULONG QueryReferences;
    ULONG Reserved2;
    ULONG DbgReferenceCount;
} OBJECT_HEADER_NAME_INFO, *POBJECT_HEADER_NAME_INFO;
// FLAG:
#define OB_FLAG_HAS_NAME_INFO   0x02

typedef struct _OBJECT_DIRECTORY_ENTRY {
    LIST_ENTRY Chain;       // Buat disambungin ke OBJECT_DIRECTORY->Head
    PVOID Object;           // Pointer ke badan objeknya (misal objek Device)
} OBJECT_DIRECTORY_ENTRY, *POBJECT_DIRECTORY_ENTRY;

/* Symbolic link */
typedef struct _OBJECT_SYMBOLIC_LINK {
    ANSI_STRING LinkTarget;
    PVOID TargetObject;
} OBJECT_SYMBOLIC_LINK, *POBJECT_SYMBOLIC_LINK;

extern POBJECT_TYPE ObpTypeObjectType;
extern ULONG ObpNextTypeId;
extern LIST_ENTRY ObpTypeObjectList;
extern POBJECT_TYPE ObpDirectoryObjectType;
extern PVOID ObpRootDirectoryObject;
extern POBJECT_TYPE ObpSymbolicLinkObjectType;
extern PVOID ObpTypesDirectoryObject;
extern PVOID ObpDeviceDirectoryObject;
extern PVOID ObpDriverDirectoryObject;
extern POBJECT_TYPE ObpDeviceObjectType;
extern POBJECT_TYPE ObpDriverObjectType;

#define OBJ_INHERIT             0x00000002
#define OBJ_PERMANENT           0x00000010
#define OBJ_EXCLUSIVE           0x00000020
#define OBJ_CASE_INSENSITIVE    0x00000040
#define OBJ_OPENIF              0x00000080
#define OBJ_OPENLINK            0x00000100
#define OBJ_KERNEL_HANDLE       0x00000200

#define InitializeObjectAttributes( p, n, a, r, s ) { \
    (p)->Length = sizeof( OBJECT_ATTRIBUTES );          \
    (p)->RootDirectory = r;                             \
    (p)->Attributes = a;                                \
    (p)->ObjectName = n;                                \
    (p)->SecurityDescriptor = s;                        \
    (p)->SecurityQualityOfServiceLife = NULL;               \
}

typedef struct _OB_LOOKUP_CONTEXT {
    PVOID CurrentDirectory;
    HANDLE RootDirectory;
    PVOID RootObject;
    POBJECT_TYPE ExpectedType;
    ANSI_STRING RemainingName;  // Menggunakan ANSI_STRING (pointer + length)
    ULONG ReparseCount;
    BOOLEAN CaseInsensitive;
    CHAR ReparseBuffer[512];    // Reparse buffer aman di dalam Context (bukan stack lokal)
    BOOLEAN InsertMode;         // TRUE = Berhenti di parent directory buat persiapan ObInsertObject
    PVOID LockObject;           // Placeholder: Nanti diisi pointer ke Spinlock/Mutex direktori
    KPROCESSOR_MODE AccessMode; // Track siapa yang manggil: Kernel atau User-space
    ACCESS_MASK DesiredAccess;  // What access is allowed in this system?
    ULONG Attributes;
    PVOID ParseContext;
} OB_LOOKUP_CONTEXT, *POB_LOOKUP_CONTEXT;

typedef struct _DEVICE_OBJECT DEVICE_OBJECT, *PDEVICE_OBJECT;
typedef struct _DRIVER_OBJECT DRIVER_OBJECT, *PDRIVER_OBJECT;

typedef VEASTATUS (VEAPI *PDRIVER_INITIALIZE)(
    PDRIVER_OBJECT DriverObject
);

typedef VEASTATUS (VEAPI *POBIO_INTERRUPT_HANDLER)(
    PVOID TargetObject, // DEVICE_OBJECT atau DRIVER_OBJECT
    POIP Oip            // Paket OIP yang sedang diproses
);

//
// Extended Driver
//

// Add to DeviceStack
typedef VEASTATUS (VEAPI OBIO_ADD_DEVICE_ROUTINE)(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT TargetDevice);
typedef OBIO_ADD_DEVICE_ROUTINE *POBIO_ADD_DEVICE_ROUTINE;
typedef VOID (VEAPI OBIO_REMOVE_DEVICE_ROUTINE)(PDRIVER_OBJECT DriverObject);
typedef OBIO_REMOVE_DEVICE_ROUTINE *POBIO_REMOVE_DEVICE_ROUTINE;

typedef struct _VPB *PVPB;

// Struktur DRIVER_OBJECT
typedef struct _DRIVER_OBJECT {
    PVOID DriverStart;          // Pointer ke base address driver di memory (opsional)
    PDEVICE_OBJECT DeviceObject;// Pointer ke Device pertama yang dibuat oleh driver ini
    PVOID DriverExtension;      // Data tambahan spesifik driver
    POBIO_INTERRUPT_HANDLER InterruptHandler;
    POBIO_ADD_DEVICE_ROUTINE AddDeviceRoutine;
    POBIO_REMOVE_DEVICE_ROUTINE RemoveDeviceRoutine;
} DRIVER_OBJECT, *PDRIVER_OBJECT;

typedef VOID
(VEAPI IO_TIMER_ROUTINE)
(IN PDEVICE_OBJECT DeviceObject,
 IN PVOID Context);
typedef IO_TIMER_ROUTINE *PIO_TIMER_ROUTINE;

typedef struct _IO_TIMER
{
    USHORT Type;
    USHORT TimerEnabled;
    LIST_ENTRY IoTimerList;
    PIO_TIMER_ROUTINE TimerRoutine;
    PVOID Context;
    PDEVICE_OBJECT DeviceObject;
} IO_TIMER, *PIO_TIMER;

// Struktur DEVICE_OBJECT
typedef struct _DEVICE_OBJECT {
    PDRIVER_OBJECT DriverObject;// Driver mana yang memiliki device ini
    PDEVICE_OBJECT NextDevice;  // Device berikutnya (jika 1 driver punya banyak device)
    ULONG DeviceType;           // Jenis device (misal: FILE_DEVICE_SERIAL_PORT)
    ULONG Characteristics;      // Karakteristik (misal: FILE_DEVICE_SECURE_OPEN)
    PVOID DeviceExtension;      // Pointer ke memori rahasia/private milik device ini (opsional)
    ULONG Flags; 
    POBIO_INTERRUPT_HANDLER InterruptHandler;
    struct _DEVICE_NODE *DeviceNode;
    PVPB Vpb;
    KEVENT DeviceLock;
    PIO_TIMER Timer;
    union
    {
        LIST_ENTRY ListEntry;
    } Queue;
} DEVICE_OBJECT, *PDEVICE_OBJECT;

#define OIP_DEALLOCATE_ON_COMPLETION 0x00000001

typedef enum _OB_CALLOUT_TYPE {
    ObCalloutCreate,
    ObCalloutParse,
    ObCalloutDelete,
    ObCalloutSecurity
} OB_CALLOUT_TYPE;

typedef struct _OB_CALLOUT_RECORD {
    LIST_ENTRY Chain;             // Rantai untuk melacak jika callout bersarang (nested)
    PVOID Object;                 // Objek yang sedang diproses
    POBJECT_TYPE ObjectType;      // Tipe objek
    OB_CALLOUT_TYPE CalloutType;  // Jenis operasi (Delete, Create, dll)
    PVOID RoutineAddress;         // Alamat memori fungsi driver yang dieksekusi
} OB_CALLOUT_RECORD, *POB_CALLOUT_RECORD;

extern PBLOCK_BOOT_2 ObioBlockBoot;

typedef struct _DEVICE_RELATIONS {
    ULONG Count;              // Jumlah child PDO yang ditemukan
    PDEVICE_OBJECT Objects[1];// Array dinamis pointer DEVICE_OBJECT (PDOs)
} DEVICE_RELATIONS, *PDEVICE_RELATIONS;

//
// Device Node
//

typedef struct _DEVICE_NODE {
    PDEVICE_OBJECT PhysicalDeviceObject;
    struct _DEVICE_NODE *Parent;
    struct _DEVICE_NODE *Sibling;
    struct _DEVICE_NODE *Child;
    struct _DEVICE_NODE *LastChild;
    ANSI_STRING InstancePath;   // -> nyambung ke key Enum\...
    ULONG State;                // Started, Removed, dll
    ULONG PreviousState;
    PDEVICE_RELATIONS PendingDeviceRelations;
    ULONG Flags;
    VEASTATUS LastStateCompletionStatus;
    ULONG Level;
    ULONG Problem;
    POBIO_REQUIRED_RESOURCES_LIST_EX RequiredResources;
    POBIO_RESOURCE_LIST AdditionalResources;
} DEVICE_NODE, *PDEVICE_NODE;

typedef VEASTATUS (VEAPI *POBIO_PDO_TRAVERSE_ROUTINE)(
    IN PDEVICE_OBJECT PhysicalDeviceObject,
    IN PDEVICE_NODE DeviceNode,
    IN PVOID Context
);

typedef enum _DEVNODE_STATE
{
    DeviceNodeUnspecified = 0x300,
    DeviceNodeUninitialized = 0x301,
    DeviceNodeInitialized = 0x302,
    DeviceNodeDriversAdded = 0x303,
    DeviceNodeResourcesAssigned = 0x304,
    DeviceNodeStartPending = 0x305,
    DeviceNodeStartCompletion = 0x306,
    DeviceNodeStartPostWork = 0x307,
    DeviceNodeStarted = 0x308,
    DeviceNodeQueryStopped = 0x309,
    DeviceNodeStopped = 0x30a,
    DeviceNodeRestartCompletion = 0x30b,
    DeviceNodeEnumeratePending = 0x30c,
    DeviceNodeEnumerateCompletion = 0x30d,
    DeviceNodeAwaitingQueuedDeletion = 0x30e,
    DeviceNodeAwaitingQueuedRemoval = 0x30f,
    DeviceNodeQueryRemoved = 0x310,
    DeviceNodeRemovePendingCloses = 0x311,
    DeviceNodeRemoved = 0x312,
    DeviceNodeDeletePendingCloses = 0x313,
    DeviceNodeDeleted = 0x314,
    MaxDeviceNodeState = 0x315,
} DEVNODE_STATE;

//
// Device Action
//

typedef enum _DEVICE_ACTION
{
    PiActionEnumDeviceTree,
    PiActionEnumRootDevices,
    PiActionResetDevice,
    PiActionAddBootDevices,
    PiActionStartDevice,
    PiActionQueryState,
} DEVICE_ACTION;

#define OBIO_DEVNODE_SYNTHETIC       (1 << 0)  // node "dikarang" sistem, bukan hasil enumerasi bus asli (buat Root)
#define OBIO_DEVNODE_ENUMERATED      (1 << 1)  // udah lewat proses enumerasi (ID query dkk)
#define OBIO_DEVNODE_NO_RESOURCE     (1 << 2)  // device ini gak butuh resource assignment (IRQ/port/mem)
#define OBIO_DEVNODE_BOOT_CRITICAL   (1 << 3)  // kalau device ini gagal, sistem gak boleh lanjut boot
#define OBIO_DEVNODE_DISABLED        (1 << 4)  // sengaja dimatiin (user/admin disable)
#define OBIO_DEVNODE_LEGACY_DRIVER   (1 << 5)  // dipasangin manual, bukan lewat PnP match ID (opsional, kalau lu masih mau punya jalur ini)
#define OBIO_DEVNODE_HAS_PROBLEM     (1 << 6)
#define OBIO_DEVNODE_REENUMERATED    (1 << 7)
#define OBIO_DEVNODE_MISSING_DEVICE  (1 << 8)

//
// Device Flags
//
#define DEVICE_INITIALIZING           0x00000001
#define DEVICE_STARTED                0x00000002
#define DEVICE_DISABLED               0x00000004
#define DEVICE_REMOVED                0x00000008
#define DEVICE_SURPRISE_REMOVED       0x00000010
#define DEVICE_REMOVABLE_MEDIA        0x00000100
#define DEVICE_READ_ONLY              0x00000200
#define DEVICE_EXCLUSIVE              0x00000400
#define DEVICE_VIRTUAL                0x00000800
#define DEVICE_POWER_PAGABLE          0x00008000
#define DEVICE_POWER_INRUSH           0x00010000
#define DEVICE_SYSTEM_BOOT           0x00020000

typedef enum _DEVICE_POWER_STATE {
    PowerDeviceUnspecified = 0,
    PowerDeviceD0,            // D0 (Hardware On)
    PowerDeviceD1,            // D1 (Low Power)
    PowerDeviceD2,            // D2 (Standby)
    PowerDeviceD3,            // D3 (Hardware Off / Sleep)
    PowerDeviceMaximum
} DEVICE_POWER_STATE, *PDEVICE_POWER_STATE;

//
// ObioRootDeviceNode
//
extern PDEVICE_NODE ObioRootDeviceNode;

typedef struct _OBIO_DRIVER_INFORMATION
{
    LIST_ENTRY Link;
    PBOOT_DRIVER_LIST_ENTRY BootEntry;
    HANDLE ServiceHandle;
    PDRIVER_OBJECT DriverObject;
    USHORT GroupIndex;
} OBIO_DRIVER_INFORMATION, *POBIO_DRIVER_INFORMATION;

extern PLIST_ENTRY ObioGroupTable;
extern ULONG ObioGroupOrderCount;
extern ULONG ObioGroupTableCount;
extern PANSI_STRING ObioGroupOrderTable;

typedef struct _OB_LIFECYCLE_REPORT_TABLE
{
    PVOID ObjectOrHandle;
    PVOID TargetObject;
    PVOID OwnerThread;
    ULONG CurrentReference;
    BOOLEAN IsReferenced;
    BOOLEAN IsHandle;
} OB_LIFECYCLE_REPORT_TABLE, *POB_LIFECYCLE_REPORT_TABLE;


extern LIST_ENTRY ObioDeviceActionRequestList;
extern KSPIN_LOCK ObioDeviceTreeLock;

#define DNF_PROBLEM OBIO_DEVNODE_HAS_PROBLEM
#define DNF_REENUMERATE OBIO_DEVNODE_REENUMERATED
#define DNF_ENUMERATED OBIO_DEVNODE_ENUMERATED
#define DNF_DEVICE_GONE OBIO_DEVNODE_MISSING_DEVICE
#define DNF_HAS_BOOT_CONFIG (1 << 9)

// 
// DNF PROBLEM FLAGS
//
#define DN_NO_RESOURCES                   0x00000001

//
// DeviceObject Flags
//

#define DO_UNLOAD_PENDING                 0x00000001
#define DO_VERIFY_VOLUME                  0x00000002
#define DO_BUFFERED_IO                    0x00000004
#define DO_EXCLUSIVE                      0x00000008
#define DO_DIRECT_IO                      0x00000010
#define DO_MAP_IO_BUFFER                  0x00000020
#define DO_DEVICE_INITIALIZING            0x00000080
#define DO_SHUTDOWN_REGISTERED            0x00000800
#define DO_BUS_ENUMERATED_DEVICE          0x00001000
#define DO_POWER_PAGABLE                  0x00002000
#define DO_POWER_INRUSH                   0x00004000
#define DO_FILESYSTEM_HIGH_PRIO           0x00008000
#define DO_SYSTEM_BOOT_PARTITION          0x00010000

typedef struct _IO_INTERRUPT
{
    KINTERRUPT FirstInterrupt;
    PKINTERRUPT Interrupt[MAX_CPU];
    KSPIN_LOCK SpinLock;
} IO_INTERRUPT, *PIO_INTERRUPT;

#define FILE_ANY_ACCESS                   0x00000000
#define FILE_SPECIAL_ACCESS               FILE_ANY_ACCESS
#define FILE_READ_ACCESS                  0x00000001
#define FILE_WRITE_ACCESS                 0x00000002

//
// OBIO OIP Flags
//
#define OIP_NOCACHE                     0x00000001
#define OIP_PAGING_IO                   0x00000002
#define OIP_MOUNT_COMPLETION            0x00000002
#define OIP_SYNCHRONOUS_API             0x00000004
#define OIP_ASSOCIATED_OIP              0x00000008
#define OIP_BUFFERED_IO                 0x00000010
#define OIP_DEALLOCATE_BUFFER           0x00000020
#define OIP_INPUT_OPERATION             0x00000040
#define OIP_SYNCHRONOUS_PAGING_IO       0x00000040
#define OIP_CREATE_OPERATION            0x00000080
#define OIP_READ_OPERATION              0x00000100
#define OIP_WRITE_OPERATION             0x00000200
#define OIP_CLOSE_OPERATION             0x00000400
#define OIP_DEFER_IO_COMPLETION         0x00000800
#define OIP_OB_QUERY_NAME               0x00001000
#define OIP_HOLD_DEVICE_QUEUE           0x00002000
#define OIP_RETRY_IO_COMPLETION         0x00004000
#define OIP_CLASS_CACHE_OPERATION       0x00008000

extern LIST_ENTRY ObioPnpNotificationList;

typedef enum _OBIO_NOTIFICATION_EVENT_CATEGORY {
    EventCategoryReserved,
    EventCategoryHardwareProfileChange,
    EventCategoryDeviceInterfaceChange,
    EventCategoryTargetDeviceChange
} OBIO_NOTIFICATION_EVENT_CATEGORY;

typedef VEASTATUS
(VEAPI DRIVER_NOTIFICATION_CALLBACK_ROUTINE)(
  _In_ PVOID NotificationStructure,
  _Inout_opt_ PVOID Context);
typedef DRIVER_NOTIFICATION_CALLBACK_ROUTINE *PDRIVER_NOTIFICATION_CALLBACK_ROUTINE;

typedef struct _OBIO_PNP_NOTIFY_LIST
{
    LIST_ENTRY PnpNotifyList;
    PVOID Context;
    PDRIVER_OBJECT DriverObject;
    PDRIVER_NOTIFICATION_CALLBACK_ROUTINE Callback;
    union
    {
        GUID Guid;
        struct
        {
            PDEVICE_OBJECT DeviceObject;
        };
    };
    OBIO_NOTIFICATION_EVENT_CATEGORY EventCategory;
    UCHAR RefCount;
    BOOLEAN Deleted;
} OBIO_PNP_NOTIFY_LIST, *POBIO_PNP_NOTIFY_LIST;

typedef struct _DEVICE_INTERFACE_CHANGE_NOTIFICATION {
    USHORT Version;
    USHORT Size;    
    GUID Event;
    GUID InterfaceClassGuid;
    PANSI_STRING SymbolicLinkName;
} DEVICE_INTERFACE_CHANGE_NOTIFICATION, *PDEVICE_INTERFACE_CHANGE_NOTIFICATION;

#define PNPNOTIFY_DEVICE_INTERFACE_INCLUDE_EXISTING_INTERFACES    0x00000001
#define DEVICE_INTERFACE_INCLUDE_NONACTIVE                        0x00000001

#define OBIO_RES_TYPE_MEMORY    3
#define OBIO_RES_TYPE_PORT      1
#define OBIO_RES_TYPE_INTERRUPT 2


typedef enum _MEDIA_TYPE {
    Unknown,
    F5_1Pt2_512,
    F3_1Pt44_512,
    F3_2Pt88_512,
    F3_20Pt8_512,
    F3_720_512,
    F5_360_512,
    F5_320_512,
    F5_320_1024,
    F5_180_512,
    F5_160_512,
    RemovableMedia,
    FixedMedia,
    F3_120M_512,
    F3_640_512,
    F5_640_512,
    F5_720_512,
    F3_1Pt2_512,
    F3_1Pt23_1024,
    F5_1Pt23_1024,
    F3_128Mb_512,
    F3_230Mb_512,
    F8_256_128,
    F3_200Mb_512,
    F3_240M_512,
    F3_32M_512
} MEDIA_TYPE, *PMEDIA_TYPE;

typedef struct _DISK_GEOMETRY {
    LARGE_INTEGER  Cylinders;
    MEDIA_TYPE  MediaType;
    ULONG  TracksPerCylinder;
    ULONG  SectorsPerTrack;
    ULONG  BytesPerSector;
} DISK_GEOMETRY, *PDISK_GEOMETRY;

typedef struct _GET_LENGTH_INFORMATION {
    LARGE_INTEGER  Length;
} GET_LENGTH_INFORMATION, *PGET_LENGTH_INFORMATION;

typedef struct _STORAGE_DEVICE_NUMBER {
    ULONG DeviceType;       // FILE_DEVICE_DISK (0x00000007)
    ULONG DeviceNumber;     // Port number (0, 1, dst)
    ULONG PartitionNumber;  // 0 untuk Raw Physical Disk PDO
} STORAGE_DEVICE_NUMBER, *PSTORAGE_DEVICE_NUMBER;

typedef enum _STORAGE_PROPERTY_ID {
    StorageDeviceProperty = 0,
    StorageAdapterProperty,
    StorageDeviceIdProperty
} STORAGE_PROPERTY_ID;

typedef enum _STORAGE_QUERY_TYPE {
    PropertyStandardQuery = 0,
    PropertyExistsQuery,
    PropertyMaskQuery
} STORAGE_QUERY_TYPE;

typedef enum _STORAGE_BUS_TYPE {
    BusTypeUnknown = 0,
    BusTypeScsi,
    BusTypeAtapi,
    BusTypeAta,
    BusTypeSata = 11,
    BusTypeNvme = 17
} STORAGE_BUS_TYPE;

// Struct Input dari Caller/Usermode
typedef struct _STORAGE_PROPERTY_QUERY {
    STORAGE_PROPERTY_ID PropertyId;
    STORAGE_QUERY_TYPE  QueryType;
    UCHAR               AdditionalParameters[1];
} STORAGE_PROPERTY_QUERY, *PSTORAGE_PROPERTY_QUERY;

// Struct Output yang Dibalikin Driver
typedef struct _STORAGE_DEVICE_DESCRIPTOR {
    ULONG            Version;
    ULONG            Size;
    UCHAR            DeviceType;
    UCHAR            DeviceTypeModifier;
    BOOLEAN          RemovableMedia;
    BOOLEAN          CommandQueueing;
    ULONG            VendorIdOffset;
    ULONG            ProductIdOffset;
    ULONG            ProductRevisionOffset;
    ULONG            SerialNumberOffset;
    STORAGE_BUS_TYPE BusType;
    ULONG            RawPropertiesLength;
    UCHAR            RawDeviceProperties[1];
} STORAGE_DEVICE_DESCRIPTOR, *PSTORAGE_DEVICE_DESCRIPTOR;

typedef struct _VERIFY_INFORMATION {
    LARGE_INTEGER StartingOffset;
    ULONG         Length;
} VERIFY_INFORMATION, *PVERIFY_INFORMATION;

typedef struct _ATA_PASS_THROUGH_EX {
    USHORT    Length;
    USHORT    AtaFlags;
    UCHAR     PathId;
    UCHAR     TargetId;
    UCHAR     Lun;
    UCHAR     ReservedAsUchar;
    ULONG     DataTransferLength;
    ULONG     TimeOutValue;
    ULONG     ReservedAsUlong;
    ULONG_PTR DataBufferOffset;
    UCHAR     CurrentTaskFile[8];
    UCHAR     PreviousTaskFile[8];
} ATA_PASS_THROUGH_EX, *PATA_PASS_THROUGH_EX;

typedef VOID
(VEAPI IO_WORKITEM_ROUTINE)(
  _In_ PDEVICE_OBJECT DeviceObject,
  _In_opt_ PVOID Context);
typedef IO_WORKITEM_ROUTINE *PIO_WORKITEM_ROUTINE;

typedef struct _IO_WORKITEM
{
    UL_WORK_QUEUE_ITEM Item;
    PDEVICE_OBJECT DeviceObject;
    PIO_WORKITEM_ROUTINE WorkerRoutine;
    PVOID Context;
} IO_WORKITEM, *PIO_WORKITEM;

typedef enum _DISK_STYLE
{
    DiskStyleMbr = 0,
    DiskStyleGpt
} DISK_STYLE, *PDISK_STYLE;

typedef DISK_STYLE DISK_PARTITION_STYLE;

typedef struct _VPB
{
    USHORT Type;
    USHORT Size;
    USHORT Flags;
    USHORT VolumeLabelLength;
    struct _DEVICE_OBJECT *DeviceObject;
    struct _DEVICE_OBJECT *RealDevice;
    ULONG SerialNumber;
    ULONG ReferenceCount;
    CHAR VolumeLabel[32];
} VPB, *PVPB;

typedef struct _FILE_OBJECT
{
    SHORT           Type;
    SHORT           Size;
    PDEVICE_OBJECT  DeviceObject;;
    PVPB            Vpb;
    PVOID           FsContext, FsContext2;
    VEASTATUS       FinalStatus;
    struct _FILE_OBJECT* RelatedFileObject;
    BOOLEAN         LockOperation;
    BOOLEAN         LockPending;
    BOOLEAN         ReadAccess;
    BOOLEAN         WriteAccess;
    BOOLEAN         DeleteAccess;
    BOOLEAN         SharedRead;
    BOOLEAN         SharedWrite;
    BOOLEAN         SharedDelete;
    ULONG           Flags;
    ANSI_STRING     FileName;
    LARGE_INTEGER   CurrentByteOffset;
    volatile ULONG  Waiters;
    volatile ULONG  Busy;
    PVOID           LastLock;
    KEVENT          Lock;
    KEVENT          Event;
    KSPIN_LOCK      OipListLock;
    LIST_ENTRY      OipList;
    volatile PVOID  FileObjectExtension;
} FILE_OBJECT, *PFILE_OBJECT;

#define PartitionTypeMbr DiskStyleMbr
#define PartitionTypeGpt DiskStyleGpt

typedef struct _FILE_BASIC_INFORMATION {
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    ULONG FileAttributes;
} FILE_BASIC_INFORMATION, *PFILE_BASIC_INFORMATION;

typedef enum _CREATE_FILE_TYPE {
    CreateFileTypeNone,
    CreateFileTypeNamedPipe,
    CreateFileTypeMailslot
} CREATE_FILE_TYPE;

typedef struct _DUMMY_FILE_OBJECT
{
    OBJECT_HEADER ObjectHeader;
    CHAR FileObjectBody[sizeof(FILE_OBJECT)];
} DUMMY_FILE_OBJECT, *PDUMMY_FILE_OBJECT;

typedef struct _FILE_NETWORK_OPEN_INFORMATION {
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    LARGE_INTEGER AllocationSize;
    LARGE_INTEGER EndOfFile;
    ULONG FileAttributes;
} FILE_NETWORK_OPEN_INFORMATION, *PFILE_NETWORK_OPEN_INFORMATION;

typedef struct _OPEN_PACKET
{
    USHORT Type;
    USHORT Size;
    PFILE_OBJECT FileObject;
    VEASTATUS FinalStatus;
    ULONG_PTR Information;
    ULONG ParseCheck;
    PFILE_OBJECT RelatedFileObject;
    PDEVICE_OBJECT OriginalAttributes;
    LARGE_INTEGER AllocationSize;
    ULONG CreateOptions;
    USHORT FileAttributes;
    USHORT ShareAccess;
    PVOID EaBuffer;
    ULONG EaLength;
    ULONG Options;
    ULONG Disposition;
    PFILE_BASIC_INFORMATION FileBasicInformation;
    PFILE_NETWORK_OPEN_INFORMATION NetworkInformation;
    CREATE_FILE_TYPE CreateFileType;
    PVOID ExtraCreateParameters;
    BOOLEAN Override;
    BOOLEAN QueryOnly;
    BOOLEAN DeleteOnly;
    BOOLEAN FullAttributes;
    PDUMMY_FILE_OBJECT LocalFileObject;
    BOOLEAN TraversedMountPoint;
    ULONG InternalFlags;
    PDEVICE_OBJECT TopDeviceObjectHint;
} OPEN_PACKET, *POPEN_PACKET;

#define IO_TYPE_ADAPTER                 1
#define IO_TYPE_CONTROLLER              2
#define IO_TYPE_DEVICE                  3
#define IO_TYPE_DRIVER                  4
#define IO_TYPE_FILE                    5
#define IO_TYPE_IRP                     6
#define IO_TYPE_MASTER_ADAPTER          7
#define IO_TYPE_OPEN_PACKET             8
#define IO_TYPE_TIMER                   9
#define IO_TYPE_VPB                     10
#define IO_TYPE_ERROR_LOG               11
#define IO_TYPE_ERROR_MESSAGE           12
#define IO_TYPE_DEVICE_OBJECT_EXTENSION 13

#define OBIO_MAX_REPARSE_TRAVERSAL 0x20

extern POBJECT_TYPE ObpFileObjectType;
extern LIST_ENTRY ObioFsDiskListHead, ObioFsCdRomListHead, ObioFsNetworkListHead;

#define VPB_MOUNTED 0x0001
#define VPB_LOCKED  0x0002
#define VPB_SOURCE_READ_ONLY 0x0004
#define VPB_RAW_MOUNT 0x0008
#define VPB_REMOVE_PENDING 0x0010
#define VPB_DIRECT_WRITES_ALLOWED 0x0020

/* CreateDisposition */
#define FILE_SUPERSEDE                    0x00000000
#define FILE_OPEN                         0x00000001
#define FILE_CREATE                       0x00000002
#define FILE_OPEN_IF                      0x00000003
#define FILE_OVERWRITE                    0x00000004
#define FILE_OVERWRITE_IF                 0x00000005
#define FILE_MAXIMUM_DISPOSITION          0x00000005\

/* CreateOptions Flag */
#define FILE_RESERVE_OPFILTER             0x00100000
#define FILE_OPEN_REPARSE_POINT           0x00200000
#define FILE_OPEN_NO_RECALL               0x00400000
#define FILE_OPEN_FOR_FREE_SPACE_QUERY    0x00800000
#define FILE_OPEN_REQUIRING_OPLOCK        0x00010000
#define FILE_DISALLOW_EXCLUSIVE           0x00020000
#define FILE_DIRECTORY_FILE               0x00000001
#define FILE_WRITE_THROUGH                0x00000002
#define FILE_SEQUENTIAL_ONLY              0x00000004
#define FILE_NO_INTERMEDIATE_BUFFERING    0x00000008
#define FILE_SYNCHRONOUS_IO_ALERT         0x00000010
#define FILE_SYNCHRONOUS_IO_NONALERT      0x00000020
#define FILE_NON_DIRECTORY_FILE           0x00000040
#define FILE_CREATE_TREE_CONNECTION       0x00000080
#define FILE_COMPLETE_IF_OPLOCKED         0x00000100
#define FILE_NO_EA_KNOWLEDGE              0x00000200
#define FILE_OPEN_REMOTE_INSTANCE         0x00000400
#define FILE_RANDOM_ACCESS                0x00000800
#define FILE_DELETE_ON_CLOSE              0x00001000
#define FILE_OPEN_BY_FILE_ID              0x00002000
#define FILE_OPEN_FOR_BACKUP_INTENT       0x00004000
#define FILE_NO_COMPRESSION               0x00008000

/* ntifs */
#define FILE_CASE_SENSITIVE_SEARCH          0x00000001
#define FILE_CASE_PRESERVED_NAMES           0x00000002
#define FILE_UNICODE_ON_DISK                0x00000004
#define FILE_PERSISTENT_ACLS                0x00000008
#define FILE_FILE_COMPRESSION               0x00000010
#define FILE_VOLUME_QUOTAS                  0x00000020
#define FILE_SUPPORTS_SPARSE_FILES          0x00000040
#define FILE_SUPPORTS_REPARSE_POINTS        0x00000080
#define FILE_SUPPORTS_REMOTE_STORAGE        0x00000100
#define FILE_VOLUME_IS_COMPRESSED           0x00008000
#define FILE_SUPPORTS_OBJECT_IDS            0x00010000
#define FILE_SUPPORTS_ENCRYPTION            0x00020000
#define FILE_NAMED_STREAMS                  0x00040000
#define FILE_READ_ONLY_VOLUME               0x00080000
#define FILE_SEQUENTIAL_WRITE_ONCE          0x00100000
#define FILE_SUPPORTS_TRANSACTIONS          0x00200000
#define FILE_SUPPORTS_HARD_LINKS            0x00400000
#define FILE_SUPPORTS_EXTENDED_ATTRIBUTES   0x00800000
#define FILE_SUPPORTS_OPEN_BY_FILE_ID       0x01000000
#define FILE_SUPPORTS_USN_JOURNAL           0x02000000

/* AttributeOptions flag */
#define FILE_LIST_DIRECTORY       0x00000001  /* direktori */
#define FILE_READ_DATA            0x00000001  /* file */
#define FILE_ADD_FILE             0x00000002  /* direktori */
#define FILE_WRITE_DATA           0x00000002  /* file */
#define FILE_ADD_SUBDIRECTORY     0x00000004  /* direktori */
#define FILE_APPEND_DATA          0x00000004  /* file */
#define FILE_CREATE_PIPE_INSTANCE 0x00000004  /* named pipe */
#define FILE_READ_EA              0x00000008
#define FILE_WRITE_EA             0x00000010
#define FILE_EXECUTE              0x00000020  /* file */
#define FILE_TRAVERSE             0x00000020  /* direktori */
#define FILE_DELETE_CHILD         0x00000040  /* direktori */
#define FILE_READ_ATTRIBUTES      0x00000080
#define FILE_WRITE_ATTRIBUTES     0x00000100

/* FileAttribute flag */
#define FILE_ATTRIBUTE_READONLY           0x00000001
#define FILE_ATTRIBUTE_HIDDEN             0x00000002
#define FILE_ATTRIBUTE_SYSTEM             0x00000004
#define FILE_ATTRIBUTE_DIRECTORY          0x00000010
#define FILE_ATTRIBUTE_ARCHIVE            0x00000020
#define FILE_ATTRIBUTE_DEVICE             0x00000040
#define FILE_ATTRIBUTE_NORMAL             0x00000080
#define FILE_ATTRIBUTE_TEMPORARY          0x00000100
#define FILE_ATTRIBUTE_SPARSE_FILE        0x00000200
#define FILE_ATTRIBUTE_REPARSE_POINT      0x00000400
#define FILE_ATTRIBUTE_COMPRESSED         0x00000800
#define FILE_ATTRIBUTE_OFFLINE            0x00001000
#define FILE_ATTRIBUTE_NOT_CONTENT_INDEXED 0x00002000
#define FILE_ATTRIBUTE_ENCRYPTED          0x00004000
#define FILE_ATTRIBUTE_INTEGRITY_STREAM   0x00008000
#define FILE_ATTRIBUTE_VIRTUAL            0x00010000

#define FILE_ALL_ACCESS \
  (VEA_STANDARD_RIGHTS_REQUIRED | VEA_SYNCHRONIZE | 0x1FF)

#define FILE_GENERIC_EXECUTE \
  (VEA_STANDARD_RIGHTS_EXECUTE | FILE_READ_ATTRIBUTES | FILE_EXECUTE | VEA_SYNCHRONIZE)

#define FILE_GENERIC_READ \
  (VEA_STANDARD_RIGHTS_READ | FILE_READ_DATA | FILE_READ_ATTRIBUTES | \
   FILE_READ_EA | VEA_SYNCHRONIZE)

#define FILE_GENERIC_WRITE \
  (VEA_STANDARD_RIGHTS_WRITE | FILE_WRITE_DATA | FILE_WRITE_ATTRIBUTES | \
   FILE_WRITE_EA | FILE_APPEND_DATA | VEA_SYNCHRONIZE)

typedef struct _FILE_FULL_EA_INFORMATION {
    ULONG NextEntryOffset;
    UCHAR Flags;
    UCHAR EaNameLength;
    USHORT EaValueLength;
    CHAR EaName[1];
} FILE_FULL_EA_INFORMATION, *PFILE_FULL_EA_INFORMATION;

typedef struct _FILE_GET_EA_INFORMATION {
    ULONG NextEntryOffset;
    UCHAR EaNameLength;
    CHAR  EaName[1];
} FILE_GET_EA_INFORMATION, *PFILE_GET_EA_INFORMATION;

typedef enum _PLUGPLAY_EVENT_CATEGORY
{
    HardwareProfileChangeEvent,
    TargetDeviceChangeEvent,
    DeviceClassChangeEvent,
    CustomDeviceEvent,
    DeviceInstallEvent,
    DeviceArrivalEvent,
    PowerEvent,
    VetoEvent,
    BlockedDriverEvent,
    MaxPlugEventCategory
} PLUGPLAY_EVENT_CATEGORY;

typedef enum _PNP_VETO_TYPE {
    PNP_VetoTypeUnknown,
    PNP_VetoLegacyDevice,
    PNP_VetoPendingClose,
    PNP_VetoWindowsApp,
    PNP_VetoWindowsService,
    PNP_VetoOutstandingOpen,
    PNP_VetoDevice,
    PNP_VetoDriver,
    PNP_VetoIllegalDeviceRequest,
    PNP_VetoInsufficientPower,
    PNP_VetoNonDisableable,
    PNP_VetoLegacyDriver,
    PNP_VetoInsufficientRights,
} PNP_VETO_TYPE, *PPNP_VETO_TYPE;

typedef struct _PLUGPLAY_EVENT_BLOCK
{
    GUID EventGuid;
    PLUGPLAY_EVENT_CATEGORY EventCategory;
    PULONG Result;
    ULONG Flags;
    ULONG TotalSize;
    PVOID DeviceObject;
    union
    {
        struct
        {
            GUID ClassGuid;
            WCHAR SymbolicLinkName[ANYSIZE_ARRAY];
        } DeviceClass;
        struct
        {
            WCHAR DeviceIds[ANYSIZE_ARRAY];
        } TargetDevice;
        struct
        {
            WCHAR DeviceId[ANYSIZE_ARRAY];
        } InstallDevice;
        struct
        {
            PVOID NotificationStructure;
            WCHAR DeviceIds[ANYSIZE_ARRAY];
        } CustomNotification;
        struct
        {
            PVOID Notification;
        } ProfileNotification;
        struct
        {
            ULONG NotificationCode;
            ULONG NotificationData;
        } PowerNotification;
        struct
        {
            PNP_VETO_TYPE VetoType;
            WCHAR DeviceIdVetoNameBuffer[ANYSIZE_ARRAY];
        } VetoNotification;
        struct
        {
            GUID BlockedDriverGuid;
        } BlockedDriverNotification;
    };
} PLUGPLAY_EVENT_BLOCK, *PPLUGPLAY_EVENT_BLOCK;

extern PHANDLE_TABLE ObpKernelHandleTable;

#define KERNEL_HANDLE_FLAG            0x80000000
#define OB_KERNEL_HANDLE_TABLE_SIZE   1024

#define OB_FLAG_PERMANENT         0x04
#define OB_FLAG_KERNEL_OBJECT     0x08

typedef ULONG
(VEAPI GET_SET_DEVICE_DATA)
(
    IN OUT PVOID Context,
    IN ULONG DataType,
    IN OUT PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
);
typedef GET_SET_DEVICE_DATA *PGET_SET_DEVICE_DATA;

typedef VOID
(VEAPI REFERENCE_INTERFACE)
(
    IN PVOID Context
);
typedef REFERENCE_INTERFACE *PREFERENCE_INTERFACE;
typedef VOID
(VEAPI DEREFERENCE_INTERFACE)
(
    IN PVOID Context
);
typedef DEREFERENCE_INTERFACE *PDEREFERENCE_INTERFACE;

// Standard BUS_INTERFACE
typedef struct _BUS_INTERFACE_STANDARD
{
    USHORT Size;
    USHORT Version;
    PVOID Context;
    PREFERENCE_INTERFACE InterfaceReference;
    PDEREFERENCE_INTERFACE InterfaceDereference;
    PGET_SET_DEVICE_DATA SetBusData;
    PGET_SET_DEVICE_DATA GetBusData;
} BUS_INTERFACE_STANDARD, *PBUS_INTERFACE_STANDARD;

extern KSPIN_LOCK ObioTimerLock;
extern LIST_ENTRY ObioTimerQueueHead;
extern KDPC ObioTimerDpc;
extern KTIMER ObioTimer;

typedef struct _DEVICE_TRIM_DESCRIPTOR {
    ULONG Version;
    ULONG Size;
    BOOLEAN TrimEnabled;
} DEVICE_TRIM_DESCRIPTOR, *PDEVICE_TRIM_DESCRIPTOR;

typedef ULONG DEVICE_DATA_MANAGEMENT_SET_ACTION;

typedef struct _DEVICE_DATA_SET_RANGE {
    LONGLONG StartingOffset;
    ULONGLONG LengthInBytes;
} DEVICE_DATA_SET_RANGE, *PDEVICE_DATA_SET_RANGE;

typedef struct _DEVICE_MANAGE_DATA_SET_ATTRIBUTES {
    ULONG Size;
    DEVICE_DATA_MANAGEMENT_SET_ACTION Action;
    ULONG Flags;
    ULONG ParameterBlockOffset;
    ULONG ParameterBlockLength;
    ULONG DataSetRangesOffset;
    ULONG DataSetRangesLength;
} DEVICE_MANAGE_DATA_SET_ATTRIBUTES, *PDEVICE_MANAGE_DATA_SET_ATTRIBUTES;

typedef struct _DEVICE_DSM_NOTIFICATION_PARAMETERS {
    ULONG Size;
    ULONG Flags;
    ULONG NumFileTypeIDs;
    GUID FileTypeID[1];
} DEVICE_DSM_NOTIFICATION_PARAMETERS, *PDEVICE_DSM_NOTIFICATION_PARAMETERS;

#define DeviceDsmActionFlag_NonDestructive  0x80000000
#define IsDsmActionNonDestructive(_Action) ((BOOLEAN)((_Action & DeviceDsmActionFlag_NonDestructive) != 0))

#define DeviceDsmAction_None                    0x0u
#define DeviceDsmAction_Trim                    0x1u
#define DeviceDsmAction_Notification            (0x00000002u | DeviceDsmActionFlag_NonDestructive)
#define DeviceDsmAction_OffloadRead             (0x00000003u | DeviceDsmActionFlag_NonDestructive)
#define DeviceDsmAction_OffloadWrite            (0x00000004u)
#define DeviceDsmAction_Allocation              (0x00000005u | DeviceDsmActionFlag_NonDestructive)
#define DeviceDsmAction_Repair                  (0x00000006u | DeviceDsmActionFlag_NonDestructive)
#define DeviceDsmAction_Scrub                   (0x00000007u | DeviceDsmActionFlag_NonDestructive)
#define DeviceDsmAction_DrtQuery                (0x00000008u | DeviceDsmActionFlag_NonDestructive)
#define DeviceDsmAction_DrtClear                (0x00000009u | DeviceDsmActionFlag_NonDestructive)
#define DeviceDsmAction_DrtDisable              (0x0000000Au | DeviceDsmActionFlag_NonDestructive)
#define DeviceDsmAction_TieringQuery            (0x0000000Bu | DeviceDsmActionFlag_NonDestructive)
#define DeviceDsmAction_Map                     (0x0000000Cu | DeviceDsmActionFlag_NonDestructive)
#define DeviceDsmAction_RegenerateParity        (0x0000000Du | DeviceDsmActionFlag_NonDestructive)
#define DeviceDsmAction_NvCache_Change_Priority (0x0000000Eu | DeviceDsmActionFlag_NonDestructive)
#define DeviceDsmAction_NvCache_Evict           (0x0000000Fu | DeviceDsmActionFlag_NonDestructive)
#define DeviceDsmAction_TopologyIdQuery         (0x00000010u | DeviceDsmActionFlag_NonDestructive)
#define DeviceDsmAction_GetPhysicalAddresses    (0x00000011u | DeviceDsmActionFlag_NonDestructive)
#define DeviceDsmAction_ScopeRegen              (0x00000012u | DeviceDsmActionFlag_NonDestructive)
#define DeviceDsmAction_ReportZones             (0x00000013u | DeviceDsmActionFlag_NonDestructive)
#define DeviceDsmAction_OpenZone                (0x00000014u | DeviceDsmActionFlag_NonDestructive)
#define DeviceDsmAction_FinishZone              (0x00000015u | DeviceDsmActionFlag_NonDestructive)
#define DeviceDsmAction_CloseZone               (0x00000016u | DeviceDsmActionFlag_NonDestructive)
#define DeviceDsmAction_ResetWritePointer       (0x00000017u)
#define DeviceDsmAction_GetRangeErrorInfo       (0x00000018u | DeviceDsmActionFlag_NonDestructive)
#define DeviceDsmAction_WriteZeroes             (0x00000019u)
#define DeviceDsmAction_LostQuery               (0x0000001Au | DeviceDsmActionFlag_NonDestructive)
#define DeviceDsmAction_GetFreeSpace            (0x0000001Bu | DeviceDsmActionFlag_NonDestructive)
#define DeviceDsmAction_ConversionQuery         (0x0000001Cu | DeviceDsmActionFlag_NonDestructive)
#define DeviceDsmAction_VdtSet                  (0x0000001Du)

#define DEVICE_DSM_FLAG_ENTIRE_DATA_SET_RANGE    0x00000001

#define DEVICE_DSM_NOTIFY_FLAG_BEGIN             0x00000001
#define DEVICE_DSM_NOTIFY_FLAG_END               0x00000002

typedef struct _LOAD_UNLOAD_PARAMS
{
    VEASTATUS Status;
    PANSI_STRING RegistryPath;
    UL_WORK_QUEUE_ITEM WorkItem;
    KEVENT Event;
    PDRIVER_OBJECT DriverObject;
    BOOLEAN SetEvent;
} LOAD_UNLOAD_PARAMS, *PLOAD_UNLOAD_PARAMS;

#define SYSTEMROOT_VAR     "%SYSTEMROOT%"
#define SYSTEMROOT_VAR_LEN 12
#define SYSTEMROOT_TARGET  "/SystemRoot"
#define SYSTEMROOT_TGT_LEN 11

/* EOF */