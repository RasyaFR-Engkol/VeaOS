#pragma once

#include <stdint.h>

typedef char CHAR;
typedef short SHORT;
typedef long LONG;

typedef unsigned long ULONG;
typedef unsigned char UCHAR;
typedef unsigned short USHORT;

#define __huge
#define __far

typedef void VOID;
typedef VOID *POINTER, __huge *HPOINTER, __far *FPOINTER, *UNKNOWN_TYPE,
 __huge *HUNKNOWN_TYPE, __far *FUNKNOWN_TYPE;
typedef UCHAR *PUCHAR, __huge *HPUCHAR, __far *FPUCHAR;
typedef ULONG *PULONG, __huge *HPULONG, __far *FPULONG;
typedef USHORT *PUSHORT, __huge *HPUSHORT, __far *FPUSHORT;
typedef CHAR *PCHAR, __huge *HPCHAR, __far *FPCHAR;
typedef SHORT *PSHORT, __huge *HPSHORT, __far *FPSHORT;
typedef LONG *PLONG, __huge *HPLONG, __far *FPLONG;

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
    unsigned int       ExtendedAttributes;
} __attribute__((packed)) VEA_MEMORY_DESCRIPTOR, *PVEA_MEMORY_DESCRIPTOR;

typedef struct _BLOCK_BOOT_1
{
    ULONG Size;
    ULONG Version;

    struct _VIDEO_BOOT
    {
        HPOINTER VideoBootAddress;
        ULONG X, Y, W, H;
        ULONG Sprite;
        HPOINTER AdditionalInformation[5];
    } VideoBoot;

    HPOINTER AcpiTable;
    ULONG AcpiTableByteSize;

    unsigned int MemoryMapCount;
    PVEA_MEMORY_DESCRIPTOR MemoryMap;
} BLOCK_BOOT_1, __huge *HPBLOCK_BOOT_1;

// Struktur data RSDP versi ACPI 1.0 (Ukuran pasti 20 byte)
typedef struct _RSDP {
    char Signature[8];      // Harus berisi "RSD PTR "
    UCHAR Checksum;         // Jumlah semua byte (0-19) jika ditambah harus = 0 (mod 256)
    char OemId[6];
    UCHAR Revision;         // 0 untuk ACPI 1.0, 2 untuk ACPI 2.0+
    ULONG RsdtAddress;      // Alamat fisik 32-bit dari RSDT (Tabel utama ACPI)
} RSDP, __far *LPRSDP;

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

void extract_vbe_info(BLOCK_BOOT_1 *boot_info);
void mbr_detect(void);
LPRSDP find_acpi_rsdp(void);
void extract_memory_map(BLOCK_BOOT_1 *block_boot);

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
        uint32_t available     : 3; // Bit 9-11: Kosong, OS bebas pake buat apa aja!
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

extern _Bool vbe_active;
