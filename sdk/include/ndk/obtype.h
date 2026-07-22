#pragma once

#include "procbind.h"
#include "veastatus.h"
#include "ks.h"

// special
typedef enum _KPROCESSOR_MODE {
    KernelMode,
    UserMode
} KPROCESSOR_MODE;

/* Major Function */
#define OIP_MJ_CREATE          0x01
#define OIP_MJ_CLOSE           0x02
#define OIP_MJ_READ            0x03
#define OIP_MJ_WRITE           0x04
#define OIP_MJ_DEVICE_CONTROL  0x05
#define OIP_MJ_INTERNAL_CTL    0x06
#define OIP_MJ_POWER           0x07
#define OIP_MJ_PNP             0x08

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

    // Callback Procedures
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
    ULONG TotalObjectCount;
    ULONG Flags;
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
    
    // --- FIX 1: Routing Rantai Dua Arah ---
    UCHAR StackDepth;              // Total layer ke bawah (Buat OS alokasi array OIP_STACK)
    PVOID LowerAttachedObject;     // Pointer ke objek di Bawah (A -> B)
    PVOID UpperAttachedObject;     // Pointer ke objek di Atas  (C -> B, B -> A)
    
    PVOID SeAddon;
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
} OB_LOOKUP_CONTEXT, *POB_LOOKUP_CONTEXT;

typedef struct _DEVICE_OBJECT DEVICE_OBJECT, *PDEVICE_OBJECT;
typedef struct _DRIVER_OBJECT DRIVER_OBJECT, *PDRIVER_OBJECT;

typedef VEASTATUS (VEAPI *PDRIVER_INITIALIZE)(
    PDRIVER_OBJECT DriverObject
);

typedef BOOLEAN (VEAPI *POBIO_INTERRUPT_HANDLER)(
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
