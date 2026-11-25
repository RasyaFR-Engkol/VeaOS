#pragma once

#include "procbind.h"
#include "obtype.h"
#include <teb.h>

typedef struct _ETHREAD {
    KTHREAD Tcb;                   // HARUS PALING ATAS!
    CLIENT_ID Cid;        // TID PID (Thread ID)
    LIST_ENTRY ThreadListEntry;    // Nyambungin thread ini ke ThreadListHead milik KPROCESS
    PVOID StartAddress;            // Alamat fungsi awal saat thread ini dibikin
    VEASTATUS ExitStatus;           // Kalau thread mati, dia ninggalin kode error di sini
    struct _EPROCESS *ThreadProcess;
    KSEMAPHORE KpcReplySemaphore;
    LIST_ENTRY KpcReplyChain;
    LIST_ENTRY IrpList;
    LIST_ENTRY PostBlockList;
    LIST_ENTRY ActiveTimerListHead;
    KSPIN_LOCK ActiveTimerListLock;
    PVOID VEAStartAddres;

    struct
    {
        BOOLEAN ThreadDelete;
        BOOLEAN BreakOnTermination;
        BOOLEAN DeadThread;
    } Settings;

    LARGE_INTEGER CreateTime;
} ETHREAD, *PETHREAD;

typedef struct _EPROCESS {
    KPROCESS Pcb;                  // HARUS PALING ATAS! (Biar pointer EPROCESS bisa di-cast jadi KPROCESS)
    CLIENT_ID Cid;        // PID (Process ID)
    LIST_ENTRY ActiveProcessLinks; // Rantai yang nyambungin semua proses di VeaOS
    CHAR ImageFileName[16];        // Nama file, misal: "veakrnl.exe" atau "explorer.exe"
    PHANDLE_TABLE ObjectTable;             // Nanti dipakai buat nyimpen Handle (File, PnP Device, dll)
    struct _EPROCESS* ParentProcess; 
    WORKING_SET Vm;
    struct _PEB *Peb;
    ULONG JobStatus;

    struct
    {
        BOOLEAN ProcessDelete;
        BOOLEAN BreakOnTermination;
    } Settings;

    HANDLE UniqueProcessId;
    ULONG ActiveThreads;
    VEASTATUS ExitStatus;
    LIST_ENTRY ThreadListHead;
    HANDLE InheritedFromUniqueProcessId;
    PVOID SectionObject;
    PVOID SectionBaseAddress;
    LARGE_INTEGER CreateTime;
    RTL_AVL_TREE VadRoot;
    KSPIN_LOCK VadFreeHintLock;
    BOOLEAN AddressSpaceInitialized;
    BOOLEAN VmDeleted;
    ULONG CommitCharge;
    ULONG CommitChargePeak;
    KSPIN_LOCK VadLock;
    PVEA_TOKEN Token;
} EPROCESS, *PEPROCESS;

typedef struct _KRUNQUEUE {
    RTL_RB_TREE CfsTree;
    PKTHREAD CurrentThread;
    ULONG TotalWeight;
    ULONG RunningTasks;
    ULONGLONG MinVRuntime;
    ULONG CpuId;
    KSPIN_LOCK SpinLock;
} KRUNQUEUE, *PKRUNQUEUE;

// defaulted size
#define PtThreadStackSize KERNEL_STACK_SIZE

extern POBJECT_TYPE PtEthreadType;
extern POBJECT_TYPE PtEprocessType;
extern LIST_ENTRY PtProcessList;
extern PEPROCESS PtSystemProcess;
extern KTHREAD PtSystemIdleThread[32];
extern KRUNQUEUE PtRunQueue[MAX_CPU];
extern LIST_ENTRY PtDeadThreadListHead;
extern LIST_ENTRY PtLoadedModuleList;

typedef VOID
(VEAPI KSTART_ROUTINE)(
  _In_ PVOID StartContext);
typedef KSTART_ROUTINE *PKSTART_ROUTINE;

