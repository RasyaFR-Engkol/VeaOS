// VEAOS GCC INTRIN
//
// Feel Free to use. This is FREE
//
// These intrinsics are only used for compatibility with GCC and Clang. 
// If used with other compilers such as MSVC, these intrinsics will not work. 
// If you want to edit the Vea intrinsic file for MSVC, go to the msvc_intrin.h file.
//

#include <procbind.h>

#if defined(GCC_COMPILER)

#define __INTRIN__ FORCEINLINE

typedef UCHAR U8;
typedef USHORT U16;
typedef ULONG U32;
typedef ULONGLONG U64;
typedef ULONG_PTR UPTR;

typedef CHAR I8;
typedef SHORT I16;
typedef LONG I32;
typedef LONGLONG I64;

PVOID __INTRIN__ __GetReturnAddress(void)
{
    return __builtin_return_address(0);
}

PVOID __INTRIN__ __GetReturnAddress1(void)
{
    return __builtin_return_address(1);
}

PVOID __INTRIN__ __GetReturnAddress2(void)
{
    return __builtin_return_address(2);
}

PVOID __INTRIN__ __GetReturnAddress3(void)
{
    return __builtin_return_address(3);
}

VOID __INTRIN__ __LoadIDT(const void* pIdtr)
{
    __asm__ __volatile__("lidt %0" : : "m"(*(const char*)pIdtr) : "memory");
}

VOID __INTRIN__ __StoreIDT(void* pIdtr)
{
    __asm__ __volatile__("sidt %0" : "=m"(*(char*)pIdtr) : : "memory");
}

VOID __INTRIN__ __LoadGDT(const void* pGdtr)
{
    __asm__ __volatile__("lgdt %0" : : "m"(*(const char*)pGdtr) : "memory");
}

VOID __INTRIN__ __StoreGDT(void* pGdtr)
{
    __asm__ __volatile__("sgdt %0" : "=m"(*(char*)pGdtr) : : "memory");
}

void __INTRIN__ __LoadLDT(U16 Selector)
{
    __asm__ __volatile__("lldt %0" : : "r"(Selector) : "memory");
}

U16 __INTRIN__ __StoreLDT(void)
{
    U16 selector;
    __asm__ __volatile__("sldt %0" : "=g"(selector) : : "memory");
    return selector;
}

void __INTRIN__ __StoreLDTPtr(U16* pSelector)
{
    __asm__ __volatile__("sldt %0" : "=m"(*pSelector) : : "memory");
}

U32 __INTRIN__ __ReadEAX(void)
{
    U32 val;
    __asm__ __volatile__("movl %%eax, %0" : "=r"(val));
    return val;
}

U32 __INTRIN__ __ReadEBX(void)
{
    U32 val;
    __asm__ __volatile__("movl %%ebx, %0" : "=r"(val));
    return val;
}

U32 __INTRIN__ __ReadECX(void)
{
    U32 val;
    __asm__ __volatile__("movl %%ecx, %0" : "=r"(val));
    return val;
}

U32 __INTRIN__ __ReadEDX(void)
{
    U32 val;
    __asm__ __volatile__("movl %%edx, %0" : "=r"(val));
    return val;
}

U32 __INTRIN__ __ReadESI(void)
{
    U32 val;
    __asm__ __volatile__("movl %%esi, %0" : "=r"(val));
    return val;
}

U32 __INTRIN__ __ReadEDI(void)
{
    U32 val;
    __asm__ __volatile__("movl %%edi, %0" : "=r"(val));
    return val;
}

U32 __INTRIN__ __ReadEBP(void)
{
    U32 val;
    __asm__ __volatile__("movl %%ebp, %0" : "=r"(val));
    return val;
}

U32 __INTRIN__ __ReadESP(void)
{
    U32 val;
    __asm__ __volatile__("movl %%esp, %0" : "=r"(val));
    return val;
}

U16 __INTRIN__ __ReadCS(void)
{
    U16 val;
    __asm__ __volatile__("mov %%cs, %0" : "=r"(val));
    return val;
}

U16 __INTRIN__ __ReadDS(void)
{
    U16 val;
    __asm__ __volatile__("mov %%ds, %0" : "=r"(val));
    return val;
}

U16 __INTRIN__ __ReadSS(void)
{
    U16 val;
    __asm__ __volatile__("mov %%ss, %0" : "=r"(val));
    return val;
}

