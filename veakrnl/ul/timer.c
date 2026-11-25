/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : timer.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Timer Implementation
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 25-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file timer.c
 * --------------------------------------------------------------------- */

/* VARIABLE ***************************************************************/

POBJECT_TYPE UlTimerObject = NULL;

static GENERIC_MAPPING UlpTimerMapping =
{
    VEA_STANDARD_RIGHTS_READ    | TIMER_QUERY_STATE,
    VEA_STANDARD_RIGHTS_WRITE   | TIMER_MODIFY_STATE,
    VEA_STANDARD_RIGHTS_EXECUTE | VEA_SYNCHRONIZE,
    TIMER_ALL_ACCESS
};

/* PRIVATE ****************************************************************/

static
VOID
VEAPI
UlpDeleteTimer(
    IN PVOID Object
)
{
    PETIMER Timer = (PETIMER)Object;

    /* Pastikan timer dicabut dari KsTimerListHead sebelum memori dibebaskan */
    KsCancelTimer(&Timer->KeTimer);
}

/* PUBLIC *****************************************************************/

BOOLEAN
VEAPI
UlInitializeTimerImplementation(VOID)
{
    OBJECT_TYPE_INITIALIZER Initializer;
    VEASTATUS Status;

    RtlZeroMemory(&Initializer, sizeof(Initializer));

    Initializer.AllowAttachByDefault = FALSE;
    Initializer.DefaultNonPagedPoolCharge = sizeof(ETIMER);
    Initializer.PoolType = NonPagedPool;
    Initializer.PoolTag = 'Ktmr';
    Initializer.GenericMapping = UlpTimerMapping;
    Initializer.DeleteRoutine = UlpDeleteTimer;
    Initializer.Length = sizeof(OBJECT_TYPE_INITIALIZER);

    Status = ObCreateObjectType(
        "Timer",
        &Initializer,
        &UlTimerObject
    );
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[UL] Timer implementation failed.\n");
        return FALSE;
    }

    return TRUE;
}