typedef
VOID
(VEAPI *PKSYSTEM_ROUTINE)(
    PKSTART_ROUTINE StartRoutine,
    PVOID StartContext
);

typedef struct _KSTART_FRAME
{
    PKSYSTEM_ROUTINE SystemRoutine;
    PKSTART_ROUTINE StartRoutine;
    PVOID StartContext;
    BOOLEAN UserThread;
} KSTART_FRAME, *PKSTART_FRAME;

/* Frame Utuh untuk Kernel Thread */
typedef struct _KKINIT_FRAME
{
    KSWITCH_FRAME CtxSwitchFrame; /* Offset 0 -> Lokasi Thread->KernelStack */
    KSTART_FRAME StartFrame;      /* Offset sizeof(KSWITCH_FRAME) */
} KKINIT_FRAME, *PKKINIT_FRAME;

/* Frame Utuh untuk User Thread (Ring 3) */
typedef struct _KUINIT_FRAME
{
    KSWITCH_FRAME CtxSwitchFrame; /* Offset 0 -> Lokasi Thread->KernelStack */
    KSTART_FRAME StartFrame;      /* Offset sizeof(KSWITCH_FRAME) */
    KTRAP_FRAME TrapFrame;        /* Offset sizeof(KSWITCH_FRAME) + sizeof(KSTART_FRAME) */
} KUINIT_FRAME, *PKUINIT_FRAME;

typedef struct _INITIAL_TEB
{
    PVOID PreviousStackBase;
    PVOID PreviousStackLimit;
    PVOID StackBase;
    PVOID StackLimit;
    PVOID AllocatedStackBase;
} INITIAL_TEB, *PINITIAL_TEB;

//
// Process Access Types
//
#define PROCESS_TERMINATE                       0x0001
#define PROCESS_CREATE_THREAD                   0x0002
#define PROCESS_SET_SESSIONID                   0x0004
#define PROCESS_VM_OPERATION                    0x0008
#define PROCESS_VM_READ                         0x0010
#define PROCESS_VM_WRITE                        0x0020
#define PROCESS_CREATE_PROCESS                  0x0080
#define PROCESS_SET_QUOTA                       0x0100
#define PROCESS_SET_INFORMATION                 0x0200
#define PROCESS_QUERY_INFORMATION               0x0400
#define PROCESS_SUSPEND_RESUME                  0x0800
#define PROCESS_QUERY_LIMITED_INFORMATION       0x1000

#define PROCESS_ALL_ACCESS  (VEA_STANDARD_RIGHTS_REQUIRED | VEA_SYNCHRONIZE | 0xFFF)
#define THREAD_ALL_ACCESS   (VEA_STANDARD_RIGHTS_REQUIRED | VEA_SYNCHRONIZE | 0xFFFF)

extern PVOID PtpSystemSoBase;
extern PVOID PtpSystemSoSection;
extern PHANDLE_TABLE PtpCidTable;

typedef struct _INITIAL_PEB
{
    BOOLEAN InheritedAddressSpace;
    BOOLEAN ReadImageFileExecOptions;
    BOOLEAN BeingDebugged;
    union
    {
        BOOLEAN BitField;
        struct
        {
            BOOLEAN ImageUsesLargePages:1;
            BOOLEAN IsProtectedProcess:1;
            BOOLEAN IsLegacyProcess:1;
            BOOLEAN SpareBits:5;
        };
    };
    HANDLE Mutant;
} INITIAL_PEB, *PINITIAL_PEB;

