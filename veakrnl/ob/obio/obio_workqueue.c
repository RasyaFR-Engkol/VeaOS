/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : obio_workqueue.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Obio Workqueue
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 11-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file obio_workqueue.c
 * --------------------------------------------------------------------- */

VOID
VEAPI
ObioWorkItemCallback(IN PVOID Parameter)
{
    PIO_WORKITEM WorkItem = (PIO_WORKITEM)Parameter;
    PDEVICE_OBJECT Do = WorkItem->DeviceObject;

    WorkItem->WorkerRoutine(Do, WorkItem->Context);

    ObDereferenceObject(Do);
}

VOID
VEAPI
ObioQueueWorkItem(
    IN PIO_WORKITEM WorkItem,
    IN PIO_WORKITEM_ROUTINE WorkerRoutine,
    IN WORK_QUEUE_TYPE QueueType,
    IN PVOID Context
)
{
    ObReferenceObject(WorkItem->DeviceObject);

    WorkItem->WorkerRoutine = WorkerRoutine;
    WorkItem->Context = Context;

    UlQueueWorkItem(&WorkItem->Item, QueueType);
}

VOID
VEAPI
ObioFreeWorkItem(IN PIO_WORKITEM WorkItem)
{
    UlFreePoolWithTag(WorkItem, TAG_IOWI);
}

PIO_WORKITEM
VEAPI
ObioAllocateWorkItem(
    IN PDEVICE_OBJECT DeviceObject
)
{
    PIO_WORKITEM WorkItem;

    WorkItem = UlAllocatePoolZero(NonPagedPool, sizeof(IO_WORKITEM), TAG_IOWI);
    if (!WorkItem) return NULL;

    WorkItem->DeviceObject = DeviceObject;
    UlInitializeWorkItem(&WorkItem->Item, ObioWorkItemCallback, WorkItem);

    return WorkItem;
}