U16 __INTRIN__ __ReadES(void)
{
    U16 val;
    __asm__ __volatile__("mov %%es, %0" : "=r"(val));
    return val;
}

U16 __INTRIN__ __ReadFS(void)
{
    U16 val;
    __asm__ __volatile__("mov %%fs, %0" : "=r"(val));
    return val;
}

U16 __INTRIN__ __ReadGS(void)
{
    U16 val;
    __asm__ __volatile__("mov %%gs, %0" : "=r"(val));
    return val;
}

U32 __INTRIN__ __ReadDR0(void)
{
    U32 val;
    __asm__ __volatile__("mov %%dr0, %0" : "=r"(val));
    return val;
}

U32 __INTRIN__ __ReadDR1(void)
{
    U32 val;
    __asm__ __volatile__("mov %%dr1, %0" : "=r"(val));
    return val;
}

U32 __INTRIN__ __ReadDR2(void)
{
    U32 val;
    __asm__ __volatile__("mov %%dr2, %0" : "=r"(val));
    return val;
}

U32 __INTRIN__ __ReadDR3(void)
{
    U32 val;
    __asm__ __volatile__("mov %%dr3, %0" : "=r"(val));
    return val;
}

U32 __INTRIN__ __ReadDR4(void)
{
    U32 val;
    __asm__ __volatile__("mov %%dr4, %0" : "=r"(val));
    return val;
}

U32 __INTRIN__ __ReadDR5(void)
{
    U32 val;
    __asm__ __volatile__("mov %%dr5, %0" : "=r"(val));
    return val;
}

U32 __INTRIN__ __ReadDR6(void)
{
    U32 val;
    __asm__ __volatile__("mov %%dr6, %0" : "=r"(val));
    return val;
}

U32 __INTRIN__ __ReadDR7(void)
{
    U32 val;
    __asm__ __volatile__("mov %%dr7, %0" : "=r"(val));
    return val;
}

void __INTRIN__ __Stop(void)
{
    __asm__ __volatile__("cli" ::: "memory");
}

void __INTRIN__ __Start(void)
{
    __asm__ __volatile__("sti" ::: "memory");
}

U32 __INTRIN__ __ReadEFlags(void)
{
    U32 eflags;
    __asm__ __volatile__(
        "pushfl\n\t"
        "popl %0"
        : "=r"(eflags)
        :
        : "memory"
    );
    return eflags;
}

void __INTRIN__ __WriteEFlags(U32 eflags)
{
    __asm__ __volatile__(
        "pushl %0\n\t"
        "popfl"
        :
        : "r"(eflags)
        : "memory", "cc"
    );
}

I32 __INTRIN__ __InterlockedIncrement(volatile I32* Target)
{
    I32 value = 1;
    __asm__ __volatile__(
        "lock xaddl %0, %1"
        : "+r"(value), "+m"(*Target)
        :
        : "memory"
    );
    return value + 1; 
}

I32 __INTRIN__ __InterlockedDecrement(volatile I32* Target)
{
    I32 value = -1;
    __asm__ __volatile__(
        "lock xaddl %0, %1"
        : "+r"(value), "+m"(*Target)
        :
        : "memory"
    );
    return value - 1; 
}

I16 __INTRIN__ __InterlockedIncrement16(volatile I16* Target)
{
    I16 value = 1;
    __asm__ __volatile__(
        "lock xaddw %0, %1"
        : "+r"(value), "+m"(*Target)
        :
        : "memory"
    );
    return value + 1;
}

I16 __INTRIN__ __InterlockedDecrement16(volatile I16* Target)
{
    I16 value = -1;
    __asm__ __volatile__(
        "lock xaddw %0, %1"
        : "+r"(value), "+m"(*Target)
        :
        : "memory"
    );
    return value - 1;
}

I32 __INTRIN__ __InterlockedExchange(volatile I32* Target, I32 Value)
{
    // xchg secara otomatis melakukan BUS lock tanpa perlu prefix 'lock'
    __asm__ __volatile__(
        "xchgl %0, %1"
        : "+r"(Value), "+m"(*Target)
        :
        : "memory"
    );
    return Value;
}

I32 __INTRIN__ __InterlockedCompareExchange(volatile I32* Target, I32 Exchange, I32 Comperand)
{
    I32 old_value;
    __asm__ __volatile__(
        "lock cmpxchgl %2, %1"
        : "=a"(old_value), "+m"(*Target)
        : "r"(Exchange), "0"(Comperand)
        : "memory"
    );
    return old_value; 
}