#define PROCESS_CREATE_FLAGS_BREAKAWAY              0x00000001
#define PROCESS_CREATE_FLAGS_NO_DEBUG_INHERIT       0x00000002
#define PROCESS_CREATE_FLAGS_INHERIT_HANDLES        0x00000004
#define PROCESS_CREATE_FLAGS_OVERRIDE_ADDRESS_SPACE 0x00000008
#define PROCESS_CREATE_FLAGS_LARGE_PAGES            0x00000010
#define PROCESS_CREATE_FLAGS_ALL_LARGE_PAGE_FLAGS   PROCESS_CREATE_FLAGS_LARGE_PAGES
#define PROCESS_CREATE_FLAGS_LEGAL_MASK             (PROCESS_CREATE_FLAGS_BREAKAWAY | \
                                                     PROCESS_CREATE_FLAGS_NO_DEBUG_INHERIT | \
                                                     PROCESS_CREATE_FLAGS_INHERIT_HANDLES | \
                                                     PROCESS_CREATE_FLAGS_OVERRIDE_ADDRESS_SPACE | \
                                                     PROCESS_CREATE_FLAGS_ALL_LARGE_PAGE_FLAGS)

#define PROCESS_PRIORITY_IDLE_MIN               0
#define PROCESS_PRIORITY_IDLE_LOW               1
#define PROCESS_PRIORITY_IDLE_MID               2
#define PROCESS_PRIORITY_IDLE                   3
#define PROCESS_PRIORITY_BELOW_NORMAL_MIN       4
#define PROCESS_PRIORITY_BELOW_NORMAL_LOW       5
#define PROCESS_PRIORITY_BELOW_NORMAL           6
#define PROCESS_PRIORITY_BELOW_NORMAL_MAX       7
#define PROCESS_PRIORITY_NORMAL                 8
#define PROCESS_PRIORITY_NORMAL_FOREGROUND      9
#define PROCESS_PRIORITY_NORMAL_BOOST           10
#define PROCESS_PRIORITY_NORMAL_MAX             11
#define PROCESS_PRIORITY_ABOVE_NORMAL_MIN       12
#define PROCESS_PRIORITY_ABOVE_NORMAL           13
#define PROCESS_PRIORITY_ABOVE_NORMAL_MAX       14
#define PROCESS_PRIORITY_HIGH_MIN               15
#define PROCESS_PRIORITY_HIGH                   16
#define PROCESS_PRIORITY_HIGH_BOOST             17
#define PROCESS_PRIORITY_HIGH_MAX               18
#define PROCESS_PRIORITY_CRITICAL_MIN           19
#define PROCESS_PRIORITY_CRITICAL               20
#define PROCESS_PRIORITY_CRITICAL_BOOST         21
#define PROCESS_PRIORITY_CRITICAL_MAX           22
#define PROCESS_PRIORITY_REALTIME_LOW           23
#define PROCESS_PRIORITY_REALTIME               24
#define PROCESS_PRIORITY_REALTIME_FOREGROUND    25
#define PROCESS_PRIORITY_REALTIME_BOOST         26
#define PROCESS_PRIORITY_REALTIME_HIGH          27
#define PROCESS_PRIORITY_HYPERCRITICAL_MIN      28
#define PROCESS_PRIORITY_HYPERCRITICAL          29
#define PROCESS_PRIORITY_HYPERCRITICAL_MAX      30
#define PROCESS_PRIORITY_MAXIMUM                31

extern const ULONG PtpPriorityToWeight[32];

#define NtCurrentProcess()                      ((HANDLE)(LONG_PTR)-1)
#define ZwCurrentProcess()                      NtCurrentProcess()
#define NtCurrentThread()                       ((HANDLE)(LONG_PTR)-2)
#define ZwCurrentThread()                       NtCurrentThread()

