#pragma once

#include <stdint.h>
#include <procbind.h>

// Forward for KTHREAD KPROCESS KPRCB KIPCR KPCR
#include <../ndk/ks.h>
#include <../../../veakrnl/include/internal/x86.h>

typedef char CHAR;
typedef short SHORT;
typedef long LONG;

typedef unsigned long ULONG;
typedef unsigned char UCHAR;
typedef unsigned short USHORT;

#define __huge
#define __far

#pragma pack(push, 1)

//
// 1. SMBIOS 2.x Entry Point Structure (Magic: "_SM_")
//
typedef struct _SMBIOS_ENTRY_POINT {
    CHAR AnchorString[4];             // "_SM_"
    UCHAR Checksum;                   // Checksum seluruh EPS
    UCHAR Length;                     // Ukuran struktur ini (biasanya 0x1F)
    UCHAR MajorVersion;               // Versi SMBIOS (misal: 2)
    UCHAR MinorVersion;               // Versi SMBIOS (misal: 8)
    USHORT MaxStructureSize;          // Ukuran struktur terbesar di SMBIOS
    UCHAR EntryPointRevision;         // EPS Revision
    UCHAR FormattedArea[5];           // Reserved
    
    // Intermediate Anchor Header (DMI)
    CHAR IntermediateAnchorString[5]; // "_DMI_"
    UCHAR IntermediateChecksum;       // Checksum bagian DMI
    USHORT TableLength;               // Total panjang seluruh tabel SMBIOS (Bytes)
    ULONG TableAddress;               // Alamat Fisik Tabel SMBIOS di memori!
    USHORT NumberOfStructures;        // Jumlah total struktur/tabel SMBIOS
    UCHAR BcdRevision;                // BCD Revision
} SMBIOS_ENTRY_POINT, *PSMBIOS_ENTRY_POINT;

//
// 2. SMBIOS Header untuk Setiap Tabel (Type 0, Type 1, Type 4, dst)
//
typedef struct _SMBIOS_HEADER {
    UCHAR Type;                       // Type 0 = BIOS, Type 1 = System, Type 4 = CPU, dll.
    UCHAR Length;                     // Ukuran header + field terformat
    USHORT Handle;                    // Unique handle
} SMBIOS_HEADER, *PSMBIOS_HEADER;

#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    unsigned char  size;           // Harus selalu diisi 16 (0x10)
    unsigned char  reserved;       // Selalu 0
    unsigned short sectors;        // Berapa sektor yang mau dibaca?
    unsigned short buffer_offset;  // Ke offset memori mana data mau ditaruh?
    unsigned short buffer_segment; // Ke segment memori mana data mau ditaruh?
    unsigned long   lba_lower;      // Nomor LBA bawah (32-bit)
    unsigned long   lba_upper;      // Nomor LBA atas (32-bit) - biarin 0 aja buat sekarang
} DiskAddressPacket;

typedef struct {
    unsigned char  status;          // 0x80 = Bootable, 0x00 = Inactive
    unsigned char  chs_first[3];    // CHS awal (bisa diabaikan, kita pakai LBA)
    unsigned char  type;            // 0x0B atau 0x0C untuk FAT32
    unsigned char  chs_last[3];     // CHS akhir
    unsigned long  lba_start;       // PENTING: Di sektor berapa P1 dimulai?
    unsigned long  sector_count;    // Ukuran partisi dalam sektor
} MBR_PartitionEntry;

typedef enum _VEA_MEMORY_TYPE {
    VeaMemoryTypeUsable          = 1, // RAM kosong, siap dipakai kernel
    VeaMemoryTypeReserved        = 2, // Dipakai hardware/BIOS, JANGAN disentuh!
    VeaMemoryTypeAcpiReclaimable = 3, // Tabel ACPI (bisa dihapus kalau udah selesai dibaca)
    VeaMemoryTypeAcpiNvs         = 4, // ACPI Non-Volatile Storage (wajib dipertahankan)
    VeaMemoryTypeBadMemory       = 5  // Sektor RAM rusak
} VEA_MEMORY_TYPE;

