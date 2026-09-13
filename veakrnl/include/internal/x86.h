#pragma once

#include <../ndk/ks.h>
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

typedef struct _KSWITCH_FRAME {
    ULONG Edi;
    ULONG Esi;
    ULONG Ebx;
    ULONG Ebp;
    ULONG EFlags;
    ULONG SwapReturnAddress; // Di-pop ke EIP (Menuju PtpSystemThreadStartup)
    ULONG DummyArg1;         // Dibuang oleh 'ret 8'
    ULONG DummyArg2;         // Dibuang oleh 'ret 8'
    ULONG WrapperReturnAddress; // Alamat kembalian (PtpExitThread)
    ULONG StartAddress;         // Argumen 1: Fungsi Asli (UlPhase1)
    ULONG Argument;             // Argumen 2: Argumen Fungsi Asli
} KSWITCH_FRAME, *PKSWITCH_FRAME;

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
extern void isr_3_entry(void);
extern void isr_6_entry(void);
extern void isr_8_entry(void);
extern void isr_13_entry(void);
extern void isr_14_entry(void);

// ============================================================================
// IDT GATE FLAGS (32-Bit x86)
// ============================================================================

//
// 1. INTERRUPT GATES (CPU Otomatis CLI / Mematikan Interrupt Flag saat masuk)
//
#define IDT_FLAGS_INTR_RING0    0x8E    // 1000 1110b -> Kernel Interrupt (Hardware IRQ, APIC, dll)
#define IDT_FLAGS_INTR_RING3    0xEE    // 1110 1110b -> User Interrupt (Bisa dipanggil User Mode via 'INT n')

//
// 2. TRAP GATES (CPU TIDAK Mengubah Interrupt Flag / IF tetap seperti semula)
//
#define IDT_FLAGS_TRAP_RING0    0x8F    // 1000 1111b -> Kernel Exception (Divide Error, Page Fault, dll)
#define IDT_FLAGS_TRAP_RING3    0xEF    // 1110 1111b -> User Exception/Breakpoint (INT 3, INTO, Syscall)

//
// 3. TASK GATES (Memanfaatkan Hardware TSS Switching)
//
#define IDT_FLAGS_TASK_RING0    0x85    // 1000 0101b -> Hardware Task Gate (Khusus Double Fault #DF)

/* Standarized GDT */
#define KGDT32_R0_NULL   0x00   // Index 0 (Offset 0x00), RPL 0
#define KGDT32_R0_CODE   0x08   // Index 1 (Offset 0x08), RPL 0 -> Kernel Code
#define KGDT32_R0_DATA   0x10   // Index 2 (Offset 0x10), RPL 0 -> Kernel Data
#define KGDT32_R3_CODE   0x1B   // Index 3 (Offset 0x1B | 3) -> User Code
#define KGDT32_R3_DATA   0x23   // Index 4 (Offset 0x20 | 3) -> User Data
#define KGDT32_TSS       0x28   // Index 5 (Offset 0x28), RPL 0 -> Task State Segment
#define KGDT32_R0_PCR    0x30   // Index 6 (Offset 0x30), RPL 0 -> KPCR (FS Register)

/* Where kernel bytes END? */
extern ULONG _veaos_end;

#define MAX_CPU 32

#define STANDARD_TIMER_HZ 250
#define TIMER_INTERVAL_MS       (1000 / STANDARD_TIMER_HZ)  // 4 ms
#define TIMER_INCREMENT         (TIMER_INTERVAL_MS * 10000) 

typedef struct _VEA_TIB {
    struct _EXCEPTION_REGISTRATION_RECORD *ExceptionList; // FS:[0x00]
    PVOID StackBase;                                      // FS:[0x04]
    PVOID StackLimit;                                     // FS:[0x08]
    PVOID SubSystemTib;                                   // FS:[0x0C]
    PVOID FiberData;                                      // FS:[0x10]
    PVOID ArbitraryUserPointer;                           // FS:[0x14]
    struct _VEA_TIB *Self;                                 // FS:[0x18] (User-mode Self-Pointer)
} VEA_TIB, *PVEA_TIB;