typedef enum _PROCESSINFOCLASS
{
    ProcessBasicInformation = 0,
    ProcessQuotaLimits = 1,
    ProcessIoCounters = 2,
    ProcessVmCounters = 3,
    ProcessTimes = 4,
    ProcessBasePriority = 5,
    ProcessRaisePriority = 6,
    ProcessDebugPort = 7,
    ProcessExceptionPort = 8,
    ProcessAccessToken = 9,
    ProcessLdtInformation = 10,
    ProcessLdtSize = 11,
    ProcessDefaultHardErrorMode = 12,
    ProcessIoPortHandlers = 13,
    ProcessPooledUsageAndLimits = 14,
    ProcessWorkingSetWatch = 15,
    ProcessUserModeIOPL = 16,
    ProcessEnableAlignmentFaultFixup = 17,
    ProcessPriorityClass = 18,
    ProcessWx86Information = 19,
    ProcessHandleCount = 20,
    ProcessAffinityMask = 21,
    ProcessPriorityBoost = 22,
    ProcessDeviceMap = 23,
    ProcessSessionId = 24,
    ProcessWow64Information = 26,
    ProcessImageFileName = 27,
    ProcessLKEYInformation = 28,
    MaxProcessInfoClass
} PROCESSINFOCLASS;

typedef struct _PROCESS_BASIC_INFORMATION
{
    VEASTATUS ExitStatus;
    PPEB PebBaseAddress;
    ULONG_PTR AffinityMask;
    UCHAR BasePriority;
    ULONG_PTR UniqueProcessId;
    ULONG_PTR InheritedFromUniqueProcessId;
} PROCESS_BASIC_INFORMATION, *PPROCESS_BASIC_INFORMATION;

#define PROCESS_DUP_HANDLE               (0x0040)

#define THREAD_TERMINATE                 0x0001
#define THREAD_SUSPEND_RESUME            0x0002
#define THREAD_ALERT                     0x0004
#define THREAD_GET_CONTEXT               0x0008
#define THREAD_SET_CONTEXT               0x0010
#define THREAD_SET_INFORMATION           0x0020
#define THREAD_SET_LIMITED_INFORMATION   0x0400
#define THREAD_QUERY_LIMITED_INFORMATION 0x0800

extern PVOID PtpSystemSoEntryPoint;
extern PVOID PtpSystemSoEntryPoint ;
extern PVOID KsUserApcDispatcher ;
extern PVOID KsUserExceptionDispatcher ;
extern PVOID KsUserCallbackDispatcher ;
extern PVOID KsRaiseUserExceptionDispatcher ;

typedef enum _VEAPI_THREAD_CALLOUT_TYPE
{
    VeapiThreadCalloutInitialize = 0,
    VeapiThreadCalloutExit       = 1
} VEAPI_THREAD_CALLOUT_TYPE;

typedef VEASTATUS (VEAPI *PVEAPI_PROCESS_CALLOUT)(
    IN PEPROCESS Process,
    IN BOOLEAN Create
);

typedef VEASTATUS (VEAPI *PVEAPI_THREAD_CALLOUT)(
    IN PETHREAD Thread,
    IN VEAPI_THREAD_CALLOUT_TYPE Type
);

typedef struct _VEAPI_CALLOUTS_FPNS
{
    PVEAPI_PROCESS_CALLOUT  ProcessCallout;
    PVEAPI_THREAD_CALLOUT   ThreadCallout;
    ULONG                   WindowStationObjectSize;
    ULONG                   DesktopObjectSize;
    POB_DELETE_ROUTINE      WindowStationDeleteProcedure;
    POB_PARSE_ROUTINE       WindowStationParseProcedure;
    POB_OKAYTOCLOSE_METHOD  WindowStationOkToCloseProcedure;
    POB_DELETE_ROUTINE      DesktopDeleteProcedure;
    POB_OPEN_METHOD         DesktopOpenProcedure;
    POB_CLOSE_METHOD        DesktopCloseProcedure;
    POB_OKAYTOCLOSE_METHOD  DesktopOkToCloseProcedure;
} VEAPI_CALLOUTS_FPNS, *PVEAPI_CALLOUTS_FPNS;

extern VEAPI_CALLOUTS_FPNS PtpVeapiCallouts;

typedef enum _PTVEAPIJOBCALLOUTTYPE
{
   PtVEAPIJobCalloutSetInformation = 0,
   PtVEAPIJobCalloutAddProcess = 1,
   PtVEAPIJobCalloutTerminate = 2,
} PTVEAPIJOBCALLOUTTYPE;

/* EOF */