typedef struct _VEA_MEMORY_DESCRIPTOR {
    unsigned long long BaseAddress;
    unsigned long long Length;
    unsigned int       Type;
    unsigned int       ACPIExtended;
} __attribute__((packed)) VEA_MEMORY_DESCRIPTOR, *PVEA_MEMORY_DESCRIPTOR;

typedef enum _VEA_MEMORY_EXTENDED_TYPE
{
    LoaderBad = 0,
    LoaderGood,
    LoaderOccupied,
    LoaderOldMemory,
    LoaderAcpiNVS,
    LoaderAcpiReclaimableMemory,
    LoaderAcpiOccupied,
    LoaderMAX
} VEA_MEMORY_EXTENDED_TYPE;

typedef struct _VEA_MEMORY_DESCRIPTOR_EX {
    VEA_MEMORY_DESCRIPTOR Descriptor;
    ULONG ExtendedType;
} __attribute__((packed)) VEA_MEMORY_DESCRIPTOR_EX, *PVEA_MEMORY_DESCRIPTOR_EX;

typedef struct _VIDEO_BOOT
{
    PVOID VideoBootAddress;
    ULONG X, Y, W, H;
    ULONG Sprite;
    PVOID AdditionalInformation[5];
} VIDEO_BOOT, *PVIDEO_BOOT;

typedef struct _BLOCK_BOOT_1
{
    ULONG Size;
    ULONG Version;
    VIDEO_BOOT VideoBoot;
    PVOID AcpiTable;
    ULONG AcpiTableByteSize;

    unsigned int MemoryMapCount;
    PVEA_MEMORY_DESCRIPTOR MemoryMap;
} BLOCK_BOOT_1, __huge *HPBLOCK_BOOT_1;



typedef enum _BOOT_DEVICE_TYPE {
    BootDeviceTypeUnknown = 0,
    BootDeviceTypeMbr,
    BootDeviceTypeGpt,
    BootDeviceTypeUefiPath
} BOOT_DEVICE_TYPE;

typedef struct _BOOT_DEVICE_IDENTIFIER {
    BOOT_DEVICE_TYPE Type;
    union {
        struct {
            ULONG DiskSignature;     // MBR 4-byte Signature
            ULONG PartitionNumber;   // Partition Index
        } Mbr;
        struct {
            GUID PartitionGuid;      // GPT Unique Partition GUID
        } Gpt;
        struct {
            PVOID DevicePathBuffer;  // Pointer ke raw EFI_DEVICE_PATH
            ULONG DevicePathLength;
        } Uefi;
    } u;
} BOOT_DEVICE_IDENTIFIER, *PBOOT_DEVICE_IDENTIFIER;

typedef struct _BOOT_DRIVER_LIST_ENTRY 
{
    LIST_ENTRY Link;              // Doubly-linked list pointer (Flink/Blink)
    ANSI_STRING FilePath;      // Path file, misal: L"\System32\Drivers\ahci.ko"
    ANSI_STRING RegistryPath;  // Path registry, misal: L"\Services\ahci"
    PVOID ImageBase;             // Alamat virtual .ko di RAM
    ULONG ImageSize;             // Ukuran file/memory .ko
    PVOID EntryPoint;            // Pointer ke EntryMain / DriverEntry
    ULONG Flags;                 // Status (LDRP_DRIVER_LOADED, dll)
} BOOT_DRIVER_LIST_ENTRY, *PBOOT_DRIVER_LIST_ENTRY;

typedef struct _BLOCK_BOOT_2
{
    ULONG Size;
    ULONG Version;
    ULONG OsMjVersion;
    ULONG OsMnVersion;
    VIDEO_BOOT VideoBoot;
    PVOID AcpiTable;
    ULONG AcpiTableByteSize;
    PSMBIOS_ENTRY_POINT SmbiosTable;
    UINT MemoryMapCount;
    PVEA_MEMORY_DESCRIPTOR MemoryMap;
    ULONG_PTR KernelStack;
    ULONG_PTR Prcb;
    ULONG_PTR Process;
    ULONG_PTR Thread;
    PVOID SystemHiveBase;
    ULONG SystemHiveLength;
    LIST_ENTRY BootDriverList;
    PSTR VeaBootArgument;
    PSTR VeaBootDriveOption;
    PVOID KernelBase;
    ULONG KernelSize;
    BOOT_DEVICE_IDENTIFIER BootPartitionSignature;
} BLOCK_BOOT_2, __huge *HPBLOCK_BOOT_2;

