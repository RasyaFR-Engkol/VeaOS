#pragma once

#include <procbind.h>
#include <ultypes2.h>
#include <veastatus.h>

typedef struct _VEA_TIB {
    PVOID ExceptionList;
    PVOID StackBase;                                      // FS:[0x04]
    PVOID StackLimit;                                     // FS:[0x08]
    PVOID SubSystemTib;                                   // FS:[0x0C]
    PVOID FiberData;                                      // FS:[0x10]
    PVOID ArbitraryUserPointer;                           // FS:[0x14]
    struct _VEA_TIB *Self;                                 // FS:[0x18] (User-mode Self-Pointer)
} VEA_TIB, *PVEA_TIB;

FORCEINLINE struct _TEB *NtCurrentTeb(VOID)
{
    void* pTEB;
    __asm__("movl %%fs:0x18, %0" : "=r" (pTEB));
    return pTEB;
}

//
// PCTEB
//
typedef struct _TEB_ACTIVE_FRAME_CONTEXT
{
    ULONG Flags;
    PSTR FrameName;
} TEB_ACTIVE_FRAME_CONTEXT, *PTEB_ACTIVE_FRAME_CONTEXT;
typedef const struct _TEB_ACTIVE_FRAME_CONTEXT *PCTEB_ACTIVE_FRAME_CONTEXT;

typedef struct _TEB_ACTIVE_FRAME
{
    ULONG Flags;
    struct _TEB_ACTIVE_FRAME *Previous;
    PCTEB_ACTIVE_FRAME_CONTEXT Context;
} TEB_ACTIVE_FRAME, *PTEB_ACTIVE_FRAME;
typedef const struct _TEB_ACTIVE_FRAME *PCTEB_ACTIVE_FRAME;

//
// Thread Environment Block (TEB) - Modern NT (Windows 10/11)
//
typedef struct _TEB
{
    VEA_TIB                 NtTib;
    PVOID                   EnvironmentPointer;
    CLIENT_ID               ClientId;
    PVOID                   ActiveRpcHandle;
    PVOID                   ThreadLocalStoragePointer;
    struct _PEB            *ProcessEnvironmentBlock;
    ULONG                   LastErrorValue;
    ULONG                   CountOfOwnedCriticalSections;
    PVOID                   CsrClientThread;
    PVOID                   Win32ThreadInfo;
    ULONG                   User32Reserved[26];
    ULONG                    UserReserved[5];
    PVOID                   WOW32Reserved;
    LCID                    CurrentLocale;
    ULONG                   FpSoftwareStatusRegister;

    PVOID                   ReservedForDebuggerInstrumentation[16];
    PVOID                   SystemReserved1[38];

    LONG                    ExceptionCode;
#ifdef _VEA64B_
    UCHAR                   Padding0[4];
#endif

    // PACTIVATION_CONTEXT_STACK ActivationContextStackPointer;

    PULONG_PTR              InstrumentationCallbackSp;
    PULONG_PTR              InstrumentationCallbackPreviousPc;
    PULONG_PTR              InstrumentationCallbackPreviousSp;
#ifdef _VEA64B_
    ULONG                   TxFsContext;
    UCHAR                   InstrumentationCallbackDisabled;
    UCHAR                   Padding1[3];
#else
    UCHAR                   InstrumentationCallbackDisabled;
    UCHAR                   SpareBytes[23];
    ULONG                   TxFsContext;
#endif

    CLIENT_ID               RealClientId;
    PVOID                   GdiCachedProcessHandle;
    ULONG                   GdiClientPID;
    ULONG                   GdiClientTID;
    PVOID                   GdiThreadLocalInfo;
    PVOID                   Win32ClientInfo[62];
    PVOID                   glDispatchTable[233];
    ULONG_PTR               glReserved1[29];
    PVOID                   glReserved2;
    PVOID                   glSectionInfo;
    PVOID                   glSection;
    PVOID                   glTable;
    PVOID                   glCurrentRC;
    PVOID                   glContext;
    ULONG                   LastStatusValue;
#ifdef _VEA64B_
    UCHAR                   Padding2[4];
#endif
    UNICODE_STRING          StaticUnicodeString;
    WCHAR                   StaticUnicodeBuffer[261];
#ifdef _VEA64B_
    UCHAR                   Padding3[6];
#endif
    PVOID                   DeallocationStack;
    PVOID                   TlsSlots[64];
    LIST_ENTRY              TlsLinks;
    PVOID                   Vdm;
    PVOID                   ReservedForNtRpc;
    PVOID                   DbgSsReserved[2];
    ULONG                   HardErrorMode;
#ifdef _VEA64B_
    UCHAR                   Padding4[4];
#endif
    PVOID                   Instrumentation[13 - sizeof(GUID) / sizeof(PVOID)];
    GUID                    ActivityId;
    PVOID                   SubProcessTag;
    PVOID                   PerflibData;
    PVOID                   EtwTraceData;
    PVOID                   WinSockData;
    ULONG                   GdiBatchCount;
    union
    {
        PROCESSOR_NUMBER    CurrentIdealProcessor;
        ULONG32             IdealProcessorValue;
        struct
        {
            UCHAR           ReservedPad0;
            UCHAR           ReservedPad1;
            UCHAR           ReservedPad2;
            UCHAR           IdealProcessor;
        };
    };
    ULONG                   GuaranteedStackBytes;
#ifdef _VEA64B_
    UCHAR                   Padding5[4];
#endif
    PVOID                   ReservedForPerf;
    PVOID                   ReservedForOle;
    ULONG                   WaitingOnLoaderLock;
#ifdef _VEA64B_
    UCHAR                   Padding6[4];
#endif
    PVOID                   SavedPriorityState;
    PULONG_PTR              ReservedForCodeCoverage;
    PULONG_PTR              ThreadPoolData;
    PVOID*                  TlsExpansionSlots;
#ifdef _VEA64B_
    PVOID                   DeallocationBStore;
    PVOID                   BStoreLimit;
#endif
    ULONG                   MuiGeneration;
    ULONG                   IsImpersonating;
    PVOID                   NlsCache;
    PVOID                   pShimData;
    USHORT                  HeapVirtualAffinity;
    USHORT                  LowFragHeapDataSlot;
#ifdef _VEA64B_
    UCHAR                   Padding7[4];
#endif
    PHANDLE                 CurrentTransactionHandle;
    PTEB_ACTIVE_FRAME       ActiveFrame;
    PVOID                   FlsData;

    PVOID                   PreferredLanguages;
    PVOID                   UserPrefLanguages;
    PVOID                   MergedPrefLanguages;
    ULONG                   MuiImpersonation;
    union
    {
        USHORT              CrossTebFlags;
        struct
        {
            USHORT          SpareCrossTebBits : 16;
        };
    };
    union
    {
        USHORT              SameTebFlags;
        struct
        {
            USHORT          DbgSafeThunkCall : 1;
            USHORT          DbgInDebugPrint : 1;
            USHORT          DbgHasFiberData : 1;
            USHORT          DbgSkipThreadAttach : 1;
            USHORT          DbgWerInShipAssertCode : 1;
            USHORT          DbgIssuedInitialBp : 1;
            USHORT          DbgClonedThread : 1;
            USHORT          SpareSameTebBits : 9;
        };
    };
    PVOID                   TxnScopeEnterCallback;
    PVOID                   TxnScopeExitCallback;
    PVOID                   TxnScopeContext;
    ULONG                   LockCount;

    LONG                    WowTebOffset;
    PVOID                   ResourceRetValue;
    PVOID                   ReservedForWdf;
    ULONG64                 ReservedForCrt;
    GUID                    EffectiveContainerId;
} TEB, *PTEB;

