// ULTYPES2
#pragma once

#include <procbind.h>

#define HANDLE_TO_INDEX(h) ((ULONG)(h) >> 2)
#define INDEX_TO_HANDLE(i) ((HANDLE)((i) << 2))

#define TEMPORARY_MAX_HANDLE 100

//
// This is for Kernel Worker
//

typedef enum _WORK_QUEUE_TYPE
{
    CriticalWorkQueue,
    DelayedWorkQueue,
    HyperCriticalWorkQueue,
    MaximumWorkQueue
} WORK_QUEUE_TYPE, *PWORK_QUEUE_TYPE;

//
// Ulp Worker Structure Information
//

typedef struct _UL_QUEUE_WORK_TYPE
{
    ULONG QueueDisabled:1;
    ULONG MakeThreadsAsNecessary:1;
    ULONG WaitMode:1;
    ULONG WorkerCount:29;
} UL_QUEUE_WORK_INFO, *PUL_QUEUE_WORK_INFO;

typedef VOID 
(VEAPI WORKER_THREAD_ROUTINE)(
    IN PVOID Parameter);
typedef WORKER_THREAD_ROUTINE *PWORKER_THREAD_ROUTINE;

typedef struct _UL_WORK_QUEUE_ITEM {
    LIST_ENTRY List;
    PWORKER_THREAD_ROUTINE WorkerRoutine;
    volatile PVOID Parameter;
} UL_WORK_QUEUE_ITEM, *PUL_WORK_QUEUE_ITEM;

#define EVENT_QUERY_STATE (0x0001)
#define EVENT_MODIFY_STATE (0x0002)
#define EVENT_ALL_ACCESS (VEA_STANDARD_RIGHTS_REQUIRED | VEA_SYNCHRONIZE | 0x3)

typedef struct _OBJECT_TYPE *POBJECT_TYPE;

extern UCHAR UlpInitializationPhase;
extern POBJECT_TYPE UlEventObjectType;