// Struktur data RSDP versi ACPI 1.0 (Ukuran pasti 20 byte)
typedef struct _RSDP {
    char Signature[8];      // Harus berisi "RSD PTR "
    UCHAR Checksum;         // Jumlah semua byte (0-19) jika ditambah harus = 0 (mod 256)
    char OemId[6];
    UCHAR Revision;         // 0 untuk ACPI 1.0, 2 untuk ACPI 2.0+
    ULONG RsdtAddress;      // Alamat fisik 32-bit dari RSDT (Tabel utama ACPI)
} RSDP, __far *LPRSDP;

typedef struct _ACPI_RSDP_EXTENDED {
    struct _RSDP RsdpOld;
    
    // Field ACPI 2.0+ (Hanya valid jika Revision >= 2)
    ULONG  Length;            // Ukuran total struktur RSDP v2+
    ULONGLONG XsdtAddress;       // 64-bit Physical Address XSDT
    UCHAR  ExtendedChecksum;
    UCHAR  Reserved[3];
} ACPI_RSDP_EXTENDED, *PACPI_RSDP_EXTENDED;

typedef struct _ACPI_HEADER
{
    CHAR Signature[4];
    ULONG Length;
    UCHAR  Revision;
    UCHAR  Checksum;
    CHAR OemId[6];
    CHAR OemTableId[8];
    ULONG OemRevision;
    CHAR AslCompilerId[4];
    ULONG AslCompilerRevision;
} ACPI_HEADER, *PACPI_HEADER;

#pragma pack(pop)

// baca disk
void _LBA_READ(unsigned long drive_id, DiskAddressPacket *DAP);

// stropertaion

//
// string operation
//
#define NULL ((void*)0)
void cmemset(void *dst, char value, unsigned int n);
void* cmemcpy(void *dst, const void *src, int n);
int cmemcmp(const void *s1, const void *s2, unsigned int n);
void* cmemmove(void *dst, const void *src, unsigned int n);
char* cstrncpy(char *dst, const char *src, unsigned int n);
char* cstrcpy(char *dst, const char *src);
int cstrcmp(const char *s1, const char *s2);
int cstrncmp(const char *s1, const char *s2, unsigned int n);
char* cstrcat(char *dst, const char *src);
char *cstrfwas(char* string);
int cstrequal(const char *str1, const char *str2);
#define str_equals cstrequal
const char* next_token(const char *src, char *dest_buf);
int parse_decimal(const char *str);
int cstrlen(const char *string);
void int_to_ascii(int num, char *str);
void hex_to_ascii(unsigned long long num, char *str);
int ascii_to_int(const char *str);
unsigned long long ascii_to_hex(const char *str);
char* cstrstr(const char* haystack, const char* needle);
int cstricmp(const char *s1, const char *s2);
char* chextoascii(unsigned int num, char *str);
char* cinttoascii(int num, char *str);

void LBA_READ_TO_BUFFER(uint32_t lba, void* buffer, uint32_t size);
extern MBR_PartitionEntry mbr_part_entry[4];

extern uint32_t __attribute__((cdecl)) _INT10_CALL(uint32_t eax_in, uint32_t ebx_in);
extern uint32_t __attribute__((cdecl)) _INT16_CALL(uint32_t eax_in);
extern uint32_t __attribute__((cdecl)) _INT10_CALL_BUFFER(uint32_t eax, uint32_t ebx, uint32_t ecx, uint16_t es, uint16_t di);
extern int __attribute__((cdecl)) _INT15_E820_CALL(uint32_t *ebx_ptr, uint16_t es, uint16_t di);

