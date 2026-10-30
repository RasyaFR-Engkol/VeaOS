/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : irqobj.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : PKINTERRUPT Object Management
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 02-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file irqobj.c
 * --------------------------------------------------------------------- */

/* PUBLIC */

VOID
VEAPI
KsInitializeInterrupt(
    IN PKINTERRUPT Interrupt,
    IN PKSERVICE_ROUTINE ServiceRoutine,
    IN PVOID ServiceContext,
    IN KINTERRUPT_CONNECTION_TYPE ConnectionType,
    IN ULONG Vector,
    IN KIRQL Irql,
    IN KIRQL SynchronizeIrql,
    IN KINTERRUPT_MODE InterruptMode,
    IN CHAR ProcessorNumber,
    IN PKSPIN_LOCK SpinLock
)
{
    Interrupt->Type = InterruptObject;
    Interrupt->Size = sizeof(KINTERRUPT);

    if (SpinLock)
    {
        Interrupt->ActualLock = SpinLock;
    }
    else
    {
        KsInitializeSpinlock(&Interrupt->SpinLock);
        Interrupt->ActualLock = &Interrupt->SpinLock;
    }

    Interrupt->ServiceRoutine = ServiceRoutine;
    Interrupt->ServiceContext = ServiceContext;
    Interrupt->ConnectionType = ConnectionType;
    Interrupt->IrqVector = Vector;
    Interrupt->Vector = 0;
    Interrupt->Irql = Irql;
    Interrupt->SynchronizeIrql = SynchronizeIrql;
    Interrupt->InterruptMode = InterruptMode;
    Interrupt->Number = ProcessorNumber;
    Interrupt->Connected = FALSE;
}

VEASTATUS
VEAPI
KsConnectInterrupt(IN PKINTERRUPT Interrupt)
{
    VEASTATUS Status;

    /* TODO: Support SMP later */
    if (!Interrupt || Interrupt->Connected || !Interrupt->ServiceRoutine)
    {
        return STATUS_INVALID_PARAMETER;
    }

    switch (Interrupt->ConnectionType)
    {
        case InterruptLineBased:
        {
            Status = HctConnectInterruptToIsr(
                Interrupt,
                Interrupt->IrqVector);
            break;
        }

        case InterruptMessageBased:
        {
            Status = HctConnectMessageInterrupt(
                Interrupt,
                Interrupt->SynchronizeIrql,
                &Interrupt->Message);
            break;
        }

        default:
            return STATUS_INVALID_PARAMETER;
    }

    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    Interrupt->MessageSignaled =
        (Interrupt->ConnectionType == InterruptMessageBased);
    Interrupt->MessageSize = Interrupt->MessageSignaled ?
        sizeof(Interrupt->Message) : 0;
    Interrupt->Connected = TRUE;
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
KsDisconnectInterrupt(IN PKINTERRUPT Interrupt)
{
    VEASTATUS Status;

    if (!Interrupt || !Interrupt->Connected)
    {
        return STATUS_INVALID_PARAMETER;
    }

    Status = HctDisconnectInterruptFromIsr(Interrupt);
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    Interrupt->Connected = FALSE;
    Interrupt->MessageSignaled = FALSE;
    Interrupt->MessageSize = 0;
    RtlZeroMemory(&Interrupt->Message, sizeof(Interrupt->Message));
    return STATUS_SUCCESS;
}