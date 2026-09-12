#pragma once

#include "procbind.h"
#include "veastatus.h"
#include "ks.h"
#include "sffunc.h"

// special
typedef enum _KPROCESSOR_MODE {
    KernelMode,
    UserMode
} KPROCESSOR_MODE;

/* =========================================================
 * Major Function (OIP_MJ)
 * ========================================================= */
#define OIP_MJ_CREATE               0x01 // Buka/handle baru ke objek (file, device, dll)
#define OIP_MJ_CLOSE                0x02 // Handle ditutup
#define OIP_MJ_READ                 0x03 // Baca data
#define OIP_MJ_WRITE                0x04 // Tulis data
#define OIP_MJ_DEVICE_CONTROL       0x05 // IOCTL ke device (setara DeviceIoControl)
#define OIP_MJ_INTERNAL_CTL         0x06 // IOCTL internal antar-driver (kernel-mode only)
#define OIP_MJ_POWER                0x07 // Request terkait power management
#define OIP_MJ_PNP                  0x08 // Request terkait Plug and Play
 
#define OIP_MJ_QUERY_INFORMATION    0x09 // "Kasih tau info soal objek ini" (size, attribute, dll)
#define OIP_MJ_SET_INFORMATION      0x0A // "Ubah info objek ini"
#define OIP_MJ_FLUSH_BUFFERS        0x0B // Paksa flush cache/buffer ke storage asli
#define OIP_MJ_CLEANUP              0x0C // Handle count ke-0 tapi objek belum di-close penuh
#define OIP_MJ_SHUTDOWN              0x0D // OS mau shutdown, driver sempet beres-beres dulu
#define OIP_MJ_SYSTEM_CONTROL       0x0E // Query/set system-level info (mirip WMI di NT) 
#define OIP_MJ_MAXIMUM_FUNCTION      0x0E

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
 
/* =========================================================
 * Power Minor Functions (Dipakai saat Major = OIP_MJ_POWER)
 * ========================================================= */
#define OIP_MN_WAIT_WAKE                    0x00 // OS nyuruh hardware nunggu sinyal buat bangunin PC
#define OIP_MN_POWER_SEQUENCE                0x01 // Optimalisasi power state
#define OIP_MN_SET_POWER                    0x02 // OS nyuruh: "Ganti ke mode Sleep (D3) atau Nyala (D0)"
#define OIP_MN_QUERY_POWER                  0x03 // OS nanya: "Aman nggak kalau sekarang lu gw suruh Sleep?"

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
            PVOID SecurityContext; // Pointer ke struct security lu (nanti)
        } Create;

        // Generic arguments (buat custom operasi yang ga masuk kategori atas)
        struct {
            PVOID Argument1;
            PVOID Argument2;
            PVOID Argument3;
            PVOID Argument4;
        } Others;
    } Parameters;

    // Target object untuk "layer" stack ini
    PVOID TargetObject;        // Pengganti DeviceObject di NT
    PVOID FileObject;          // Konteks file jika operasi ini terkait file handle terbuka
    
    // Callback kalau operasi di layer ini udah selesai
    POIP_COMPLETION_ROUTINE CompletionRoutine;   // PIO_COMPLETION_ROUTINE
    PVOID Context;             // Konteks untuk completion routine

} OIP_STACK, *POIP_STACK;

typedef struct _OIP {
    ULONG Size;                // Ukuran total memori paket ini (Header + Stacks)
    ULONG StackCount;          // Total jumlah layer stack yang dialokasikan
    volatile LONG CurrentLocation; // Indeks stack yang sedang aktif (dimulai dari StackCount, mundur ke 0)
    
    // Status keseluruhan paket
    volatile LONG IoStatus;         // STATUS_SUCCESS, STATUS_PENDING, dll
    ULONG Information;         // Berapa byte yang sukses diproses (Return Length)
    
    // Buffer Data Utama (Direct/Buffered I/O)
    PVOID UserBuffer;          // Buffer asli dari user
    PVOID SystemBuffer;        // Buffer aman yang udah di-copy ke kernel
    ULONG UserTag, SystemTag;
    
    // Pointer ke stack berjalan
    POIP_STACK CurrentStackLocation; 

    UCHAR RequestorMode;
    ULONG Flags;

    BOOLEAN Cancel;                   // Flag kalau paket ini minta dibatalkan
    PVOID CancelRoutine;              // Fungsi callback buat driver ngeberhentiin hardware

    PKEVENT UserEvent;
    PVEASTATUS UserIosb;
} OIP, *POIP;