void bios_print_string(const char* str);
char wait_for_keypress(void);
void force_reboot();
void bios_print_char(char c);

#define KERNEL_SAFE_PLACE 0x1000000
#define BLOCK_BOOT_1_ADDR 0x20000
#define VBE_BUFFER_ADDR 0x30000

// ==========================================
// 3. VESA VBE Structs (Lengkap VBE 2.0/3.0)
// ==========================================
#pragma pack (push, 1)
typedef struct _VBE_INFO_BLOCK {
    char VbeSignature[4];      // "VESA"
    USHORT VbeVersion;         // 0x0200 = VBE 2.0, 0x0300 = VBE 3.0
    ULONG OemStringPtr;        // Pointer ke OEM String
    ULONG Capabilities;        
    ULONG VideoModePtr;        // Pointer ke array video mode (diakhiri 0xFFFF)
    USHORT TotalMemory;        // Total memori / 64KB
    USHORT OemSoftwareRev;
    ULONG OemVendorNamePtr;
    ULONG OemProductNamePtr;
    ULONG OemProductRevPtr;
    UCHAR Reserved[222];
    UCHAR OemData[256];
} VBE_INFO_BLOCK, __far *LPVBE_INFO_BLOCK;

typedef struct _VBE_MODE_INFO {
    USHORT ModeAttributes;
    UCHAR WinAAttributes;
    UCHAR WinBAttributes;
    USHORT WinGranularity;
    USHORT WinSize;
    USHORT WinASegment;
    USHORT WinBSegment;
    ULONG WinFuncPtr;
    USHORT BytesPerScanLine;   // Pitch!
    
    // VBE 1.2+
    USHORT XResolution;
    USHORT YResolution;
    UCHAR XCharSize;
    UCHAR YCharSize;
    UCHAR NumberOfPlanes;
    UCHAR BitsPerPixel;        // BPP!
    UCHAR NumberOfBanks;
    UCHAR MemoryModel;
    UCHAR BankSize;
    UCHAR NumberOfImagePages;
    UCHAR Reserved1;
    
    // Direct Color Fields (VBE 1.2+)
    UCHAR RedMaskSize;
    UCHAR RedFieldPosition;
    UCHAR GreenMaskSize;
    UCHAR GreenFieldPosition;
    UCHAR BlueMaskSize;
    UCHAR BlueFieldPosition;
    UCHAR RsvdMaskSize;
    UCHAR RsvdFieldPosition;
    UCHAR DirectColorModeInfo;
    
    // VBE 2.0+
    ULONG PhysBasePtr;         // INI YANG PALING PENTING! (LFB)
    ULONG OffScreenMemOffset;
    USHORT OffScreenMemSize;
    UCHAR Reserved2[206];
} VBE_MODE_INFO, __far *LPVBE_MODE_INFO;
#pragma pack(pop)

void restart_n_message(void);

void extract_vbe_info(BLOCK_BOOT_2 *boot_info);
void mbr_detect(void);
LPRSDP find_acpi_rsdp(void);
void extract_memory_map(BLOCK_BOOT_2 *block_boot);

//
// page entry
//
#pragma pack(push, 1)
typedef union _PAGE_ENTRY {
    uint32_t raw; // Buat ngisi nol atau math kasar
    struct {
        uint32_t present       : 1; // Bit 0: 1 = Halaman ada di RAM
        uint32_t rw            : 1; // Bit 1: 0 = Read Only, 1 = Read/Write
        uint32_t user          : 1; // Bit 2: 0 = Kernel (Ring 0), 1 = User (Ring 3)
        uint32_t write_through : 1; // Bit 3: Caching write-through
        uint32_t cache_disable : 1; // Bit 4: Caching disable
        uint32_t accessed      : 1; // Bit 5: Diset CPU kalau memori dibaca
        uint32_t dirty         : 1; // Bit 6: Diset CPU kalau memori ditulis
        uint32_t ps_or_pat     : 1; // Bit 7: Page Size (PDE) / PAT (PTE)
        uint32_t global        : 1; // Bit 8: Global page (nggak di-flush dari TLB)
        ULONG cow              : 1; // Bit 9: Copy on Write
        uint32_t available     : 2; // Bit 10-11: Kosong, OS bebas pake buat apa aja!
        uint32_t frame         : 20;// Bit 12-31: Physical Address >> 12
    } bits;
} PAGE_ENTRY;
#pragma pack(pop)

