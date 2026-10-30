/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : power.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Initialize Power 
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 25-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file power.c
 * --------------------------------------------------------------------- */

/* VARIABLE */
SYSTEM_POWER_CAPABILITES PopCapabilities;

/* FUNCTION */

BOOLEAN
VEAPI
PoInitSystem(IN ULONG BootPhase)
{
    PVOID NotificationEntry;

    if (BootPhase == 1)
    {
        VEASTATUS Status;

        /* Power Button Notification */
        Status = ObioRegisterPlugPlayNotification(
            EventCategoryDeviceInterfaceChange,
            PNPNOTIFY_DEVICE_INTERFACE_INCLUDE_EXISTING_INTERFACES,
            (PVOID)&VEA_GUID_DEVICE_SYS_BUTTON,
            ObioRootDeviceNode->PhysicalDeviceObject->DriverObject,
            PopAddRemoveSysCapsCallback,
            (PVOID)(ULONG_PTR)PolicyDeviceSystemButton,
            &NotificationEntry
        );

        if (!VEA_SUCCESS(Status)) return FALSE;
        
        /* Register Lid (Laptop?) */
        Status = ObioRegisterPlugPlayNotification(
            EventCategoryDeviceInterfaceChange,
            PNPNOTIFY_DEVICE_INTERFACE_INCLUDE_EXISTING_INTERFACES,
            (PVOID)&VEA_GUID_DEVICE_LID,
            ObioRootDeviceNode->PhysicalDeviceObject->DriverObject,
            PopAddRemoveSysCapsCallback,
            (PVOID)(ULONG_PTR)PolicyDeviceSystemButton,
            &NotificationEntry
        );

        if (!VEA_SUCCESS(Status)) return FALSE;

        /* Register Battery (So we treat this as Laptop???) */
        Status = ObioRegisterPlugPlayNotification(
            EventCategoryDeviceInterfaceChange,
            PNPNOTIFY_DEVICE_INTERFACE_INCLUDE_EXISTING_INTERFACES,
            (PVOID)&VEA_GUID_DEVICE_BATTERY,
            ObioRootDeviceNode->PhysicalDeviceObject->DriverObject,
            PopAddRemoveSysCapsCallback,
            (PVOID)(ULONG_PTR)PolicyDeviceSystemButton,
            &NotificationEntry
        );

        if (!VEA_SUCCESS(Status)) return FALSE;
    }

    /* Thats weird in ReactOS. we can turn off ACPI while we have bugcheck code that
       said PC_ACPI_UNAVAILABLE. Every modern PCs today requires ACPI. */
    RtlZeroMemory(&PopCapabilities, sizeof(SYSTEM_POWER_CAPABILITES));

    /* S5 always available on any PC ACPIs even tho in Emulator */
    PopCapabilities.SystemS5 = TRUE;

    PopInitShutdownList();

    return TRUE;
}

VOID
VEAPI
PopPerfIdle(PPROCESSOR_POWER_STATE PowerState)
{
    DPRINT("PerfIdle function: %p\n", PowerState);
}

VOID
VEAPI
PopPerfIdleDpc(IN PKDPC Dpc,
               IN PVOID DeferredContext,
               IN PVOID SystemArgument1,
               IN PVOID SystemArgument2)
{
    PopPerfIdle(&((PKPRCB)DeferredContext)->PowerState);
}

VOID
VEAFAST
PopIdle0(IN PPROCESSOR_POWER_STATE PowerState)
{
    /* TODO: ReactOS said FIXME? */
    __Start();
    __Halt();
}

VOID
VEAPI
PoInitializePrcb(IN PKPRCB Prcb)
{
    RtlZeroMemory(&Prcb->PowerState, sizeof(Prcb->PowerState));
    Prcb->PowerState.Idle0KernelTimeLimit = 0xFFFFFFFF;
    Prcb->PowerState.CurrentThrottle = 100;
    Prcb->PowerState.CurrentThrottleIndex = 0;
    Prcb->PowerState.IdleFunction = PopIdle0; 

    KsInitializeDpc(&Prcb->PowerState.PerfDpc, PopPerfIdleDpc, Prcb);
    KsInitializeTimer(&Prcb->PowerState.PerfTimer);
}

/* EOF */