PVOID __INTRIN__ __InterlockedExchangePointer(volatile void** Target, void* Value)
{
#if defined(__x86_64__) || defined(_M_X64)
    __asm__ __volatile__(
        "xchgq %0, %1"
        : "+r"(Value), "+m"(*Target)
        :
        : "memory"
    );
#else
    __asm__ __volatile__(
        "xchgl %0, %1"
        : "+r"(Value), "+m"(*Target)
        :
        : "memory"
    );
#endif
    return Value;
}

void __INTRIN__ __YieldProcessor(void)
{
    __asm__ __volatile__("pause" ::: "memory");
}

U8 __INTRIN__ __In8(U16 Port)
{
    U8 value;
    __asm__ __volatile__("inb %1, %0" : "=a"(value) : "Nd"(Port));
    return value;
}

void __INTRIN__ __Out8(U16 Port, U8 Data)
{
    __asm__ __volatile__("outb %0, %1" : : "a"(Data), "Nd"(Port));
}

U16 __INTRIN__ __In16(U16 Port)
{
    U16 value;
    __asm__ __volatile__("inw %1, %0" : "=a"(value) : "Nd"(Port));
    return value;
}

void __INTRIN__ __Out16(U16 Port, U16 Data)
{
    __asm__ __volatile__("outw %0, %1" : : "a"(Data), "Nd"(Port));
}

U32 __INTRIN__ __In32(U16 Port)
{
    U32 value;
    __asm__ __volatile__("inl %1, %0" : "=a"(value) : "Nd"(Port));
    return value;
}

void __INTRIN__ __Out32(U16 Port, U32 Data)
{
    __asm__ __volatile__("outl %0, %1" : : "a"(Data), "Nd"(Port));
}

U32 __INTRIN__ __ReadCR0(void)
{
    U32 val;
    __asm__ __volatile__("mov %%cr0, %0" : "=r"(val));
    return val;
}

void __INTRIN__ __WriteCR0(U32 Data)
{
    __asm__ __volatile__("mov %0, %%cr0" : : "r"(Data) : "memory");
}

U32 __INTRIN__ __ReadCR2(void)
{
    U32 val;
    __asm__ __volatile__("mov %%cr2, %0" : "=r"(val));
    return val;
}

void __INTRIN__ __WriteCR2(U32 Data)
{
    __asm__ __volatile__("mov %0, %%cr2" : : "r"(Data) : "memory");
}

U32 __INTRIN__ __ReadCR3(void)
{
    U32 val;
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(val));
    return val;
}

void __INTRIN__ __WriteCR3(U32 Data)
{
    __asm__ __volatile__("mov %0, %%cr3" : : "r"(Data) : "memory");
}

U32 __INTRIN__ __ReadCR4(void)
{
    U32 val;
    __asm__ __volatile__("mov %%cr4, %0" : "=r"(val));
    return val;
}

void __INTRIN__ __WriteCR4(U32 Data)
{
    __asm__ __volatile__("mov %0, %%cr4" : : "r"(Data) : "memory");
}

U64 __INTRIN__ __ReadMSR(U32 Msr)
{
    U32 low, high;
    __asm__ __volatile__("rdmsr" : "=a"(low), "=d"(high) : "c"(Msr));
    return ((U64)high << 32) | low;
}

void __INTRIN__ __WriteMSR(U32 Msr, U64 Value)
{
    U32 low = (U32)Value;
    U32 high = (U32)(Value >> 32);
    __asm__ __volatile__("wrmsr" : : "a"(low), "d"(high), "c"(Msr));
}

void __INTRIN__ __CpuID(U32 Leaf, U32* Eax, U32* Ebx, U32* Ecx, U32* Edx)
{
    __asm__ __volatile__(
        "cpuid"
        : "=a"(*Eax), "=b"(*Ebx), "=c"(*Ecx), "=d"(*Edx)
        : "a"(Leaf)
    );
}

U32 __INTRIN__ __ReadFS32(U32 Offset)
{
    U32 val;
    __asm__ __volatile__("movl %%fs:%1, %0" : "=r"(val) : "m"(*(U32*)(UPTR)Offset));
    return val;
}