#define PAGE_DIR_ADDR       0x40000
#define PAGE_TABLE_IDT_ADDR 0x41000
#define PAGE_TABLE_HHK_ADDR 0x42000
PAGE_ENTRY *make_cr3_higher_half(void);

#define PD_INDEX(vaddr) (((uint32_t)(vaddr)) >> 22)
#define PT_INDEX(vaddr) ((((uint32_t)(vaddr)) >> 12) & 0x03FF)

#define RECURSIVE_PT_BASE 0xFFC00000
#define RECURSIVE_PD_BASE 0xFFFFF000

#define GET_PAGE_DIR() ((PAGE_ENTRY *)RECURSIVE_PD_BASE)
#define GET_PAGE_TABLE(vaddr) ((PAGE_ENTRY *)(RECURSIVE_PT_BASE + (PD_INDEX(vaddr) * 4096)))

#define PAGE_TABLE_LFB_ADDR 0x43000
#define LFB_VIRTUAL_ADDR 0xE0000000

BLOCK_BOOT_1 *relocate_block_boot_to_higher_half(BLOCK_BOOT_1 *boot_info, PAGE_ENTRY *pd);

extern BOOLEAN vbe_active;

// VFS Relation
#define FS_ATTR_READONLY    0x01
#define FS_ATTR_HIDDEN      0x02
#define FS_ATTR_SYSTEM      0x04
#define FS_ATTR_DIRECTORY   0x10

typedef struct _VFS_FILE_INFO {
    char FileName[256];
    unsigned long FileSize;
    unsigned char Attributes;
    
    // Ini PENTING: Untuk nyimpen data spesifik FS (kayak FAT32_DirEntry)
    // Biar pas FsReadFile dipanggil, driver FS tau ini file apa.
    unsigned char FsInternal[32]; 
} VFS_FILE_INFO, *PVFS_FILE_INFO;

int FsLookupFileInformation(
    const char* FullPath,
    PVFS_FILE_INFO OutFileInfo
);

unsigned long FsReadFile(PVFS_FILE_INFO FileInfo, void *Buffer);

// LM
extern VEA_MEMORY_DESCRIPTOR_EX LmExtendedMap[128];
extern VEA_MEMORY_DESCRIPTOR LmMemoryMap[64];
extern int LmExtendedMapCount;
extern int LmMemoryMapCount;

#define PDE_INDEX(virt) ((virt) >> 22)
#define PTE_INDEX(virt) (((virt) >> 12) & 0x03FF)

VOID
VEAPI
LmMapVirtualMemory(
    PAGE_ENTRY *PageDirectory,
    ULONG VirtualAddr,
    ULONG PhysicalAddr,
    ULONG PageCount
);

PVOID
VEAPI
LmAllocatePages(ULONG PageCount);

PVOID
VEAPI
LmAllocatePagesPreVirt(ULONG PageCount);

VOID
VEAPI
LmBuildMemoryMap(VOID);

VOID
VEAPI
LmInitMemoryManager(VOID);

PAGE_ENTRY*
VEAPI
LmInitializePaging(VOID);

PAGE_ENTRY*
VEAPI
LmInitSystem(VOID);

PVOID
VEAPI
LmAllocateVirtualPages(ULONG TargetVirtualAddr, ULONG PageCount);

PVOID
VEAPI
LmMapIoSpace(
    ULONG PhysicalAddress,
    ULONG Size
);

#define PAGE_SIZE 0x1000

VOID
VEAPI
AcpiMapExtendedPointer(
    LPRSDP VirtualRsdp
);