typedef LONG (VEAPI *POB_INTERRUPT_ROUTINE)(
    PVOID Object, 
    POIP Packet
);

typedef LONG (VEAPI *POB_CREATE_ROUTINE)(
    PVOID Object
);

typedef VOID (VEAPI *POB_DELETE_ROUTINE)(
    PVOID Object
);

typedef struct _OB_LOOKUP_CONTEXT OB_LOOKUP_CONTEXT, *POB_LOOKUP_CONTEXT;

typedef LONG (VEAPI *POB_PARSE_ROUTINE)(
    PVOID ParseObject,
    POB_LOOKUP_CONTEXT LookupContext,
    PVOID *ResolvedObject  // [OUT] Objek hasil akhir (kalau ketemu)
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
    POB_CREATE_ROUTINE    CreateRoutine;
    POB_DELETE_ROUTINE    DeleteRoutine;    
    POB_PARSE_ROUTINE     ParseRoutine;
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
    POB_CREATE_ROUTINE    CreateRoutine;
    POB_DELETE_ROUTINE    DeleteRoutine;
    POB_PARSE_ROUTINE     ParseRoutine;
} OBJECT_TYPE, *POBJECT_TYPE;

typedef struct _OBJECT_HEADER
{
    LONG_PTR ReferenceCount;
    POBJECT_TYPE Type;
    UCHAR Flags;
    PVEA_SECURITY_DESCRIPTOR SecurityDescriptor;
    UCHAR StackDepth;              // Total layer ke bawah (Buat OS alokasi array OIP_STACK)
    PVOID LowerAttachedObject;     // Pointer ke objek di Bawah (A -> B)
    PVOID UpperAttachedObject;     // Pointer ke objek di Atas  (C -> B, B -> A)
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
#define OB_FLAG_HAS_NAME_INFO 0x0021

typedef struct _OBJECT_DIRECTORY_ENTRY {
    LIST_ENTRY Chain;       // Buat disambungin ke OBJECT_DIRECTORY->Head
    PVOID Object;           // Pointer ke badan objeknya (misal objek Device)
} OBJECT_DIRECTORY_ENTRY, *POBJECT_DIRECTORY_ENTRY;

/* Symbolic link */
typedef struct _OBJECT_SYMBOLIC_LINK {
    ANSI_STRING LinkTarget; // Menggantikan CHAR LinkTarget[256]
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

typedef struct _OBJECT_ATTRIBUTES {
    ULONG Length;
    PVOID RootDirectory;        // Pointer ke objek direktori tempat path ini bermula (Bisa NULL)
    PANSI_STRING ObjectName;    // String nama path (misal: "Device/MyDisk")
    ULONG Attributes;           // Flags (OBJ_CASE_INSENSITIVE, dll)
    PVOID SecurityDescriptor;   // Konteks keamanan (sementara bisa diset NULL)
    PVOID SecurityQualityOfServiceLife;
} OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;

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
    PVOID RootDirectory;
    POBJECT_TYPE ExpectedType;
    ANSI_STRING RemainingName;  // Menggunakan ANSI_STRING (pointer + length)
    ULONG ReparseCount;
    BOOLEAN CaseInsensitive;
    CHAR ReparseBuffer[512];    // Reparse buffer aman di dalam Context (bukan stack lokal)
    BOOLEAN InsertMode;         // TRUE = Berhenti di parent directory buat persiapan ObInsertObject
    PVOID LockObject;           // Placeholder: Nanti diisi pointer ke Spinlock/Mutex direktori
    KPROCESSOR_MODE AccessMode; // Track siapa yang manggil: Kernel atau User-space
    ACCESS_MASK DesiredAccess;  // What access is allowed in this system?
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

// Struktur DRIVER_OBJECT
typedef struct _DRIVER_OBJECT {
    PVOID DriverStart;          // Pointer ke base address driver di memory (opsional)
    PDEVICE_OBJECT DeviceObject;// Pointer ke Device pertama yang dibuat oleh driver ini
    PVOID DriverExtension;      // Data tambahan spesifik driver
    POBIO_INTERRUPT_HANDLER InterruptHandler;
} DRIVER_OBJECT, *PDRIVER_OBJECT;

// Struktur DEVICE_OBJECT
typedef struct _DEVICE_OBJECT {
    PDRIVER_OBJECT DriverObject;// Driver mana yang memiliki device ini
    PDEVICE_OBJECT NextDevice;  // Device berikutnya (jika 1 driver punya banyak device)
    ULONG DeviceType;           // Jenis device (misal: FILE_DEVICE_SERIAL_PORT)
    ULONG Characteristics;      // Karakteristik (misal: FILE_DEVICE_SECURE_OPEN)
    PVOID DeviceExtension;      // Pointer ke memori rahasia/private milik device ini (opsional)
    ULONG Flags; 
    POBIO_INTERRUPT_HANDLER InterruptHandler;
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

//
// File Device Type
//
#define FILE_DEVICE_BEEP                  0x00000001
#define FILE_DEVICE_CD_ROM                0x00000002
#define FILE_DEVICE_CD_ROM_FILE_SYSTEM    0x00000003
#define FILE_DEVICE_CONTROLLER            0x00000004
#define FILE_DEVICE_DATALINK              0x00000005
#define FILE_DEVICE_DFS                   0x00000006
#define FILE_DEVICE_DISK                  0x00000007
#define FILE_DEVICE_DISK_FILE_SYSTEM      0x00000008
#define FILE_DEVICE_FILE_SYSTEM           0x00000009
#define FILE_DEVICE_INPORT_PORT           0x0000000a
#define FILE_DEVICE_KEYBOARD              0x0000000b
#define FILE_DEVICE_MAILSLOT              0x0000000c
#define FILE_DEVICE_MIDI_IN               0x0000000d
#define FILE_DEVICE_MIDI_OUT              0x0000000e
#define FILE_DEVICE_MOUSE                 0x0000000f
#define FILE_DEVICE_MULTI_UNC_PROVIDER    0x00000010
#define FILE_DEVICE_NAMED_PIPE            0x00000011
#define FILE_DEVICE_NETWORK               0x00000012
#define FILE_DEVICE_NETWORK_BROWSER       0x00000013
#define FILE_DEVICE_NETWORK_FILE_SYSTEM   0x00000014
#define FILE_DEVICE_NULL                  0x00000015
#define FILE_DEVICE_PARALLEL_PORT         0x00000016
#define FILE_DEVICE_PHYSICAL_NETCARD      0x00000017
#define FILE_DEVICE_PRINTER               0x00000018
#define FILE_DEVICE_SCANNER               0x00000019
#define FILE_DEVICE_SERIAL_MOUSE_PORT     0x0000001a
#define FILE_DEVICE_SERIAL_PORT           0x0000001b
#define FILE_DEVICE_SCREEN                0x0000001c
#define FILE_DEVICE_SOUND                 0x0000001d
#define FILE_DEVICE_STREAMS               0x0000001e
#define FILE_DEVICE_TAPE                  0x0000001f
#define FILE_DEVICE_TAPE_FILE_SYSTEM      0x00000020
#define FILE_DEVICE_TRANSPORT             0x00000021
#define FILE_DEVICE_UNKNOWN               0x00000022
#define FILE_DEVICE_VIDEO                 0x00000023
#define FILE_DEVICE_VIRTUAL_DISK          0x00000024
#define FILE_DEVICE_WAVE_IN               0x00000025
#define FILE_DEVICE_WAVE_OUT              0x00000026
#define FILE_DEVICE_8042_PORT             0x00000027
#define FILE_DEVICE_NETWORK_REDIRECTOR    0x00000028
#define FILE_DEVICE_BATTERY               0x00000029
#define FILE_DEVICE_BUS_EXTENDER          0x0000002a
#define FILE_DEVICE_MODEM                 0x0000002b
#define FILE_DEVICE_VDM                   0x0000002c
#define FILE_DEVICE_MASS_STORAGE          0x0000002d
#define FILE_DEVICE_SMB                   0x0000002e
#define FILE_DEVICE_KS                    0x0000002f
#define FILE_DEVICE_CHANGER               0x00000030
#define FILE_DEVICE_SMARTCARD             0x00000031
#define FILE_DEVICE_ACPI                  0x00000032
#define FILE_DEVICE_DVD                   0x00000033
#define FILE_DEVICE_FULLSCREEN_VIDEO      0x00000034
#define FILE_DEVICE_DFS_FILE_SYSTEM       0x00000035
#define FILE_DEVICE_DFS_VOLUME            0x00000036
#define FILE_DEVICE_SERENUM               0x00000037
#define FILE_DEVICE_TERMSRV               0x00000038
#define FILE_DEVICE_KSEC                  0x00000039
#define FILE_DEVICE_FIPS                  0x0000003a
#define FILE_DEVICE_INFINIBAND            0x0000003b
#define FILE_DEVICE_VMBUS                 0x0000003e
#define FILE_DEVICE_CRYPT_PROVIDER        0x0000003f
#define FILE_DEVICE_WPD                   0x00000040
#define FILE_DEVICE_BLUETOOTH             0x00000041
#define FILE_DEVICE_MT_COMPOSITE          0x00000042
#define FILE_DEVICE_MT_TRANSPORT          0x00000043
#define FILE_DEVICE_BIOMETRIC             0x00000044
#define FILE_DEVICE_PMI                   0x00000045
#define FILE_DEVICE_EHSTOR                0x00000046
#define FILE_DEVICE_DEVAPI                0x00000047
#define FILE_DEVICE_GPIO                  0x00000048
#define FILE_DEVICE_USBEX                 0x00000049
#define FILE_DEVICE_CONSOLE               0x00000050
#define FILE_DEVICE_NFP                   0x00000051
#define FILE_DEVICE_SYSENV                0x00000052
#define FILE_DEVICE_VIRTUAL_BLOCK         0x00000053
#define FILE_DEVICE_POINT_OF_SERVICE      0x00000054
#define FILE_DEVICE_STORAGE_REPLICATION   0x00000055
#define FILE_DEVICE_TRUST_ENV             0x00000056
#define FILE_DEVICE_UCM                   0x00000057
#define FILE_DEVICE_UCMTCPCI              0x00000058
#define FILE_DEVICE_PERSISTENT_MEMORY     0x00000059
#define FILE_DEVICE_NVDIMM                0x0000005a
#define FILE_DEVICE_HOLOGRAPHIC           0x0000005b
#define FILE_DEVICE_SDFXHCI               0x0000005c
#define FILE_DEVICE_UCMUCSI               0x0000005d

//
// Device Node
//

typedef struct _DEVICE_NODE {
    PDEVICE_OBJECT PhysicalDeviceObject;
    struct _DEVICE_NODE *Parent;
    struct _DEVICE_NODE *Sibling;
    struct _DEVICE_NODE *Child;
    ANSI_STRING InstancePath;   // -> nyambung ke key Enum\...
    ULONG State;                // Started, Removed, dll
    ULONG Flags;
} DEVICE_NODE, *PDEVICE_NODE;

typedef enum _DEVNODE_STATE {
    DeviceNodeUninitialized = 0,   // baru dialokasi, belum diisi apapun
    DeviceNodeEnumerated,          // udah kedeteksi dari bus (ID, resource mentah)
    DeviceNodeResourceAssigned,    // resource (IRQ/port/mem) udah dialokasikan
    DeviceNodeStarted,             // driver-nya udah jalan & device siap dipake
    DeviceNodeStopped,             // dihentikan sementara (misal buat realokasi resource)
    DeviceNodeRemoved,             // device dicabut / gak ada lagi secara fisik
    DeviceNodeFailed               // gagal start, driver error, dst
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

//
// Bus QUERY
//
typedef enum _BUS_QUERY_ID_TYPE {
    BusQueryDeviceID = 0,     // Misal: "PCI\VEN_8086&DEV_2922"
    BusQueryHardwareIDs,      // Misal: "PCI\VEN_8086&DEV_2922&SUBSYS_00000000"
    BusQueryCompatibleIDs,    // Misal: "PCI\CC_010601" (Generic AHCI Controller)
    BusQueryInstanceID,       // Misal: "3&13c0b0c5&0"
    BusQueryDeviceSerialNumber
} BUS_QUERY_ID_TYPE, *PBUS_QUERY_ID_TYPE;

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

typedef struct _DEVICE_RELATIONS {
    ULONG Count;              // Jumlah child PDO yang ditemukan
    PDEVICE_OBJECT Objects[1];// Array dinamis pointer DEVICE_OBJECT (PDOs)
} DEVICE_RELATIONS, *PDEVICE_RELATIONS;

//
// Device POWER STATE
//
typedef enum _SYSTEM_POWER_STATE {
    PowerSystemUnspecified = 0,
    PowerSystemWorking,       // S0 (PC Nyala)
    PowerSystemSleeping1,     // S1
    PowerSystemSleeping2,     // S2
    PowerSystemSleeping3,     // S3 (RAM Suspend)
    PowerSystemHibernate,     // S4 (Disk Suspend)
    PowerSystemShutdown,      // S5 (Mati Total)
    PowerSystemMaximum
} SYSTEM_POWER_STATE, *PSYSTEM_POWER_STATE;

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