typedef enum _SYSTEM_INFORMATION_CLASS
{
    SystemBasicInformation                                = 0, // 0x0
    SystemProcessorInformation                            = 1, // 0x1
    SystemPerformanceInformation                          = 2, // 0x2
    SystemTimeOfDayInformation                            = 3, // 0x3
    SystemPathInformation                                 = 4, // 0x4 - Obsolete: Use KUSER_SHARED_DATA
    SystemProcessInformation                              = 5, // 0x5
    SystemCallCountInformation                            = 6, // 0x6
    SystemDeviceInformation                               = 7, // 0x7
    SystemProcessorPerformanceInformation                 = 8, // 0x8
    SystemFlagsInformation                                = 9, // 0x9
    SystemCallTimeInformation                             = 10, // 0xA
    SystemModuleInformation                               = 11, // 0xB
    SystemLocksInformation                                = 12, // 0xC
    SystemStackTraceInformation                           = 13, // 0xD
    SystemPagedPoolInformation                            = 14, // 0xE
    SystemNonPagedPoolInformation                         = 15, // 0xF
    SystemHandleInformation                               = 16, // 0x10
    SystemObjectInformation                               = 17, // 0x11
    SystemPageFileInformation                             = 18, // 0x12
    SystemVdmInstemulInformation                          = 19, // 0x13
    SystemVdmBopInformation                               = 20, // 0x14
    SystemFileCacheInformation                            = 21, // 0x15
    SystemPoolTagInformation                              = 22, // 0x16
    SystemInterruptInformation                            = 23, // 0x17
    SystemDpcBehaviorInformation                          = 24, // 0x18
    SystemFullMemoryInformation                           = 25, // 0x19
    SystemLoadGdiDriverInformation                        = 26, // 0x1A
    SystemUnloadGdiDriverInformation                      = 27, // 0x1B
    SystemTimeAdjustmentInformation                       = 28, // 0x1C
    SystemSummaryMemoryInformation                        = 29, // 0x1D
    SystemMirrorMemoryInformation                         = 30, // 0x1E
    SystemPerformanceTraceInformation                     = 31, // 0x1F
    SystemObsolete0                                       = 32, // 0x20
    SystemExceptionInformation                            = 33, // 0x21
    SystemCrashDumpStateInformation                       = 34, // 0x22
    SystemKernelDebuggerInformation                       = 35, // 0x23
    SystemContextSwitchInformation                        = 36, // 0x24
    SystemRegistryQuotaInformation                        = 37, // 0x25
    SystemExtendServiceTableInformation                   = 38, // 0x26
    SystemPrioritySeperation                              = 39, // 0x27
    SystemVerifierAddDriverInformation                    = 40, // 0x28
    SystemVerifierRemoveDriverInformation                 = 41, // 0x29
    SystemProcessorIdleInformation                        = 42, // 0x2A
    SystemLegacyDriverInformation                         = 43, // 0x2B
    SystemCurrentTimeZoneInformation                      = 44, // 0x2C
    SystemLookasideInformation                            = 45, // 0x2D
    SystemTimeSlipNotification                            = 46, // 0x2E
    SystemSessionCreate                                   = 47, // 0x2F
    SystemSessionDetach                                   = 48, // 0x30
    SystemSessionInformation                              = 49, // 0x31
    SystemRangeStartInformation                           = 50, // 0x32
    SystemVerifierInformation                             = 51, // 0x33
    SystemVerifierThunkExtend                             = 52, // 0x34 - Win 11: SystemVerifierReserved
    SystemSessionProcessInformation                       = 53, // 0x35
    SystemLoadGdiDriverInSystemSpace                      = 54, // 0x36
    SystemNumaProcessorMap                                = 55, // 0x37
    SystemPrefetcherInformation                           = 56, // 0x38
    SystemExtendedProcessInformation                      = 57, // 0x39
    SystemRecommendedSharedDataAlignment                  = 58, // 0x3A
    SystemComPlusPackage                                  = 59, // 0x3B
    SystemNumaAvailableMemory                             = 60, // 0x3C
    SystemProcessorPowerInformation                       = 61, // 0x3D
    SystemEmulationBasicInformation                       = 62, // 0x3E
    SystemEmulationProcessorInformation                   = 63, // 0x3F
    SystemExtendedHandleInformation                       = 64, // 0x40
    SystemLostDelayedWriteInformation                     = 65, // 0x41
    SystemBigPoolInformation                              = 66, // 0x42
    SystemSessionPoolTagInformation                       = 67, // 0x43
    SystemSessionMappedViewInformation                    = 68, // 0x44
    SystemHotpatchInformation                             = 69, // 0x45
    SystemObjectSecurityMode                              = 70, // 0x46
    SystemWatchdogTimerHandler                            = 71, // 0x47
    SystemWatchdogTimerInformation                        = 72, // 0x48
    SystemLogicalProcessorInformation                     = 73, // 0x49
    SystemWow64SharedInformationObsolete                  = 74, // 0x4A
    SystemRegisterFirmwareTableInformationHandler         = 75, // 0x4B
    SystemFirmwareTableInformation                        = 76, // 0x4C

#if (NTDDI_VERSION >= NTDDI_VISTA) || defined(__REACTOS__)
    SystemModuleInformationEx                             = 77, // 0x4D
    SystemVerifierTriageInformation                       = 78, // 0x4E
    SystemSuperfetchInformation                           = 79, // 0x4F
    SystemMemoryListInformation                           = 80, // 0x50
    SystemFileCacheInformationEx                          = 81, // 0x51
    SystemThreadPriorityClientIdInformation               = 82, // 0x52
    SystemProcessorIdleCycleTimeInformation               = 83, // 0x53
    SystemVerifierCancellationInformation                 = 84, // 0x54
    SystemProcessorPowerInformationEx                     = 85, // 0x55
    SystemRefTraceInformation                             = 86, // 0x56
    SystemSpecialPoolInformation                          = 87, // 0x57
    SystemProcessIdInformation                            = 88, // 0x58
    SystemErrorPortInformation                            = 89, // 0x59
    SystemBootEnvironmentInformation                      = 90, // 0x5A
    SystemHypervisorInformation                           = 91, // 0x5B
    SystemVerifierInformationEx                           = 92, // 0x5C
    SystemTimeZoneInformation                             = 93, // 0x5D
    SystemImageFileExecutionOptionsInformation            = 94, // 0x5E
    SystemCoverageInformation                             = 95, // 0x5F
    SystemPrefetchPatchInformation                        = 96, // 0x60
    SystemVerifierFaultsInformation                       = 97, // 0x61
    SystemSystemPartitionInformation                      = 98, // 0x62
    SystemSystemDiskInformation                           = 99, // 0x63
    SystemProcessorPerformanceDistribution                = 100, // 0x64
    SystemNumaProximityNodeInformation                    = 101, // 0x65
    SystemDynamicTimeZoneInformation                      = 102, // 0x66
    SystemCodeIntegrityInformation                        = 103, // 0x67
    SystemProcessorMicrocodeUpdateInformation             = 104, // 0x68
    SystemProcessorBrandString                            = 105, // 0x69
    SystemVirtualAddressInformation                       = 106, // 0x6A
#endif // (NTDDI_VERSION >= NTDDI_VISTA)

#if (NTDDI_VERSION >= NTDDI_WIN7) || defined(__REACTOS__)
    SystemLogicalProcessorAndGroupInformation             = 107, // 0x6B
    SystemProcessorCycleTimeInformation                   = 108, // 0x6C
    SystemStoreInformation                                = 109, // 0x6D
    SystemRegistryAppendString                            = 110, // 0x6E
    SystemAitSamplingValue                                = 111, // 0x6F
    SystemVhdBootInformation                              = 112, // 0x70
    SystemCpuQuotaInformation                             = 113, // 0x71
    SystemNativeBasicInformation                          = 114, // 0x72
    SystemErrorPortTimeouts                               = 115, // 0x73
    SystemLowPriorityIoInformation                        = 116, // 0x74
    SystemBootEntropyInformation                          = 117, // 0x75
    SystemVerifierCountersInformation                     = 118, // 0x76
    SystemPagedPoolInformationEx                          = 119, // 0x77
    SystemSystemPtesInformationEx                         = 120, // 0x78
    SystemNodeDistanceInformation                         = 121, // 0x79
    SystemAcpiAuditInformation                            = 122, // 0x7A
    SystemBasicPerformanceInformation                     = 123, // 0x7B
    SystemQueryPerformanceCounterInformation              = 124, // 0x7C
#endif // (NTDDI_VERSION >= NTDDI_WIN7)

#if (NTDDI_VERSION >= NTDDI_WIN8)
    SystemSessionBigPoolInformation                       = 125, // 0x7D
    SystemBootGraphicsInformation                         = 126, // 0x7E
    SystemScrubPhysicalMemoryInformation                  = 127, // 0x7F
    SystemBadPageInformation                              = 128, // 0x80
    SystemProcessorProfileControlArea                     = 129, // 0x81
    SystemCombinePhysicalMemoryInformation                = 130, // 0x82
    SystemEntropyInterruptTimingInformation               = 131, // 0x83
    SystemConsoleInformation                              = 132, // 0x84
    SystemPlatformBinaryInformation                       = 133, // 0x85
    SystemThrottleNotificationInformation                 = 134, // 0x86 - 6.2 only
    SystemPolicyInformation                               = 134, // 0x86 - 6.3 and higher
    SystemHypervisorProcessorCountInformation             = 135, // 0x87
    SystemDeviceDataInformation                           = 136, // 0x88
    SystemDeviceDataEnumerationInformation                = 137, // 0x89
    SystemMemoryTopologyInformation                       = 138, // 0x8A
    SystemMemoryChannelInformation                        = 139, // 0x8B
    SystemBootLogoInformation                             = 140, // 0x8C
    SystemProcessorPerformanceInformationEx               = 141, // 0x8D
    SystemSpare0                                          = 142, // 0x8E - 6.2 to 1511
    SystemCriticalProcessErrorLogInformation              = 142, // 0x8E - 1607 and higher
    SystemSecureBootPolicyInformation                     = 143, // 0x8F
    SystemPageFileInformationEx                           = 144, // 0x90
    SystemSecureBootInformation                           = 145, // 0x91
    SystemEntropyInterruptTimingRawInformation            = 146, // 0x92
    SystemPortableWorkspaceEfiLauncherInformation         = 147, // 0x93
    SystemFullProcessInformation                          = 148, // 0x94
#endif // (NTDDI_VERSION >= NTDDI_WIN8)

#if (NTDDI_VERSION >= NTDDI_WINBLUE)
    SystemKernelDebuggerInformationEx                     = 149, // 0x95
    SystemBootMetadataInformation                         = 150, // 0x96
    SystemSoftRebootInformation                           = 151, // 0x97
    SystemElamCertificateInformation                      = 152, // 0x98
    SystemOfflineDumpConfigInformation                    = 153, // 0x99
    SystemProcessorFeaturesInformation                    = 154, // 0x9A
    SystemRegistryReconciliationInformation               = 155, // 0x9B
    SystemEdidInformation                                 = 156, // 0x9C
#endif // (NTDDI_VERSION >= NTDDI_WINBLUE)

#if (NTDDI_VERSION >= NTDDI_WIN10)
    SystemManufacturingInformation                        = 157, // 0x9D
    SystemEnergyEstimationConfigInformation               = 158, // 0x9E
    SystemHypervisorDetailInformation                     = 159, // 0x9F
    SystemProcessorCycleStatsInformation                  = 160, // 0xA0
    SystemVmGenerationCountInformation                    = 161, // 0xA1
    SystemTrustedPlatformModuleInformation                = 162, // 0xA2
    SystemKernelDebuggerFlags                             = 163, // 0xA3
    SystemCodeIntegrityPolicyInformation                  = 164, // 0xA4
    SystemIsolatedUserModeInformation                     = 165, // 0xA5
    SystemHardwareSecurityTestInterfaceResultsInformation = 166, // 0xA6
    SystemSingleModuleInformation                         = 167, // 0xA7
    SystemAllowedCpuSetsInformation                       = 168, // 0xA8
    SystemVsmProtectionInformation                        = 169, // 0xA9 - aka SystemDmaProtectionInformation
    SystemInterruptCpuSetsInformation                     = 170, // 0xAA
    SystemSecureBootPolicyFullInformation                 = 171, // 0xAB
    SystemCodeIntegrityPolicyFullInformation              = 172, // 0xAC
    SystemAffinitizedInterruptProcessorInformation        = 173, // 0xAD
    SystemRootSiloInformation                             = 174, // 0xAE
    SystemCpuSetInformation                               = 175, // 0xAF
    SystemCpuSetTagInformation                            = 176, // 0xB0
    SystemWin32WerStartCallout                            = 177, // 0xB1
    SystemSecureKernelProfileInformation                  = 178, // 0xB2
    SystemCodeIntegrityPlatformManifestInformation        = 179, // 0xB3
    SystemInterruptSteeringInformation                    = 180, // 0xB4
    SystemSupportedProcessorArchitectures                 = 181, // 0xB5
    SystemMemoryUsageInformation                          = 182, // 0xB6
    SystemCodeIntegrityCertificateInformation             = 183, // 0xB7
    SystemPhysicalMemoryInformation                       = 184, // 0xB8
    SystemControlFlowTransition                           = 185, // 0xB9
    SystemKernelDebuggingAllowed                          = 186, // 0xBA
    SystemActivityModerationExeState                      = 187, // 0xBB
    SystemActivityModerationUserSettings                  = 188, // 0xBC
    SystemCodeIntegrityPoliciesFullInformation            = 189, // 0xBD
    SystemCodeIntegrityUnlockInformation                  = 190, // 0xBE
    SystemIntegrityQuotaInformation                       = 191, // 0xBF
    SystemFlushInformation                                = 192, // 0xC0
    SystemProcessorIdleMaskInformation                    = 193, // 0xC1
    SystemSecureDumpEncryptionInformation                 = 194, // 0xC2
    SystemWriteConstraintInformation                      = 195, // 0xC3
    SystemKernelVaShadowInformation                       = 196, // 0xC4
    SystemHypervisorSharedPageInformation                 = 197, // 0xC5
    SystemFirmwareBootPerformanceInformation              = 198, // 0xC6
    SystemCodeIntegrityVerificationInformation            = 199, // 0xC7
    SystemFirmwarePartitionInformation                    = 200, // 0xC8
    SystemSpeculationControlInformation                   = 201, // 0xC9
    SystemDmaGuardPolicyInformation                       = 202, // 0xCA
    SystemEnclaveLaunchControlInformation                 = 203, // 0xCB
    SystemWorkloadAllowedCpuSetsInformation               = 204, // 0xCC
    SystemCodeIntegrityUnlockModeInformation              = 205, // 0xCD
    SystemLeapSecondInformation                           = 206, // 0xCE
    SystemFlags2Information                               = 207, // 0xCF
    SystemSecurityModelInformation                        = 208, // 0xD0
    SystemCodeIntegritySyntheticCacheInformation          = 209, // 0xD1
    SystemFeatureConfigurationInformation                 = 210, // 0xD2
    SystemFeatureConfigurationSectionInformation          = 211, // 0xD3
    SystemFeatureUsageSubscriptionInformation             = 212, // 0xD4
    SystemSecureSpeculationControlInformation             = 213, // 0xD5
    SystemSpacesBootInformation                           = 214, // 0xD6
    SystemFwRamdiskInformation                            = 215, // 0xD7
    SystemWheaIpmiHardwareInformation                     = 216, // 0xD8
    SystemDifSetRuleClassInformation                      = 217, // 0xD9
    SystemDifClearRuleClassInformation                    = 218, // 0xDA
    SystemDifApplyPluginVerificationOnDriver              = 219, // 0xDB
    SystemDifRemovePluginVerificationOnDriver             = 220, // 0xDC
    SystemShadowStackInformation                          = 221, // 0xDD
    SystemBuildVersionInformation                         = 222, // 0xDE
    SystemPoolLimitInformation                            = 223, // 0xDF
    SystemCodeIntegrityAddDynamicStore                    = 224, // 0xE0
    SystemCodeIntegrityClearDynamicStores                 = 225, // 0xE1
    SystemDifPoolTrackingInformation                      = 226, // 0xE2 - Win 11
    SystemPoolZeroingInformation                          = 227, // 0xE3
#endif // (NTDDI_VERSION >= NTDDI_WIN10)

#if (NTDDI_VERSION >= NTDDI_WIN11)
    SystemDpcWatchdogInformation                          = 228, // 0xE4
    SystemDpcWatchdogInformation2                         = 229, // 0xE5
    SystemSupportedProcessorArchitectures2                = 230, // 0xE6
    SystemSingleProcessorRelationshipInformation          = 231, // 0xE7
    SystemXfgCheckFailureInformation                      = 232, // 0xE8
    SystemIommuStateInformation                           = 233, // 0xE9
    SystemHypervisorMinrootInformation                    = 234, // 0xEA
    SystemHypervisorBootPagesInformation                  = 235, // 0xEB
    SystemPointerAuthInformation                          = 236, // 0xEC
    SystemSecureKernelDebuggerInformation                 = 237, // 0xED
    SystemOriginalImageFeatureInformation                 = 238, // 0xEE
    SystemMemoryNumaInformation                           = 239, // 0xEF
    SystemMemoryNumaPerformanceInformation                = 240, // 0xF0
    SystemCodeIntegritySignedPoliciesFullInformation      = 241, // 0xF1
    SystemSecureSecretsInformation                        = 242, // 0xF2
    SystemTrustedAppsRuntimeInformation                   = 243, // 0xF3
    SystemBadPageInformationEx                            = 244, // 0xF4
    SystemResourceDeadlockTimeout                         = 245, // 0xF5
    SystemBreakOnContextUnwindFailureInformation          = 246, // 0xF6
    SystemOslRamdiskInformation                           = 247, // 0xF7
#endif // (NTDDI_VERSION >= NTDDI_WIN11)

    MaxSystemInfoClass
} SYSTEM_INFORMATION_CLASS, *PSYSTEM_INFORMATION_CLASS;