typedef UCHAR KAFFINITY;

#pragma pack(push, 4)

typedef struct _KPRCB {
    // --- SCHEDULER STATE ---
    struct _KTHREAD *CurrentThread;  
    struct _KTHREAD *NextThread;     
    struct _KTHREAD *IdleThread;     

    // --- SMP / CPU IDENTITY ---
    UCHAR Number;                    // ID Core (0, 1, 2...)
    KAFFINITY SetMember;             // Bitmask CPU ini (1 << Number), penting untuk IPI!
    UCHAR CpuType;
    UCHAR CpuID;
    
    // --- HAL / HARDWARE RESERVED ---
    PVOID HalReserved[16];           // Tempat menyimpan LAPIC Base atau Local Timer

    // --- DPC (DEFERRED PROCEDURE CALLS) ---
    // PENTING UNTUK MASA DEPAN: Interrupt handler gak boleh lama-lama!
    // Tugas berat dari interrupt dilempar ke antrean DPC ini.
    KSPIN_LOCK DpcLock;
    LIST_ENTRY DpcListHead;
    PVOID DpcStack;                  // Stack khusus saat mengeksekusi DPC
    volatile UCHAR DpcRoutineActive;
    volatile UCHAR DpcInterruptRequested;

    // --- IPI (INTER-PROCESSOR INTERRUPTS) ---
    // Untuk nyuruh Core lain berhenti atau nembak TLB Flush
    volatile ULONG IpiFrozen;
    volatile PVOID WorkerRoutine;

    // --- CONTEXT SWITCH & LOCKS ---
    ULONG ContextSwitches;
    // Tambahkan variabel KSPIN_LOCK khusus untuk antrean scheduler di sini jika perlu
} KPRCB, *PKPRCB;

typedef struct _KPCR {
    union {
        VEA_TIB VeaTib;                // Offset 0x00 s/d 0x1B
        struct {
            PVOID Used_ExceptionList;
            PVOID Used_StackBase;
            PVOID Reserved2;
            PVOID TssCopy;           // FS:[0x0C]
            ULONG ContextSwitchesCopy;
            KAFFINITY SetMemberCopy;
            PVOID Used_Self;
        };
    };
    
    struct _KPCR *SelfPcr;           // FS:[0x1C] - KUNCI: Pointer ke KPCR ini sendiri
    struct _KPRCB *Prcb;             // FS:[0x20] - Pointer ke ekstensi KPRCB
    
    KIRQL Irql;                      // FS:[0x24] - IRQL saat ini (PASSIVE, DISPATCH, dsb)
    ULONG IRR;                       // FS:[0x28] - Interrupt Request Register (Software)
    ULONG IrrActive;                 // FS:[0x2C]
    ULONG IDR;                       // FS:[0x30]
    PVOID KdVersionBlock;            // FS:[0x34] - Info untuk Kernel Debugger
    
    struct _KIDTENTRY *IDT;          // FS:[0x38]
    struct _KGDTENTRY *GDT;          // FS:[0x3C]
    struct _KTSS *TSS;               // FS:[0x40]
    
    USHORT MajorVersion;
    USHORT MinorVersion;
    KAFFINITY SetMember;             // FS:[0x48]
    ULONG StallScaleFactor;          // Untuk delay loop
    UCHAR Number;                    // Processor ID
} KPCR, *PKPCR;

typedef struct _KIPCR {
    KPCR Pcr;       // Bagian publik di atas (selalu harus di urutan pertama)
    KPRCB PrcbData; // Bagian privat (Scheduler & State CPU)
} KIPCR, *PKIPCR;

#pragma pack(pop)

#define KeReloadFsPcr() \
    asm volatile("mov %0, %%fs" : : "r" ((USHORT)KGDT32_R0_PCR) : "memory")