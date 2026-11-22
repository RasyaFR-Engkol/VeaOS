/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : event.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Upper Layer Event
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 30-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file event.c
 * --------------------------------------------------------------------- */

GENERIC_MAPPING UlpEventMapping =
{
    VEA_STANDARD_RIGHTS_READ | EVENT_QUERY_STATE,
    VEA_STANDARD_RIGHTS_WRITE | EVENT_MODIFY_STATE,
    VEA_STANDARD_RIGHTS_EXECUTE | VEA_SYNCHRONIZE,
    EVENT_ALL_ACCESS
};

POBJECT_TYPE UlEventObjectType;

BOOLEAN
VEAPI
UlInitializeEventImplementation(VOID)
{
    OBJECT_TYPE_INITIALIZER Initializer;
    VEASTATUS Status;

    RtlZeroMemory(&Initializer, sizeof(Initializer));

    Initializer.ObjectSize = sizeof(KEVENT);
    Initializer.Length = sizeof(Initializer);
    Initializer.AllowAttachByDefault = FALSE;
    Initializer.DefaultNonPagedPoolCharge = sizeof(KEVENT);
    Initializer.PoolTag = 'Evnt';
    Initializer.PoolType = NonPagedPool;
    Initializer.GenericMapping = UlpEventMapping;
    Initializer.ValidAccessMask = EVENT_ALL_ACCESS;
    
    Status = ObCreateObjectType(
        "Event",
        &Initializer,
        &UlEventObjectType
    );
    if (!VEA_SUCCESS(Status)) return FALSE;

    return TRUE;
}

VEASTATUS
VEAPI
VeaCreateEvent(
    OUT PHANDLE EventHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL,
    IN EVENT_TYPE EventType,
    IN BOOLEAN InitialState
)
{
    KPROCESSOR_MODE PreviousMode;
    OBJECT_ATTRIBUTES LocalObjectAttributes;
    POBJECT_ATTRIBUTES CapturedObjectAttributes;
    PKEVENT EventObject;
    HANDLE LocalEventHandle;
    VEASTATUS Status;

    if (EventType != NotificationEvent && EventType != SynchronizationEvent)
        return STATUS_INVALID_PARAMETER;

    PreviousMode = KeGetPreviousMode();
    CapturedObjectAttributes = ObjectAttributes;
    EventObject = NULL;
    LocalEventHandle = NULL;

    if (PreviousMode == UserMode)
    {
        if (EventHandle == NULL)
            return STATUS_ACCESS_VIOLATION;

        _VEA_TRY
        {
            ProbeForWrite(EventHandle, sizeof(HANDLE), sizeof(ULONG));
            *EventHandle = NULL;

            if (ObjectAttributes != NULL)
            {
                ProbeForRead(ObjectAttributes, sizeof(OBJECT_ATTRIBUTES), sizeof(ULONG));
                RtlCopyMemory(
                    &LocalObjectAttributes,
                    ObjectAttributes,
                    sizeof(OBJECT_ATTRIBUTES)
                );
                CapturedObjectAttributes = &LocalObjectAttributes;
            }
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            return STATUS_ACCESS_VIOLATION;
        }
        _VEA_END;
    }
    else if (EventHandle == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    Status = ObCreateObject(
        PreviousMode,
        UlEventObjectType,
        CapturedObjectAttributes,
        PreviousMode,
        NULL,
        sizeof(KEVENT),
        0,
        0,
        (PVOID*)&EventObject
    );
    if (!VEA_SUCCESS(Status))
        return Status;

    KsInitializeEvent(EventObject, EventType, InitialState);

    Status = ObInsertObject(
        EventObject,
        NULL,
        DesiredAccess,
        0,
        NULL,
        &LocalEventHandle
    );
    if (!VEA_SUCCESS(Status))
        return Status;

    if (PreviousMode == UserMode)
    {
        _VEA_TRY
        {
            *EventHandle = LocalEventHandle;
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            VeaClose(LocalEventHandle);
            return STATUS_ACCESS_VIOLATION;
        }
        _VEA_END;
    }
    else
    {
        *EventHandle = LocalEventHandle;
    }

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
VeaSetEvent(
    IN HANDLE EventHandle,
    OUT PLONG PreviousState OPTIONAL
)
{
    KPROCESSOR_MODE PreviousMode;
    PKEVENT EventObject;
    LONG LocalPreviousState;
    VEASTATUS Status;

    PreviousMode = KeGetPreviousMode();
    EventObject = NULL;

    if (PreviousState != NULL && PreviousMode == UserMode)
    {
        _VEA_TRY
        {
            ProbeForWrite(PreviousState, sizeof(LONG), sizeof(ULONG));
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            return STATUS_ACCESS_VIOLATION;
        }
        _VEA_END;
    }

    Status = ObReferenceObjectByHandle(
        EventHandle,
        EVENT_MODIFY_STATE,
        UlEventObjectType,
        PreviousMode,
        (PVOID*)&EventObject
    );
    if (!VEA_SUCCESS(Status))
        return Status;

    LocalPreviousState = KsSetEvent(EventObject, FALSE);
    ObDereferenceObject(EventObject);

    if (PreviousState != NULL)
    {
        if (PreviousMode == UserMode)
        {
            _VEA_TRY
            {
                *PreviousState = LocalPreviousState;
            }
            _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
            {
                return STATUS_ACCESS_VIOLATION;
            }
            _VEA_END;
        }
        else
        {
            *PreviousState = LocalPreviousState;
        }
    }

    return STATUS_SUCCESS;
}