#define NtCurrentPeb() (NtCurrentTeb()->ProcessEnvironmentBlock)

#if defined(_VEA64B_)
    #define GDI_HANDLE_BUFFER_SIZE 60
#else
    #define GDI_HANDLE_BUFFER_SIZE 34
#endif

typedef VEASTATUS
(VEAPI *PPOST_PROCESS_INIT_ROUTINE)(
    VOID
);

typedef struct _LDR_MODULE
{
    LIST_ENTRY InLoadOrderLinks;
    LIST_ENTRY InInitializationOrderLinks;
    PVOID      Base;
    PVOID      EntryPoint;
    PVOID      InitFunction;
    LONG       LoadCount;
    ULONG      Flags;
    CHAR       Name[64];
    PELF32_DYN Dynamic;
    PSTR       StrTab;
    PVOID      InitArray;
    ULONG      InitArrayCount;
    PVOID       FiniFunction;
    PVOID       FiniArray;
    ULONG       FiniArrayCount;
} LDR_MODULE, *PLDR_MODULE;

typedef struct _PEB_LDR_DATA
{
    ULONG Length;
    BOOLEAN Initialized;
    PVOID SsHandle;
    LIST_ENTRY InLoadOrderModuleList;
    LIST_ENTRY InMemoryOrderModuleList;
    LIST_ENTRY InInitializationOrderModuleList;
    PVOID EntryInProgress;
    UCHAR ShutdownInProgress;
    PVOID ShutdownThreadId;
} PEB_LDR_DATA, *PPEB_LDR_DATA;

typedef struct _PEB_FREE_BLOCK
{
    struct _PEB_FREE_BLOCK* Next;
    ULONG Size;
} PEB_FREE_BLOCK, *PPEB_FREE_BLOCK;