VEASTATUS
VEAPI
VeaCreateTimer(
    OUT PHANDLE TimerHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL,
    IN TIMER_TYPE TimerType
)
{
    PETIMER TimerObject;
    HANDLE Handle;
    VEASTATUS Status;
    KPROCESSOR_MODE PreviousMode;

    if (TimerHandle == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if ((TimerType != NotificationTimer) &&
        (TimerType != SynchronizationTimer))
    {
        return STATUS_INVALID_PARAMETER;
    }

    PreviousMode = KeGetPreviousMode();

    if (PreviousMode != KernelMode)
    {
        _VEA_TRY
        {
            *TimerHandle = *TimerHandle;
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            _VEA_YIELD(return GetExceptionCode());
        }
        _VEA_END;
    }

    Status = ObCreateObject(
        KernelMode,
        UlTimerObject,
        ObjectAttributes,
        KernelMode,
        NULL,
        sizeof(ETIMER),
        0,
        0,
        (PVOID *)&TimerObject
    );
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    RtlZeroMemory(TimerObject, sizeof(ETIMER));

    KsInitializeTimer(&TimerObject->KeTimer);
    TimerObject->KeTimer.Header.Type = (TimerType == NotificationTimer)
        ? TimerNotificationObject
        : TimerSynchronizationObject;

    KsInitializeSpinlock(&TimerObject->Lock);
    InitializeListHead(&TimerObject->ActiveTimerListEntry);
    TimerObject->ApcAssociated = FALSE;

    Status = ObInsertObject(
        TimerObject,
        NULL,
        DesiredAccess,
        0,
        NULL,
        &Handle
    );
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    if (PreviousMode != KernelMode)
    {
        _VEA_TRY
        {
            *TimerHandle = Handle;
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            return GetExceptionCode();
        }
        _VEA_END;
    }
    else
    {
        *TimerHandle = Handle;
    }
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
VeaSetTimer(
    IN HANDLE TimerHandle,
    IN PLARGE_INTEGER DueTime,
    IN PTIMER_APC_ROUTINE TimerApcRoutine OPTIONAL,
    IN PVOID TimerContext OPTIONAL,
    IN BOOLEAN WakeTimer,
    IN LONG Period OPTIONAL,
    OUT PBOOLEAN PreviousState OPTIONAL
)
{
    PETIMER TimerObject;
    LARGE_INTEGER CapturedDueTime;
    BOOLEAN State;
    KIRQL OldIrql;
    KPROCESSOR_MODE PreviousMode;
    VEASTATUS Status;

    UNREFERENCED_PARAMETER(TimerContext);

    if ((DueTime == NULL) || (Period < 0))
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (TimerApcRoutine != NULL)
    {
        return STATUS_NOT_IMPLEMENTED;
    }

    PreviousMode = KeGetPreviousMode();

    if (PreviousMode != KernelMode)
    {
        _VEA_TRY
        {
            ProbeForReadLargeInteger(DueTime);
            CapturedDueTime = *DueTime;

            if (PreviousState != NULL)
            {
                ProbeForWrite(PreviousState, sizeof(BOOLEAN), 1);
            }
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            _VEA_YIELD(return GetExceptionCode());
        }
        _VEA_END;
    }
    else
    {
        CapturedDueTime = *DueTime;
    }

    Status = ObReferenceObjectByHandle(
        TimerHandle,
        TIMER_MODIFY_STATE,
        UlTimerObject,
        PreviousMode,
        (PVOID *)&TimerObject
    );
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    KsAcquireSpinLock(&TimerObject->Lock, &OldIrql);
    State = KsSetTimer(
        &TimerObject->KeTimer,
        CapturedDueTime,
        Period,
        NULL
    );

    KsReleaseSpinLock(&TimerObject->Lock, OldIrql);

    ObDereferenceObject(TimerObject);

    if (PreviousState != NULL)
    {
        if (PreviousMode != KernelMode)
        {
            _VEA_TRY
            {
                *PreviousState = State;
            }
            _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
            {
                return GetExceptionCode();
            }
            _VEA_END;
        }
        else
        {
            *PreviousState = State;
        }
    }

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
VeaOpenTimer(
    OUT PHANDLE TimerHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes
)
{
    HANDLE Handle;
    KPROCESSOR_MODE PreviousMode;
    VEASTATUS Status;

    if ((TimerHandle == NULL) || (ObjectAttributes == NULL))
    {
        return STATUS_INVALID_PARAMETER;
    }

    PreviousMode = KeGetPreviousMode();

    if (PreviousMode != KernelMode)
    {
        _VEA_TRY
        {
            ProbeForWrite(TimerHandle, sizeof(HANDLE), sizeof(HANDLE));
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            _VEA_YIELD(return GetExceptionCode());
        }
        _VEA_END;
    }

    /* Sesuaikan jumlah parameter ObOpenObjectByName jika ob.h VeaOS lebih ringkas */
    Status = ObOpenObjectByName(
        ObjectAttributes,
        UlTimerObject,
        PreviousMode,
        DesiredAccess,
        NULL,
        &Handle
    );
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    if (PreviousMode != KernelMode)
    {
        _VEA_TRY
        {
            *TimerHandle = Handle;
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            return GetExceptionCode();
        }
        _VEA_END;
    }
    else
    {
        *TimerHandle = Handle;
    }

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
VeaQueryTimer(
    IN HANDLE TimerHandle,
    IN TIMER_INFORMATION_CLASS TimerInformationClass,
    OUT PVOID TimerInformation,
    IN ULONG TimerInformationLength,
    OUT PULONG ReturnLength OPTIONAL
)
{
    PETIMER TimerObject;
    TIMER_BASIC_INFORMATION BasicInfo;
    ULONGLONG CurrentSystemTime;
    KIRQL OldIrql;
    KPROCESSOR_MODE PreviousMode;
    VEASTATUS Status;

    /* ReactOS/NT hanya mendukung TimerBasicInformation */
    if (TimerInformationClass != TimerBasicInformation)
    {
        return STATUS_INVALID_INFO_CLASS;
    }

    if (TimerInformationLength != sizeof(TIMER_BASIC_INFORMATION))
    {
        return STATUS_INFO_LENGTH_MISMATCH;
    }

    if (TimerInformation == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    PreviousMode = KeGetPreviousMode();

    if (PreviousMode != KernelMode)
    {
        _VEA_TRY
        {
            ProbeForWrite(
                TimerInformation,
                sizeof(TIMER_BASIC_INFORMATION),
                sizeof(ULONG)
            );

            if (ReturnLength != NULL)
            {
                ProbeForWrite(ReturnLength, sizeof(ULONG), sizeof(ULONG));
            }
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            _VEA_YIELD(return GetExceptionCode());
        }
        _VEA_END;
    }

    Status = ObReferenceObjectByHandle(
        TimerHandle,
        TIMER_QUERY_STATE,
        UlTimerObject,
        PreviousMode,
        (PVOID *)&TimerObject
    );
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    /* Kunci spinlock agar pembacaan 64-bit DueTime di i686 tidak tearing */
    KsAcquireSpinLock(&TimerObject->Lock, &OldIrql);

    CurrentSystemTime = KsQuerySystemTime();
    BasicInfo.TimeRemaining.QuadPart =
        TimerObject->KeTimer.DueTime.QuadPart - (LONGLONG)CurrentSystemTime;
    BasicInfo.SignalState = (BOOLEAN)TimerObject->KeTimer.Header.SignalState;

    KsReleaseSpinLock(&TimerObject->Lock, OldIrql);

    ObDereferenceObject(TimerObject);

    if (PreviousMode != KernelMode)
    {
        _VEA_TRY
        {
            RtlCopyMemory(
                TimerInformation,
                &BasicInfo,
                sizeof(TIMER_BASIC_INFORMATION)
            );

            if (ReturnLength != NULL)
            {
                *ReturnLength = sizeof(TIMER_BASIC_INFORMATION);
            }
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            return GetExceptionCode();
        }
        _VEA_END;
    }
    else
    {
        RtlCopyMemory(
            TimerInformation,
            &BasicInfo,
            sizeof(TIMER_BASIC_INFORMATION)
        );

        if (ReturnLength != NULL)
        {
            *ReturnLength = sizeof(TIMER_BASIC_INFORMATION);
        }
    }

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
VeaCancelTimer(
    IN HANDLE TimerHandle,
    OUT PBOOLEAN CurrentState OPTIONAL
)
{
    PETIMER TimerObject;
    BOOLEAN State;
    KIRQL OldIrql;
    KPROCESSOR_MODE PreviousMode;
    VEASTATUS Status;

    PreviousMode = KeGetPreviousMode();

    if ((PreviousMode != KernelMode) && (CurrentState != NULL))
    {
        _VEA_TRY
        {
            ProbeForWrite(CurrentState, sizeof(BOOLEAN), 1);
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            _VEA_YIELD(return GetExceptionCode());
        }
        _VEA_END;
    }

    Status = ObReferenceObjectByHandle(
        TimerHandle,
        TIMER_MODIFY_STATE,
        UlTimerObject,
        PreviousMode,
        (PVOID *)&TimerObject
    );
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    KsAcquireSpinLock(&TimerObject->Lock, &OldIrql);

    State = KsCancelTimer(&TimerObject->KeTimer);

    KsReleaseSpinLock(&TimerObject->Lock, OldIrql);

    ObDereferenceObject(TimerObject);

    if (CurrentState != NULL)
    {
        if (PreviousMode != KernelMode)
        {
            _VEA_TRY
            {
                *CurrentState = State;
            }
            _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
            {
                return GetExceptionCode();
            }
            _VEA_END;
        }
        else
        {
            *CurrentState = State;
        }
    }

    return STATUS_SUCCESS;
}

/* EOF */