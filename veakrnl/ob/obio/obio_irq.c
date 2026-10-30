/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : obio_irq.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : OBIO Interrupt Connection Management
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 02-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file obio_irq.c
 * --------------------------------------------------------------------- */

/* PUBLIC */

VEASTATUS
VEAPI
ObioConnectInterrupt(
    OUT PKINTERRUPT *InterruptObject,
    IN PKSERVICE_ROUTINE ServiceRoutine,
    IN PVOID ServiceContext,
    IN PKSPIN_LOCK SpinLock,
    IN ULONG Vector,
    IN KIRQL Irql,
    IN KIRQL SynchronizeIrql,
    IN KINTERRUPT_MODE InterruptMode,
    IN CHAR ProcessorNumber
)
{
    PKINTERRUPT Interrupt;
    VEASTATUS Status;

    if (!InterruptObject)
    {
        return STATUS_INVALID_PARAMETER;
    }

    *InterruptObject = NULL;

    if (!ServiceRoutine || Vector > (0xBF - 0x30))
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (Irql < 3 || Irql > 0xB ||
        SynchronizeIrql < 3 || SynchronizeIrql > 0xB ||
        (InterruptMode != LevelSensitive && InterruptMode != Latched))
    {
        DPRINT("ObioConnectInterrupt: invalid mode/IRQL (Mode=%u Irql=%u Sync=%u) for IRQ %u\n",
               InterruptMode, Irql, SynchronizeIrql, Vector);
        return STATUS_INVALID_PARAMETER;
    }

    Interrupt = (PKINTERRUPT)UlAllocatePoolZero(
        NonPagedPool,
        sizeof(KINTERRUPT),
        'IrOb');
    if (!Interrupt)
    {
        return STATUS_INSUFFICIENT_MEMORY;
    }

    KsInitializeInterrupt(
        Interrupt,
        ServiceRoutine,
        ServiceContext,
        InterruptLineBased,
        Vector,
        Irql,
        SynchronizeIrql,
        InterruptMode,
        ProcessorNumber,
        SpinLock);

    Status = KsConnectInterrupt(Interrupt);
    if (!VEA_SUCCESS(Status))
    {
        UlFreePoolWithTag(Interrupt, 'IrOb');
        return Status;
    }

    *InterruptObject = Interrupt;
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
ObioConnectMessageInterrupt(
    OUT PKINTERRUPT *InterruptObject,
    OUT PKINTERRUPT_MESSAGE InterruptMessage,
    IN PKSERVICE_ROUTINE ServiceRoutine,
    IN PVOID ServiceContext,
    IN PKSPIN_LOCK SpinLock,
    IN KIRQL Irql,
    IN KIRQL SynchronizeIrql,
    IN CHAR ProcessorNumber
)
{
    PKINTERRUPT Interrupt;
    VEASTATUS Status;

    if (!InterruptObject || !InterruptMessage)
    {
        return STATUS_INVALID_PARAMETER;
    }

    *InterruptObject = NULL;
    RtlZeroMemory(InterruptMessage, sizeof(*InterruptMessage));

    if (!ServiceRoutine || Irql < 3 || Irql > 0xB ||
        SynchronizeIrql < 3 || SynchronizeIrql > 0xB)
    {
        return STATUS_INVALID_PARAMETER;
    }

    Interrupt = (PKINTERRUPT)UlAllocatePoolZero(
        NonPagedPool,
        sizeof(KINTERRUPT),
        'IrOb');
    if (!Interrupt)
    {
        return STATUS_INSUFFICIENT_MEMORY;
    }

    KsInitializeInterrupt(
        Interrupt,
        ServiceRoutine,
        ServiceContext,
        InterruptMessageBased,
        0,
        Irql,
        SynchronizeIrql,
        Message,
        ProcessorNumber,
        SpinLock);

    Status = KsConnectInterrupt(Interrupt);
    if (!VEA_SUCCESS(Status))
    {
        UlFreePoolWithTag(Interrupt, 'IrOb');
        return Status;
    }

    DPRINT("Connected Interrupt Message: %p\n", Interrupt);

    *InterruptMessage = Interrupt->Message;
    *InterruptObject = Interrupt;
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
ObioDisconnectInterrupt(IN PKINTERRUPT Interrupt)
{
    VEASTATUS Status;

    if (!Interrupt ||
        Interrupt->Type != InterruptObject ||
        Interrupt->Size != sizeof(KINTERRUPT))
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (!Interrupt->Connected)
    {
        return STATUS_NOT_FOUND;
    }

    Status = KsDisconnectInterrupt(Interrupt);
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    UlFreePoolWithTag(Interrupt, 'IrOb');
    return STATUS_SUCCESS;
}