//
// Loader Memory Pool
//
typedef enum _BOOT_POOL_TYPE {
    LdrReclaimablePool,        // Memori sementara Bootloader (Boot code, FAT32 buffer, ELF temp)
    LdrUnreclaimablePool,      // Memori permanen Kernel (KIPCR, BootBlock, Kernel Stack, Page Tables, Kernel Image)
    LdrReservedHardwarePool    // Framebuffer VBE, MMIO, ACPI Tables
} BOOT_POOL_TYPE;


PVOID
VEAPI
LmAllocatePool(BOOT_POOL_TYPE PoolType, ULONG ByteSize);

/* Standarized GDT */
#define KGDT32_R0_NULL   0x00   // Index 0 (Offset 0x00), RPL 0
#define KGDT32_R0_CODE   0x08   // Index 1 (Offset 0x08), RPL 0 -> Kernel Code
#define KGDT32_R0_DATA   0x10   // Index 2 (Offset 0x10), RPL 0 -> Kernel Data
#define KGDT32_R3_CODE   0x1B   // Index 3 (Offset 0x1B | 3) -> User Code
#define KGDT32_R3_DATA   0x23   // Index 4 (Offset 0x20 | 3) -> User Data
#define KGDT32_TSS       0x28   // Index 5 (Offset 0x28), RPL 0 -> Task State Segment
#define KGDT32_R0_PCR    0x30   // Index 6 (Offset 0x30), RPL 0 -> KPCR (FS Register)

// Lock
VOID
VEAPI
LdrInitializeSpinLock(PKSPIN_LOCK SpinLock);

// antoher
BOOLEAN
VEAPI
LdrCreateInitialThreadAndProcess(IN OUT BLOCK_BOOT_2 *BlockBoot2);

BOOLEAN
VEAPI
LdrGetSmbiosInformation(IN OUT BLOCK_BOOT_2 *BlockBoot2);

// Bv
VOID
VEAPI
BvPrintLog(const char *Text);

BOOLEAN
VEAPI
BvInitScreen(VOID);

VOID
VEAPI
LdrInfo(const PCHAR String);

VOID
VEAPI
LdrWarning(const PCHAR String);

VOID
VEAPI
LdrError(IN ULONG ErrorCode);

VOID
VEAPI
BvClearScreen(VOID);

extern BOOLEAN InTextMode;

typedef struct _VEA_FOOTER_KEY {
    const char *Text;
} VEA_FOOTER_KEY, *PVEA_FOOTER_KEY;

VOID
VEAPI
BvSetFooterKeyCombination(
    IN PVEA_FOOTER_KEY Keys,
    IN ULONG Count
);

#define MAX_LIST_OPTIONS 10

#define COLOR_LIST_BG        0x00000000  // Hitam (Background list biasa)
#define COLOR_LIST_TEXT      0x00FFFFFF  // Putih (Teks biasa)
#define COLOR_LIST_SEL_BG    0x00C0C0C0  // Silver (Background saat di-highlight)
#define COLOR_LIST_SEL_TEXT  0x00000000  // Hitam (Teks saat di-highlight)

#define LIST_BOTTOM_MARGIN 16

typedef struct _BV_LIST_OPTION {
    const char *Items[MAX_LIST_OPTIONS];
    ULONG ItemCount;
    ULONG X;
    ULONG Y;
    ULONG Width;
    ULONG ItemHeight;
} BV_LIST_OPTION, *PBV_LIST_OPTION;

PBV_LIST_OPTION
VEAPI
BvCreateListOption(
    IN const PCHAR *Items,
    IN ULONG Count,
    IN ULONG Width
);

VOID
VEAPI
BvUpListOption(
    IN PBV_LIST_OPTION List,
    IN ULONG OldIndex,
    IN ULONG NewIndex
);

VOID
VEAPI
BvDownListOption(
    IN PBV_LIST_OPTION List,
    IN ULONG OldIndex,
    IN ULONG NewIndex
);

//
// Key Definition
//
// Tombol Standar (Berdasarkan ASCII di Byte Bawah)
#define KEY_BACKSPACE   0x0008  // '\b'
#define KEY_TAB         0x0009  // '\t'
#define KEY_ENTER       0x000D  // '\r'
#define KEY_ESC         0x001B  // ESC

