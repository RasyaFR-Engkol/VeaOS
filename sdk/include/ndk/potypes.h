/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : potypes.h
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Saving Po Module Types
 ----- Effective since 10-2026 til FOREVER  ------------------------------- */

#pragma once

#include "ks.h"
#include "procbind.h"

/* Revision History ------------------------------------------------------
 * DATE       : 25-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file potypes.h
 * --------------------------------------------------------------------- */

typedef struct _PROCESSOR_PERF_STATE {
    UCHAR PercentFrequency;
    UCHAR MinCapacity;
    USHORT Power;
    UCHAR IncreaseLevel;
    UCHAR DecreaseLevel;
    USHORT Flags;
    ULONG IncreaseTime;
    ULONG DecreaseTime;
    ULONG IncreaseCount;
    ULONG DecreaseCount;
    ULONGLONG PerformanceTime;
} PROCESSOR_PERF_STATE, *PPROCESSOR_PERF_STATE;

struct _PROCESSOR_POWER_STATE;

typedef
VOID
(VEAFAST *PPROCESSOR_IDLE_FUNCTION)(
    struct _PROCESSOR_POWER_STATE *PState);

typedef struct _PROCESSOR_POWER_STATE
{
    PPROCESSOR_IDLE_FUNCTION IdleFunction;
    ULONG Idle0KernelTimeLimit;
    ULONG Idle0LastTime;
    PVOID IdleHandlers;
    PVOID IdleState;
    ULONG IdleHandlersCount;
    ULONGLONG LastCheck;
    // ReactOS: PROCESSOR_IDLE_TIMES IdleTimes;
    LARGE_INTEGER IdleTimes;
    ULONG IdleTime1;
    ULONG PromotionCheck;
    ULONG IdleTime2;
    UCHAR CurrentThrottle;
    UCHAR ThermalThrottleLimit;
    UCHAR CurrentThrottleIndex;
    UCHAR ThermalThrottleIndex;
    ULONG LastKernelUserTime;
    ULONG PerfIdleTime;
    ULONGLONG DebugDelta;
    ULONG DebugCount;
    ULONG LastSysTime;
    ULONGLONG TotalIdleStateTime[3];
    ULONG TotalIdleTransitions[3];
    ULONGLONG PreviousC3StateTime;
    UCHAR KneeThrottleIndex;
    UCHAR ThrottleLimitIndex;
    UCHAR PerfStatesCount;
    UCHAR ProcessorMinThrottle;
    UCHAR ProcessorMaxThrottle;
    UCHAR LastBusyPercentage;
    UCHAR LastC3Percentage;
    UCHAR LastAdjustedBusyPercentage;
    ULONG PromotionCount;
    ULONG DemotionCount;
    ULONG ErrorCount;
    ULONG RetryCount;
    ULONG Flags;
    LARGE_INTEGER PerfCounterFrequency;
    ULONG PerfTickCount;
    KTIMER PerfTimer;
    KDPC PerfDpc;
    PROCESSOR_PERF_STATE *PerfStates;
    PVOID PerfSetThrottle;
    ULONG LastC3KernelUserTime;
    ULONG Spare1[1];
} PROCESSOR_POWER_STATE, *PPROCESSOR_POWER_STATE;

typedef struct {
    ULONG Granularity;
    ULONG Capacity;
} BATTERY_REPORTING_SCALE, *PBATTERY_REPORTING_SCALE;

typedef enum _SYSTEM_POWER_STATE {
  PowerSystemUnspecified,
    PowerSystemWorking,
    PowerSystemSleeping1,
    PowerSystemSleeping2,
    PowerSystemSleeping3,
    PowerSystemHibernate,
    PowerSystemShutdown,
    PowerSystemMaximum
} SYSTEM_POWER_STATE, *PSYSTEM_POWER_STATE;

typedef struct _SYSTEM_POWER_CAPABILITIES
{
    BOOLEAN PowerButtonPresent;
    BOOLEAN SleepButtonPresent;
    BOOLEAN LidPresent;
    BOOLEAN SystemS1;
    BOOLEAN SystemS2;
    BOOLEAN SystemS3;
    BOOLEAN SystemS4;
    BOOLEAN SystemS5;
    BOOLEAN HiberFilePresent;
    BOOLEAN FullWake;
    BOOLEAN VideoDimPresent;
    BOOLEAN ApmPresent;
    BOOLEAN UpsPresent;
    BOOLEAN ThermalControl;
    BOOLEAN ProcessorThrottle;
    UCHAR ProcessorMinThrottle;
    UCHAR ProcessorMaxThrottle;
    BOOLEAN FastSystemS4;
    UCHAR spare2[3];
    BOOLEAN DiskSpinDown;
    UCHAR spare3[8];
    BOOLEAN SystemBatteriesPresent;
    BOOLEAN BatteriesAreShortTerm;
    BATTERY_REPORTING_SCALE BatteryScale[3];
    SYSTEM_POWER_STATE AcOnLineWake;
    SYSTEM_POWER_STATE SoftLidWake;
    SYSTEM_POWER_STATE RtcWake;
    SYSTEM_POWER_STATE MinDeviceWakeState;
    SYSTEM_POWER_STATE DefaultLowLatencyWake;
} SYSTEM_POWER_CAPABILITES, *PSYSTEM_POWER_CAPABILITES;

typedef struct _POP_SHUTDOWN_WAIT_ENTRY
{
    struct _POP_SHUTDOWN_WAIT_ENTRY *NextEntry;
    struct _ETHREAD *Thread;
} POP_SHUTDOWN_WAIT_ENTRY, *PPOP_SHUTDOWN_WAIT_ENTRY;

typedef enum _POP_POLICY_DEVICE_TYPE
{
    PolicyDeviceSystemButton = 0,
    PolicyDeviceThermalZone = 1,
    PolicyDeviceBattery = 2,
    PolicyDeviceMemory = 3,
    PolicyInitiatePowerActionAPI = 4,
    PolicySetPowerStateAPI = 5,
    PolicyImmediateDozeS4 = 6,
    PolicySystemIdle = 7,
    PolicyDeviceMax = 8,
} POP_POLICY_DEVICE_TYPE;

/* EOF */