#pragma once

#include <procbind.h>

typedef struct _KGDTENTRY32 {
    USHORT LimitLow;           // Bit 0-15 dari Segment Limit
    USHORT BaseLow;            // Bit 0-15 dari Base Address
    union {
        struct {
            UCHAR BaseMid;     // Bit 16-23 dari Base Address
            UCHAR Flags1;      // Gabungan Type, S, DPL, P
            UCHAR Flags2;      // Gabungan LimitHigh, AVL, L, D/B, G
            UCHAR BaseHi;      // Bit 24-31 dari Base Address
        } Bytes;
        struct {
            ULONG BaseMid : 8;
            ULONG Type : 4;    // Segment type (e.g., Code / Data attributes)
            ULONG System : 1;  // S bit: 0 = System Descriptor, 1 = Code/Data Segment
            ULONG Dpl : 2;     // Descriptor Privilege Level (0 = Kernel, 3 = User)
            ULONG Present : 1; // P bit: Must be 1 for valid segments
            ULONG LimitHi : 4; // Bit 16-19 dari Segment Limit
            ULONG Avl : 1;     // Available for system software use
            ULONG LongMode : 1;// L bit: 1 = 64-bit code segment (IA-32e)
            ULONG DefaultBig : 1; // D/B bit: 0 = 16-bit, 1 = 32-bit
            ULONG Granularity : 1;// G bit: 0 = Byte, 1 = 4KB pages alignment
            ULONG BaseHi : 8;
        } Bits;
    } HighWord;
} KGDTENTRY32, *PKGDTENTRY32;

typedef struct _KIDTDESCRIPTOR32 {
    USHORT OffsetLow;          // Bit 0-15 dari Isr Address (Offset)
    USHORT Selector;           // Target Code Segment Selector di GDT (misal: 0x08)
    UCHAR  Reserved;           // Harus di-set 0
    union {
        UCHAR Flags;
        struct {
            UCHAR Type : 4;    // 0xE = 32-bit Interrupt Gate, 0xF = 32-bit Trap Gate
            UCHAR System : 1;  // S bit: Harus 0 untuk Gate / System Descriptor
            UCHAR Dpl : 2;     // Descriptor Privilege Level (Siapa yang boleh panggil via INT)
            UCHAR Present : 1; // P bit: Must be 1 to active
        } Bits;
    } Attributes;
    USHORT OffsetHigh;         // Bit 16-31 dari Isr Address (Offset)
} KIDTDESCRIPTOR32, *PKIDTDESCRIPTOR32;

typedef struct _KTSSDESCRIPTOR32 {
    USHORT LimitLow;           // Ukuran TSS - 1 (Biasanya sizeof(KTSS32) - 1)
    USHORT BaseLow;            // Bit 0-15 dari alamat struktur TSS fisik
    union {
        struct {
            UCHAR BaseMid;
            UCHAR Flags1;
            UCHAR Flags2;
            UCHAR BaseHi;
        } Bytes;
        struct {
            ULONG BaseMid : 8;
            ULONG Type : 4;    // 0x9 = Available 32-bit TSS, 0xB = Busy 32-bit TSS
            ULONG System : 1;  // S bit: Wajib 0 untuk TSS descriptor
            ULONG Dpl : 2;     // DPL ring
            ULONG Present : 1; // P bit: Must be 1
            ULONG LimitHi : 4; // Bit 16-19 dari TSS Limit
            ULONG Avl : 1;     // AVL
            ULONG Reserved0 : 1;// L bit: 0 untuk TSS
            ULONG DefaultBig : 1;// D/B bit: 0 untuk TSS descriptor
            ULONG Granularity : 1;// G bit: Biasanya 0 (Byte granularity)
            ULONG BaseHi : 8;
        } Bits;
    } HighWord;
} KTSSDESCRIPTOR32, *PKTSSDESCRIPTOR32;

typedef struct _KREGISTER_FRAME {
    // =========================================================================
    // SEGMENT REGISTERS (Didorong manual di ISR Stub, misal: push ds)
    // =========================================================================
    ULONG Gs;
    ULONG Fs;
    ULONG Es;
    ULONG Ds;

    // =========================================================================
    // GENERAL PURPOSE REGISTERS (Urutan sesuai instruksi PUSHAD)
    // =========================================================================
    ULONG Edi;
    ULONG Esi;
    ULONG Ebp;
    ULONG Esp;      // Nilai ESP dari PUSHAD (biasanya diabaikan saat POPAD)
    ULONG Ebx;
    ULONG Edx;
    ULONG Ecx;
    ULONG Eax;

    // =========================================================================
    // INTERRUPT INFO (Didorong manual oleh ISR stub sebelum panggil C handler)
    // =========================================================================
    ULONG Vector;    // Nomor Interupsi / ISR Vector (misal: 0x0D untuk #GP)
    ULONG ErrorCode; // Didorong otomatis oleh CPU untuk exception tertentu, 
                     // atau dummy 0 didorong manual jika tidak ada error code.

    // =========================================================================
    // HARDWARE FRAME (Didorong OTOMATIS oleh CPU saat interupsi terjadi)
    // =========================================================================
    ULONG Eip;
    ULONG Cs;
    ULONG Eflags;
    ULONG HardwareEsp;
    ULONG HardwareSs;
} KREGISTER_FRAME, *PKREGISTER_FRAME;