// Tombol Spesial / Non-ASCII (Berdasarkan Scan Code di Byte Atas: ScanCode << 8)
#define KEY_UP          0x4800
#define KEY_DOWN        0x5000
#define KEY_LEFT        0x4B00
#define KEY_RIGHT       0x4D00

#define KEY_HOME        0x4700
#define KEY_END         0x4F00
#define KEY_PAGE_UP     0x4900
#define KEY_PAGE_DOWN   0x5100
#define KEY_DELETE      0x5300

// Function Keys (F1 - F12)
#define KEY_F1          0x3B00
#define KEY_F2          0x3C00
#define KEY_F3          0x3D00
#define KEY_F4          0x3E00
#define KEY_F5          0x3F00
#define KEY_F6          0x4000
#define KEY_F7          0x4100
#define KEY_F8          0x4200
#define KEY_F9          0x4300
#define KEY_F10         0x4400
#define KEY_F11         0x8500
#define KEY_F12         0x8600

// Extended Key API
SHORT
GetKeycode(VOID);

VOID
FsInitialize(VOID);

VOID
VEAPI
BvRerenderLayout(VOID);

typedef struct _OSI386_BOOT_OPTION
{
    PCHAR BootName;
    PCHAR ArgumentLineOptions;
    BOOLEAN Chainloading;
} OSI386_BOOT_OPTION, *POSI386_BOOT_OPTION;

VOID
VEAPI
BvErrorLog(
    IN PCHAR ErrorCode,
    IN PCHAR ErrorFile
);

extern CHAR LdrLastCheckedFile[256];

// Standard error status for OSI386
#define STATUS_VBE_ERROR                    0xC0000001
#define STATUS_LOADER_MEMORY_FAILURE        0xC0000002
#define STATUS_INSUFFICIENT_RESOURCES       0xC0000003
#define STATUS_FILE_NOT_FOUND               0xC0000004
#define STATUS_FILE_EXECUTABLE_INVALID      0xC0000005
#define STATUS_UNKNOWN                      0xC0000006
#define STATUS_LOADER_POOL_CORRUPT          0xC0000007
#define STATUS_MEMORY_MAP_FAILED            0xC0000008
#define STATUS_FILE_SYSTEM_CORRUPT          0xC0000009
#define STATUS_BOOTING_CHAINLOADING_FAILURE 0xC000000A
#define STATUS_BOOTING_FAILED               0xC000000B
#define STATUS_READ_DISK_ERROR              0xC000000C
#define STATUS_VKEY_CORRUPTED               0xC000000D
#define STATUS_PROCEDURE_NOT_FOUND          0xC000000E

VOID
VEAPI
LdrBuildProcessorControlBlock(IN OUT BLOCK_BOOT_2 *BlockBoot2);

VOID
VEAPI
BvMoveVideoInfoToBootBlock(IN OUT BLOCK_BOOT_2 *BlockBoot2);

VOID
VEAPI
LdrLoadSystemHive(
    IN OUT BLOCK_BOOT_2 *BlockBoot
);

VOID
VEAPI
LdrExtractDiskInformation(IN OUT HPBLOCK_BOOT_2 BlockBoot2);

VOID 
VEAPI
LvkInitializeLoaderParser(IN PBLOCK_BOOT_2 BlockBoot2);

VOID
VEAPI
LdrLoadBootDriver(IN PBLOCK_BOOT_2 BlockBoot2);

extern BOOLEAN LvkParserReady;

BOOLEAN
VEAPI
LvkLoadBootDrivers(VOID);

extern PBLOCK_BOOT_2 LdrBlockBoot;
extern PVOID LdrBufferToKernel;

VOID
VEAPI
LdrLoadBootDriver(IN PBLOCK_BOOT_2 BlockBoot2);

LONG
VEAPI
LdrInitLoadBootDriver(
    IN PSTR ServiceName,
    IN PSTR ResolvedPath
);

extern BOOLEAN BvFooterHeaderState;