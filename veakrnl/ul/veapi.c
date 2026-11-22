/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : veapi.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : VEAPI Implementation
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 21-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file veapi.c
 * --------------------------------------------------------------------- */

POBJECT_TYPE UlWindowStationObjectType = NULL;
POBJECT_TYPE UlDesktopObjectType       = NULL;
extern VEAPI_CALLOUTS_FPNS PtpVeapiCallouts;

static GENERIC_MAPPING UlpWindowStationMapping =
{
    VEA_STANDARD_RIGHTS_READ    | 0x0001 | 0x0002 | 0x0010 | 0x0100, /* WINSTA_GENERIC_READ */
    VEA_STANDARD_RIGHTS_WRITE   | 0x0004 | 0x0008 | 0x0020 | 0x0040, /* WINSTA_GENERIC_WRITE */
    VEA_STANDARD_RIGHTS_EXECUTE | 0x0080 | 0x0200,                   /* WINSTA_GENERIC_EXECUTE */
    VEA_STANDARD_RIGHTS_ALL     | 0x037F                             /* WINSTA_ALL_ACCESS */
};

static GENERIC_MAPPING UlpDesktopMapping =
{
    VEA_STANDARD_RIGHTS_READ    | 0x0001 | 0x0040,                   /* DESKTOP_GENERIC_READ */
    VEA_STANDARD_RIGHTS_WRITE   | 0x0002 | 0x0004 | 0x0008 | 0x0010 | 0x0020 | 0x0080,
    VEA_STANDARD_RIGHTS_EXECUTE | 0x0100,                            /* DESKTOP_SWITCHDESKTOP */
    VEA_STANDARD_RIGHTS_ALL     | 0x01FF                             /* DESKTOP_ALL_ACCESS */
};

static VOID VEAPI UlpWinStaDelete(IN PVOID Object)
{
    if (PtpVeapiCallouts.WindowStationDeleteProcedure)
        PtpVeapiCallouts.WindowStationDeleteProcedure(Object);
}

static
LONG
VEAPI
UlpWinStaParse(
    IN PVOID ParseObject,
    IN POB_LOOKUP_CONTEXT LookupContext,
    IN PVOID Context,
    OUT PVOID *ResolvedObject
)
{
    if (PtpVeapiCallouts.WindowStationParseProcedure)
    {
        return PtpVeapiCallouts.WindowStationParseProcedure(
            ParseObject,
            LookupContext,
            Context,
            ResolvedObject
        );
    }

    return STATUS_OBJECT_TYPE_MISMATCH;
}

static BOOLEAN VEAPI UlpWinStaOkayToClose(
    IN PEPROCESS Process OPTIONAL,
    IN PVOID Object,
    IN HANDLE Handle,
    IN KPROCESSOR_MODE PreviousMode
)
{
    if (PtpVeapiCallouts.WindowStationOkToCloseProcedure)
        return PtpVeapiCallouts.WindowStationOkToCloseProcedure(Process, Object, Handle, PreviousMode);
    return TRUE;
}

static VOID VEAPI UlpDesktopDelete(IN PVOID Object)
{
    if (PtpVeapiCallouts.DesktopDeleteProcedure)
        PtpVeapiCallouts.DesktopDeleteProcedure(Object);
}

static VEASTATUS VEAPI UlpDesktopOpen(
    IN OB_OPEN_REASON OpenReason,
    IN PEPROCESS Process OPTIONAL,
    IN PVOID Object,
    IN ACCESS_MASK GrantedAccess,
    IN ULONG HandleCount
)
{
    if (PtpVeapiCallouts.DesktopOpenProcedure)
        return PtpVeapiCallouts.DesktopOpenProcedure(OpenReason, Process, Object, GrantedAccess, HandleCount);
    return STATUS_SUCCESS;
}

static VOID VEAPI UlpDesktopClose(
    IN PEPROCESS Process OPTIONAL,
    IN PVOID Object,
    IN ACCESS_MASK GrantedAccess,
    IN ULONG ProcessHandleCount,
    IN ULONG SystemHandleCount
)
{
    if (PtpVeapiCallouts.DesktopCloseProcedure)
        PtpVeapiCallouts.DesktopCloseProcedure(Process, Object, GrantedAccess, ProcessHandleCount, SystemHandleCount);
}

static BOOLEAN VEAPI UlpDesktopOkayToClose(
    IN PEPROCESS Process OPTIONAL,
    IN PVOID Object,
    IN HANDLE Handle,
    IN KPROCESSOR_MODE PreviousMode
)
{
    if (PtpVeapiCallouts.DesktopOkToCloseProcedure)
        return PtpVeapiCallouts.DesktopOkToCloseProcedure(Process, Object, Handle, PreviousMode);
    return TRUE;
}

BOOLEAN
VEAPI
UlVeapiInit(VOID)
{
    OBJECT_TYPE_INITIALIZER TypeInitializer;
    VEASTATUS Status;

    DPRINT("[UL] Creating Win32 Object Types (WindowStation & Desktop)...\n");

    /* 1. Create WindowStation Object Type */
    RtlZeroMemory(&TypeInitializer, sizeof(OBJECT_TYPE_INITIALIZER));

    TypeInitializer.Length               = sizeof(OBJECT_TYPE_INITIALIZER);
    TypeInitializer.AllowAttachByDefault = FALSE;
    TypeInitializer.PoolTag              = 'WSta';
    TypeInitializer.ObjectSize           = 0; /* Di-set oleh veapi.ko saat register callout */
    TypeInitializer.PoolType             = NonPagedPool;
    TypeInitializer.ValidAccessMask      = VEA_STANDARD_RIGHTS_REQUIRED | 0x037F;
    TypeInitializer.GenericMapping       = UlpWindowStationMapping;
    TypeInitializer.DeleteRoutine        = UlpWinStaDelete;
    TypeInitializer.ParseRoutine         = UlpWinStaParse;
    TypeInitializer.OkayToCloseRoutine   = UlpWinStaOkayToClose;

    Status = ObCreateObjectType(
        "WindowsStation",
        &TypeInitializer,
        &UlWindowStationObjectType
    );
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[UL] Failed to create WindowStation object type: 0x%08X\n", Status);
        return FALSE;
    }

    /* 2. Create Desktop Object Type */

    TypeInitializer.PoolTag            = 'Desk';
    TypeInitializer.ValidAccessMask    = VEA_STANDARD_RIGHTS_REQUIRED | 0x01FF;
    TypeInitializer.GenericMapping     = UlpDesktopMapping;
    TypeInitializer.DeleteRoutine      = UlpDesktopDelete;
    TypeInitializer.ParseRoutine       = NULL;
    TypeInitializer.OpenRoutine        = UlpDesktopOpen;
    TypeInitializer.CloseRoutine       = UlpDesktopClose;
    TypeInitializer.OkayToCloseRoutine = UlpDesktopOkayToClose;

    Status = ObCreateObjectType(
        "Desktop",
        &TypeInitializer,
        &UlDesktopObjectType
    );
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[UL] Failed to create Desktop object type: 0x%08X\n", Status);
        return FALSE;
    }

    return TRUE;
}
