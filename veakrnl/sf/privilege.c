/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : privilege.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Security Privilege
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 17-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file privilege.c
 * --------------------------------------------------------------------- */

const LUID SfCreateTokenPrivilege          = CONST_LUID(SF_CREATE_TOKEN_PRIVILEGE, 0);
const LUID SfAssignPrimaryTokenPrivilege   = CONST_LUID(SF_ASSIGNPRIMARYTOKEN_PRIVILEGE, 0);
const LUID SfLockMemoryPrivilege           = CONST_LUID(SF_LOCK_MEMORY_PRIVILEGE, 0);
const LUID SfIncreaseQuotaPrivilege        = CONST_LUID(SF_INCREASE_QUOTA_PRIVILEGE, 0);
const LUID SfTcbPrivilege                  = CONST_LUID(SF_TCB_PRIVILEGE, 0);
const LUID SfSecurityPrivilege             = CONST_LUID(SF_SECURITY_PRIVILEGE, 0);
const LUID SfTakeOwnershipPrivilege        = CONST_LUID(SF_TAKE_OWNERSHIP_PRIVILEGE, 0);
const LUID SfLoadDriverPrivilege           = CONST_LUID(SF_LOAD_DRIVER_PRIVILEGE, 0);
const LUID SfSystemProfilePrivilege        = CONST_LUID(SF_SYSTEM_PROFILE_PRIVILEGE, 0);
const LUID SfSystemtimePrivilege           = CONST_LUID(SF_SYSTEMTIME_PRIVILEGE, 0);
const LUID SfProfileSingleProcessPrivilege = CONST_LUID(SF_PROF_SINGLE_PROCESS_PRIVILEGE, 0);
const LUID SfIncreaseBasePriorityPrivilege = CONST_LUID(SF_INC_BASE_PRIORITY_PRIVILEGE, 0);
const LUID SfCreatePagefilePrivilege       = CONST_LUID(SF_CREATE_PAGEFILE_PRIVILEGE, 0);
const LUID SfCreatePermanentPrivilege      = CONST_LUID(SF_CREATE_PERMANENT_PRIVILEGE, 0);
const LUID SfBackupPrivilege               = CONST_LUID(SF_BACKUP_PRIVILEGE, 0);
const LUID SfRestorePrivilege              = CONST_LUID(SF_RESTORE_PRIVILEGE, 0);
const LUID SfShutdownPrivilege             = CONST_LUID(SF_SHUTDOWN_PRIVILEGE, 0);
const LUID SfDebugPrivilege                = CONST_LUID(SF_DEBUG_PRIVILEGE, 0);
const LUID SfAuditPrivilege                = CONST_LUID(SF_AUDIT_PRIVILEGE, 0);
const LUID SfSystemEnvironmentPrivilege    = CONST_LUID(SF_SYSTEM_ENVIRONMENT_PRIVILEGE, 0);
const LUID SfChangeNotifyPrivilege         = CONST_LUID(SF_CHANGE_NOTIFY_PRIVILEGE, 0);
const LUID SfRemoteShutdownPrivilege       = CONST_LUID(SF_REMOTE_SHUTDOWN_PRIVILEGE, 0);
const LUID SfUndockPrivilege               = CONST_LUID(SF_UNDOCK_PRIVILEGE, 0);
const LUID SfSyncAgentPrivilege            = CONST_LUID(SF_SYNC_AGENT_PRIVILEGE, 0);
const LUID SfEnableDelegationPrivilege     = CONST_LUID(SF_ENABLE_DELEGATION_PRIVILEGE, 0);
const LUID SfManageVolumePrivilege         = CONST_LUID(SF_MANAGE_VOLUME_PRIVILEGE, 0);
const LUID SfImpersonatePrivilege          = CONST_LUID(SF_IMPERSONATE_PRIVILEGE, 0);
const LUID SfCreateGlobalPrivilege         = CONST_LUID(SF_CREATE_GLOBAL_PRIVILEGE, 0);
const LUID SfTrustedCredmanPrivilege       = CONST_LUID(SF_TRUSTED_CREDMAN_ACCESS_PRIVILEGE, 0);
const LUID SfRelabelPrivilege              = CONST_LUID(SF_RELABEL_PRIVILEGE, 0);
const LUID SfIncreaseWorkingSetPrivilege   = CONST_LUID(SF_INC_WORKING_SET_PRIVILEGE, 0);
const LUID SfTimeZonePrivilege             = CONST_LUID(SF_TIME_ZONE_PRIVILEGE, 0);
const LUID SfCreateSymbolicLinkPrivilege   = CONST_LUID(SF_CREATE_SYMBOLIC_LINK_PRIVILEGE, 0);

BOOLEAN
VEAPI
SfSinglePrivilegeCheck(
    IN LUID PrivilegeValue,
    IN KPROCESSOR_MODE PreviousMode
)
{
    PEPROCESS CurrentProcess;
    PVEA_TOKEN Token;
    ULONGLONG RequiredBit;

    /* KernelMode selalu bypass pengecekan privilege */
    if (PreviousMode == KernelMode)
    {
        return TRUE;
    }

    /* Validasi batas well-known privilege LUID */
    if (PrivilegeValue.HighPart != 0 ||
        PrivilegeValue.LowPart < SF_MIN_WELL_KNOWN_PRIVILEGE ||
        PrivilegeValue.LowPart > SF_MAXIMUM_PRIVILEGE_LIMIT)
    {
        return FALSE;
    }

    CurrentProcess = (PEPROCESS)PtGetCurrentProcess();
    Token = CurrentProcess ? CurrentProcess->Token : NULL;
    if (!Token)
    {
        return FALSE;
    }

    RequiredBit = SF_PRIVILEGE_BIT(PrivilegeValue.LowPart);

    /* Privilege harus dimiliki (Present) DAN sedang aktif (Enabled) */
    return ((Token->PrivilegesPresent & RequiredBit) &&
            (Token->PrivilegesEnabled  & RequiredBit)) ? TRUE : FALSE;
}

LUID
VEAPI
SfpAllocateLocallyUniqueId(VOID)
{
    LUID NewLuid;
    ULONGLONG Val = __InterlockedIncrement((volatile I32*)SfGlobalLuidCounter);
    NewLuid.LowPart  = (ULONG)(Val & 0xFFFFFFFF);
    NewLuid.HighPart = (LONG)(Val >> 32);
    return NewLuid;
}