/* Macro for excpetion*/
#define KS_EXCEPTION_DIVIDE_BY_ZERO          0
#define KS_EXCEPTION_DEBUG                   1
#define KS_EXCEPTION_NMI                     2 // Non-Maskable Interrupt
#define KS_EXCEPTION_BREAKPOINT              3
#define KS_EXCEPTION_OVERFLOW                4
#define KS_EXCEPTION_BOUNDS_CHECK            5
#define KS_EXCEPTION_INVALID_OPCODE          6
#define KS_EXCEPTION_DEVICE_NOT_AVAILABLE    7
#define KS_EXCEPTION_DOUBLE_FAULT            8
#define KS_EXCEPTION_COPROCESSOR_OVERRUN     9 // Hanya di CPU jadul
#define KS_EXCEPTION_INVALID_TSS             10
#define KS_EXCEPTION_SEGMENT_NOT_PRESENT     11
#define KS_EXCEPTION_STACK_FAULT             12
#define KS_EXCEPTION_GENERAL_PROTECTION      13 // #GP Fault paling sering muncul
#define KS_EXCEPTION_PAGE_FAULT              14 // #PF Fault
#define KS_EXCEPTION_FLOATING_POINT_ERROR    16
#define KS_EXCEPTION_ALIGNMENT_CHECK         17
#define KS_EXCEPTION_MACHINE_CHECK           18
#define KS_EXCEPTION_SIMD_ERROR              19
#define KS_EXCEPTION_VIRTUALIZATION          20
#define KS_EXCEPTION_CONTROL_PROTECTION      30

/* pf error */
#define PF_ERROR_PRESENT        (1 << 0) // 0 = Page ga ada, 1 = Page protection violation (Read/Write/User violation)
#define PF_ERROR_WRITE          (1 << 1) // 0 = Crash pas READ, 1 = Crash pas WRITE
#define PF_ERROR_USER           (1 << 2) // 0 = Terjadi di Kernel Mode, 1 = Terjadi di User Mode
#define PF_ERROR_RESERVED       (1 << 3) // 1 = Ada reserved bit di page table yang ke-set
#define PF_ERROR_INSTRUCTION    (1 << 4) // 1 = Crash pas coba eksekusi kode (NX/No-Execute protection hit)

#define RPL_MASK  0x03

#define MODE_KERNEL 0
#define MODE_USER   3

/* KIDTR */
#pragma pack(push, 1)
typedef struct _KIDTR32 {
    USHORT Limit;   // Ukuran total IDT dalam byte minus 1
    ULONG Base;     // Alamat memori fisik awal dari array KiIdt
} KIDTR32, *PKIDTR32;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct _KDESCRIPTOR {
    USHORT Limit;
    ULONG Base;
} KDESCRIPTOR, *PKDESCRIPTOR;
#pragma pack(pop)

/* Exception Handling */
VOID
VEAPI
ks_initialize_exception(VOID);

VOID
VEAPI
ks_load_idt(PKIDTR32 KIDTR32);

/* List of TRAP */
extern void isr_0_entry(void);
extern void isr_1_entry(void);
extern void isr_2_entry(void);
extern void isr_6_entry(void);
extern void isr_8_entry(void);
extern void isr_13_entry(void);
extern void isr_14_entry(void);

/* TRAP FLAG */
#define IDT_FLAGS_INTR_RING0 0x8E

/* Standarized GDT */
#define KGDT32_R0_NULL   0x00   // Index 0 (Offset 0x00), RPL 0
#define KGDT32_R0_CODE   0x08   // Index 1 (Offset 0x08), RPL 0 -> Kernel Code
#define KGDT32_R0_DATA   0x10   // Index 2 (Offset 0x10), RPL 0 -> Kernel Data
#define KGDT32_R3_CODE   0x1B   // Index 3 (Offset 0x18 | 3) -> User Code
#define KGDT32_R3_DATA   0x23   // Index 4 (Offset 0x20 | 3) -> User Data
#define KGDT32_TSS       0x28   // Index 5 (Offset 0x28), RPL 0 -> Task State Segment
#define KGDT32_R0_PCR    0x30   // Index 6 (Offset 0x30), RPL 0 -> KPCR (FS Register)

/* Where kernel bytes END? */
extern ULONG _veaos_end;