typedef struct _SYSTEM_BASIC_INFORMATION
{
    ULONG Reserved;
    ULONG TimerResolution;
    ULONG PageSize;
    ULONG NumberOfPhysicalPages;
    ULONG LowestPhysicalPageNumber;
    ULONG HighestPhysicalPageNumber;
    ULONG AllocationGranularity;
    ULONG_PTR MinimumUserModeAddress;
    ULONG_PTR MaximumUserModeAddress;
    ULONG_PTR ActiveProcessorsAffinityMask;
    CHAR NumberOfProcessors;
} SYSTEM_BASIC_INFORMATION, *PSYSTEM_BASIC_INFORMATION;

typedef struct _HANDLE_TABLE_ENTRY {
    PVOID Object;          // Pointer ke Header/Body Objek (NULL jika entry ini kosong)
    ULONG GrantedAccess;   // Hak akses untuk handle ini
    ULONG NextFreeIndex;   // Index ke entry kosong berikutnya (hanya valid jika Object == NULL)
    PVOID OwnerThread;     // Thread yang menerbitkan dan memiliki handle ini
} HANDLE_TABLE_ENTRY, *PHANDLE_TABLE_ENTRY;

typedef struct _CLIENT_ID {
    HANDLE UniqueProcess; 
    HANDLE UniqueThread;  
} CLIENT_ID, *PCLIENT_ID;

/* EOF */