typedef struct _PEB
{
    BOOLEAN InheritedAddressSpace;
    BOOLEAN ReadImageFileExecOptions;
    BOOLEAN BeingDebugged;
    union
    {
        BOOLEAN BitField;
        struct
        {
            BOOLEAN ImageUsesLargePages : 1;
            BOOLEAN IsProtectedProcess : 1;
            BOOLEAN IsLegacyProcess : 1;
            BOOLEAN IsImageDynamicallyRelocated : 1;
            BOOLEAN SkipPatchingUser32Forwarders : 1;
            BOOLEAN SpareBits : 3;
        };
    };
    HANDLE Mutant;
    PVOID ImageBaseAddress;
    PPEB_LDR_DATA Ldr;
    struct _RTL_USER_PROCESS_PARAMETERS* ProcessParameters;
    PVOID SubSystemData;
    PVOID ProcessHeap;
    struct _RTL_CRITICAL_SECTION* FastPebLock;
    PVOID AltThunkSListPtr;
    PVOID IFEOKey;
    union
    {
        ULONG CrossProcessFlags;
        struct
        {
            ULONG ProcessInJob : 1;
            ULONG ProcessInitializing : 1;
            ULONG ProcessUsingVEH : 1;
            ULONG ProcessUsingVCH : 1;
            ULONG ReservedBits0 : 28;
        };
    };
    union
    {
        PVOID KernelCallbackTable;
        PVOID UserSharedInfoPtr;
    };
    ULONG SystemReserved[1];
    ULONG SpareUlong; // AltThunkSListPtr32
    PPEB_FREE_BLOCK FreeList;
    ULONG TlsExpansionCounter;
    PVOID TlsBitmap;
    ULONG TlsBitmapBits[2];
    PVOID ReadOnlySharedMemoryBase;
    PVOID ReadOnlySharedMemoryHeap;
    PVOID* ReadOnlyStaticServerData;
    PVOID AnsiCodePageData;
    PVOID OemCodePageData;
    PVOID UnicodeCaseTableData;
    ULONG NumberOfProcessors;
    ULONG NtGlobalFlag;
    LARGE_INTEGER CriticalSectionTimeout;
    ULONG_PTR HeapSegmentReserve;
    ULONG_PTR HeapSegmentCommit;
    ULONG_PTR HeapDeCommitTotalFreeThreshold;
    ULONG_PTR HeapDeCommitFreeBlockThreshold;
    ULONG NumberOfHeaps;
    ULONG MaximumNumberOfHeaps;
    PVOID* ProcessHeaps;
    PVOID GdiSharedHandleTable;
    PVOID ProcessStarterHelper;
    ULONG GdiDCAttributeList;
    struct _RTL_CRITICAL_SECTION* LoaderLock;
    ULONG OSMajorVersion;
    ULONG OSMinorVersion;
    USHORT OSBuildNumber;
    USHORT OSCSDVersion;
    ULONG OSPlatformId;
    ULONG ImageSubsystem;
    ULONG ImageSubsystemMajorVersion;
    ULONG ImageSubsystemMinorVersion;
    ULONG_PTR ImageProcessAffinityMask;
    ULONG GdiHandleBuffer[GDI_HANDLE_BUFFER_SIZE];
    PPOST_PROCESS_INIT_ROUTINE PostProcessInitRoutine;
    PVOID TlsExpansionBitmap;
    ULONG TlsExpansionBitmapBits[32];
    ULONG SessionId;
    ULARGE_INTEGER AppCompatFlags;
    ULARGE_INTEGER AppCompatFlagsUser;
    PVOID pShimData;
    PVOID AppCompatInfo;
    UNICODE_STRING CSDVersion;
    struct _ACTIVATION_CONTEXT_DATA* ActivationContextData;
    struct _ASSEMBLY_STORAGE_MAP* ProcessAssemblyStorageMap;
    struct _ACTIVATION_CONTEXT_DATA* SystemDefaultActivationContextData;
    struct _ASSEMBLY_STORAGE_MAP* SystemAssemblyStorageMap;
    ULONG_PTR MinimumStackCommit;
    PVOID* FlsCallback;
    LIST_ENTRY FlsListHead;
    PVOID FlsBitmap;
    ULONG FlsBitmapBits[4];
    ULONG FlsHighIndex;
    PVOID WerRegistrationData;
    PVOID WerShipAssertPtr;
} PEB, *PPEB;

#define LDRP_FLAG_EXE        0x00000001
#define LDRP_FLAG_PROCESSED  0x00000002   /* relokasi sudah dilakukan */
#define LDRP_FLAG_LOADING    0x00000004   /* untuk deteksi siklus nanti */
#define LDRP_FLAG_INITED     0x00000008   /* DT_INIT sudah dipanggil */
#define LDRP_FLAG_DEINITED   0x00000010

#define DLL_PROCESS_DETACH    0
#define DLL_PROCESS_ATTACH    1