void __INTRIN__ __InvalidatePage(void* Address)
{
    __asm__ __volatile__("invlpg (%0)" : : "r"(Address) : "memory");
}

void __INTRIN__ __Halt(void)
{
    __asm__ __volatile__("hlt" ::: "memory");
}

void __INTRIN__ __WriteBackInvalidate(void)
{
    __asm__ __volatile__("wbinvd" ::: "memory");
}

U64 __INTRIN__ __ReadTSC(void)
{
    U32 low, high;
    __asm__ __volatile__("rdtsc" : "=a"(low), "=d"(high));
    return ((U64)high << 32) | low;
}

U64 __INTRIN__ __ReadTSCP(U32* TscAux)
{
    U32 low, high, aux;
    __asm__ __volatile__("rdtscp" : "=a"(low), "=d"(high), "=c"(aux));
    if (TscAux) *TscAux = aux;
    return ((U64)high << 32) | low;
}

U8 __INTRIN__ __BitScanForward(U32* Index, U32 Mask)
{
    if (Mask == 0) return 0;
    __asm__ __volatile__("bsfl %1, %0" : "=r"(*Index) : "r"(Mask));
    return 1;
}

U8 __INTRIN__ __BitScanReverse(U32* Index, U32 Mask)
{
    if (Mask == 0) return 0;
    __asm__ __volatile__("bsrl %1, %0" : "=r"(*Index) : "r"(Mask));
    return 1;
}

void __INTRIN__ __MemoryBarrier(void)
{
    __asm__ __volatile__("mfence" ::: "memory");
}

void __INTRIN__ __ReadBarrier(void)
{
    __asm__ __volatile__("lfence" ::: "memory");
}

void __INTRIN__ __WriteBarrier(void)
{
    __asm__ __volatile__("sfence" ::: "memory");
}

void __INTRIN__ __LoadTskReg(U16 Selector)
{
    __asm__ __volatile__("ltr %0" : : "r"(Selector));
}

U16 __INTRIN__ __StoreTskReg(void)
{
    U16 selector;
    __asm__ __volatile__("str %0" : "=r"(selector));
    return selector;
}

U8 __INTRIN__ __ReadFS8(U32 Offset)
{
    U8 val;
    __asm__ __volatile__("movb %%fs:%1, %0" : "=q"(val) : "m"(*(U8*)(UPTR)Offset));
    return val;
}

U16 __INTRIN__ __ReadFS16(U32 Offset)
{
    U16 val;
    __asm__ __volatile__("movw %%fs:%1, %0" : "=r"(val) : "m"(*(U16*)(UPTR)Offset));
    return val;
}

U8 __INTRIN__ __ReadGS8(U32 Offset)
{
    U8 val;
    __asm__ __volatile__("movb %%gs:%1, %0" : "=q"(val) : "m"(*(U8*)(UPTR)Offset));
    return val;
}

U16 __INTRIN__ __ReadGS16(U32 Offset)
{
    U16 val;
    __asm__ __volatile__("movw %%gs:%1, %0" : "=r"(val) : "m"(*(U16*)(UPTR)Offset));
    return val;
}

U32 __INTRIN__ __ReadGS32(U32 Offset)
{
    U32 val;
    __asm__ __volatile__("movl %%gs:%1, %0" : "=r"(val) : "m"(*(U32*)(UPTR)Offset));
    return val;
}

void __INTRIN__ __WriteFS32(U32 Offset, U32 Data)
{
    __asm__ __volatile__("movl %0, %%fs:%1" : : "r"(Data), "m"(*(U32*)(UPTR)Offset) : "memory");
}

U8 __INTRIN__ __InterlockedBitTestAndSet(volatile I32* Base, I32 Bit)
{
    U8 old_bit;
    __asm__ __volatile__(
        "lock btsl %2, %1;\n\t"
        "setc %0"
        : "=r"(old_bit), "+m"(*Base)
        : "Ir"(Bit)
        : "memory"
    );
    return old_bit;
}

U8 __INTRIN__ __InterlockedBitTestAndReset(volatile I32* Base, I32 Bit)
{
    U8 old_bit;
    __asm__ __volatile__(
        "lock btrl %2, %1;\n\t"
        "setc %0"
        : "=r"(old_bit), "+m"(*Base)
        : "Ir"(Bit)
        : "memory"
    );
    return old_bit;
}

#endif

/* EOF */