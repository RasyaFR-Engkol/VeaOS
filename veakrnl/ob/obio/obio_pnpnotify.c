/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : obio_pnpnotify.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : PnP Notify
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>
#include <veaguid.h>

/* Revision History ------------------------------------------------------
 * DATE       : 05-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file obio_pnpnotify.c
 * --------------------------------------------------------------------- */

/* VARIABLE */

LIST_ENTRY ObioPnpNotificationList;

/* PRIVATE */

static
VOID
ObioCallbackProcedure(
    IN PDRIVER_NOTIFICATION_CALLBACK_ROUTINE CallbackRoutine,
    IN PVOID Context,
    IN PVOID NotificationStructure
)
{
    CallbackRoutine(NotificationStructure, Context);
}

static
VOID
ObioReferencePnpNotify(IN POBIO_PNP_NOTIFY_LIST NotifyList)
{
    ObReferenceObject(NotifyList->DriverObject);
    NotifyList->RefCount++;
}

static
VOID
ObioDereferencePnpNotify(IN POBIO_PNP_NOTIFY_LIST NotifyList)
{
    ObDereferenceObject(NotifyList->DriverObject);
    NotifyList->RefCount--;

    if (NotifyList->RefCount == 0)
    {
        RemoveEntryList(&NotifyList->PnpNotifyList);
        if (NotifyList->EventCategory == EventCategoryTargetDeviceChange)
        {
            ObDereferenceObject(NotifyList->DriverObject);
        }
        UlFreePoolWithTag(NotifyList, TAG_PNP_NOTIFY);
    }
}

static
VOID
ObioProcessSingleNotification(
    IN POBIO_PNP_NOTIFY_LIST NotifyList,
    IN PVOID NotificationStructure,
    OUT PLIST_ENTRY *NextEntry
)
{
    ObioReferencePnpNotify(NotifyList);

    ObioCallbackProcedure(NotifyList->Callback, NotifyList->Context, NotificationStructure);

    *NextEntry = NotifyList->PnpNotifyList.Flink;
    ObioDereferencePnpNotify(NotifyList);
}

VOID
VEAPI
ObioNotifyDeviceInterfaceChange(
    IN PGUID Event,
    IN PGUID InterfaceClassGuid,
    IN PANSI_STRING SymbolicLinkName
)
{
    PLIST_ENTRY Entry;
    PDEVICE_INTERFACE_CHANGE_NOTIFICATION NotifyStruct;

    NotifyStruct = UlAllocatePoolWithTag(PagedPool, sizeof(*NotifyStruct), TAG_PNP_NOTIFY);
    if (!NotifyStruct) return;

    NotifyStruct->Version = 10;
    NotifyStruct->Event = *Event;
    NotifyStruct->Size = sizeof(DEVICE_INTERFACE_CHANGE_NOTIFICATION);
    NotifyStruct->InterfaceClassGuid = *InterfaceClassGuid;
    NotifyStruct->SymbolicLinkName = SymbolicLinkName;

    Entry = ObioPnpNotificationList.Flink;
    while (Entry != &ObioPnpNotificationList)
    {
        POBIO_PNP_NOTIFY_LIST NList = CONTAINING_RECORD(Entry, OBIO_PNP_NOTIFY_LIST, PnpNotifyList);

        if (!IsEqualGUID(&NotifyStruct->InterfaceClassGuid, &NList->Guid))
        {
            Entry = Entry->Flink;
            continue;
        }

        ObioProcessSingleNotification(NList, NotifyStruct, &Entry);
    }

    UlFreePoolWithTag(NotifyStruct, TAG_PNP_NOTIFY);
}

/* PUBLIC FUNCTION */

VEASTATUS
VEAPI
ObioRegisterPlugPlayNotification(
    IN OBIO_NOTIFICATION_EVENT_CATEGORY EventCategory,
    IN ULONG EventCategoryFlags,
    IN PVOID EventCategoryData OPTIONAL,
    IN PDRIVER_OBJECT DriverObject,
    IN PDRIVER_NOTIFICATION_CALLBACK_ROUTINE CallbackRoutine,
    IN OUT PVOID Context OPTIONAL,
    OUT PVOID *NotificationEntry
)
{
    PCHAR SymbolicLinkList = NULL;
    POBIO_PNP_NOTIFY_LIST Entry;
    VEASTATUS Status;

    if (!DriverObject || !CallbackRoutine || !NotificationEntry ||
        (EventCategory == EventCategoryDeviceInterfaceChange &&
         !EventCategoryData))
    {
        return STATUS_INVALID_PARAMETER;
    }

    *NotificationEntry = NULL;
    ObReferenceObject(DriverObject);
    
    Entry = UlAllocatePoolZero(PagedPool, sizeof(*Entry), TAG_PNP_NOTIFY);
    if (!Entry)
    {
        DPRINT("ERR: UlAllocatePool failed\n");
        ObDereferenceObject(DriverObject);
        return STATUS_INSUFFICIENT_MEMORY;
    }

    Entry->Callback = CallbackRoutine;
    Entry->Context = Context;
    Entry->DriverObject = DriverObject;
    Entry->EventCategory = EventCategory;
    Entry->RefCount = 1;

    /* Depends on what event and what flags.. */

    switch (EventCategory)
    {
        /* FCK NO. i nly handle this */
        case EventCategoryDeviceInterfaceChange:
        {
            Entry->Guid = *(PGUID)EventCategoryData;

            InsertTailList(&ObioPnpNotificationList, &Entry->PnpNotifyList);

            if (EventCategoryFlags & PNPNOTIFY_DEVICE_INTERFACE_INCLUDE_EXISTING_INTERFACES)
            {
                DEVICE_INTERFACE_CHANGE_NOTIFICATION NotificationInfos;
                ANSI_STRING SymbolicLinkA;
                PCHAR SymbolicLink;

                Status = ObioGetDeviceInterfaces(
                    &Entry->Guid,
                    NULL, 
                    0,
                    &SymbolicLinkList
                );

                if (VEA_SUCCESS(Status))
                {
                    NotificationInfos.Version = 10;
                    NotificationInfos.Size = sizeof(DEVICE_INTERFACE_CHANGE_NOTIFICATION);
                    NotificationInfos.Event = GUID_DEVICE_INTERFACE_ARRIVAL;
                    NotificationInfos.InterfaceClassGuid = *(PGUID)EventCategoryData;
                    NotificationInfos.SymbolicLinkName = &SymbolicLinkA;

                    for (SymbolicLink = SymbolicLinkList; 
                         *SymbolicLink;
                         SymbolicLink += SymbolicLinkA.Length + 1)
                    {
                        RtlInitAnsiString(&SymbolicLinkA, SymbolicLink);
                        DPRINT("CALLBACK FOR %s\n", SymbolicLink);
                        ObioCallbackProcedure(CallbackRoutine, Context, &NotificationInfos);
                    }

                    UlFreePoolWithTag(SymbolicLinkList, TAG_RTL_STRING);
                }
                else
                {
                    ObioDereferencePnpNotify(Entry);
                    return Status;
                }
            }
            break;
        }

        default:
            DPRINT("The rest of the other were Unimplemented: %d\n", EventCategory);
            ObioDereferencePnpNotify(Entry);
            return STATUS_NOT_SUPPORTED;
    }

    DPRINT("%s returns NotificationEntry %p\n", __FUNCTION__, Entry);

    *NotificationEntry = Entry;

    return STATUS_SUCCESS;
}

/* EOF*/