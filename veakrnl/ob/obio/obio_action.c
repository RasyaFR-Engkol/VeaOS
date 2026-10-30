/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : obio_action.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Obio Action
 ----- Effective since 09-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 16-09-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file obio_action.c
 * --------------------------------------------------------------------- */

typedef struct _DEVICE_ACTION_REQUEST
{
    LIST_ENTRY RequestListEntry;
    PDEVICE_OBJECT DeviceObject;
    PKEVENT CompletionEvent;
    PVEASTATUS CompletionStatus;
    DEVICE_ACTION Action;
} DEVICE_ACTION_REQUEST, *PDEVICE_ACTION_REQUEST;

BOOLEAN ObioDeviceActionInProgress = FALSE;
KSPIN_LOCK ObioDeviceActionLock;
LIST_ENTRY ObioDeviceActionRequestList;
UL_WORK_QUEUE_ITEM ObioDeviceActionWorkerItem;

static
VOID
VEAPI
ObioDeviceActionWorker(IN PVOID Context)
{
    KIRQL OldIrql;
    PLIST_ENTRY ListEntry;
    PDEVICE_ACTION_REQUEST Request;
    PDEVICE_NODE DeviceNode;
    VEASTATUS Status;

    KsAcquireSpinLock(&ObioDeviceActionLock, &OldIrql);
    while (!IsListEmpty(&ObioDeviceActionRequestList))
    {
        /* Pop out */
        ListEntry = RemoveHeadList(&ObioDeviceActionRequestList);
        KsReleaseSpinLock(&ObioDeviceActionLock, OldIrql);
        
        /* Get Request */
        Request = CONTAINING_RECORD(ListEntry, DEVICE_ACTION_REQUEST, RequestListEntry);

        /* Try to get device node */
        DeviceNode = ObioGetDeviceNode(Request->DeviceObject);

        /* Assume Succeed */
        Status = STATUS_SUCCESS;

        switch (Request->Action)
        {
            case PiActionAddBootDevices:
            {
                if (DeviceNode->State == DeviceNodeInitialized &&
                    !(DeviceNode->Flags & DNF_PROBLEM))
                {
                    DPRINT("PiActionAddBootDevice: %p\n", DeviceNode);

                    /* FIXME: */
                    // Status = ObioCallDriverAddDevice(DeviceNode);
                }
                else
                {
                    DPRINT("PiActionAddBootDevice cant be done for %p: State: 0x%x Flags: 0x%x\n", DeviceNode, DeviceNode->State, DeviceNode->Flags);
                }
                break;
            }
            case PiActionEnumRootDevices:
            case PiActionEnumDeviceTree:
            {
                DeviceNode->Flags |= DNF_REENUMERATE;
                ObioDevNodeStateMachine(DeviceNode);
                break;
            }

            case PiActionResetDevice:
            {
                if (DeviceNode->State == DeviceNodeRemoved)
                {
                    DeviceNode->State = DeviceNodeUninitialized;
                }
                Status = STATUS_SUCCESS;
                break;
            }

            case PiActionStartDevice:
            {
                if (DeviceNode->State == DeviceNodeInitialized &&
                   !(DeviceNode->State & DNF_PROBLEM))
                {
                    ObioDevNodeStateMachine(DeviceNode);
                }
                else
                {
                    DPRINT("ERR: Attempt to start Uninitialized device: %p\n", DeviceNode);
                    Status = STATUS_UNSUCCESSFUL;
                }
            }

            case PiActionQueryState:
            {
                if (DeviceNode->State == DeviceNodeStarted)
                {
                    Status = ObioUpdateDeviceState(DeviceNode);
                    if (VEA_SUCCESS(Status))
                    {
                        ObioDevNodeStateMachine(DeviceNode);
                    }
                }
                Status = STATUS_SUCCESS;
                break;
            }

            default:
                DPRINT("Device Action Worker got %d\n", Request->Action);
                break;
        }

        if (Request->CompletionStatus)
        {
            *Request->CompletionStatus = Status;
        }

        if (Request->CompletionEvent)
        {
            KsSetEvent(Request->CompletionEvent, FALSE);
        }

        ObDereferenceObject(Request->DeviceObject);
        UlFreePoolWithTag(Request, TAG_OBIO);
        KsAcquireSpinLock(&ObioDeviceActionLock, &OldIrql);
    }
    ObioDeviceActionInProgress = FALSE;
    KsReleaseSpinLock(&ObioDeviceActionLock, OldIrql);
    DPRINT("ObioDeviceActionWorker finished\n");
}

VOID
VEAPI
ObioQueueDeviceAction(
    IN PDEVICE_OBJECT DeviceObject,
    IN DEVICE_ACTION Action,
    IN PKEVENT CompletionEvent OPTIONAL,
    OUT PVEASTATUS CompletionStatus OPTIONAL
)
{
    PDEVICE_ACTION_REQUEST ActionRequest;
    KIRQL OldIrql;

    /* No stack here since it's async */
    ActionRequest = UlAllocatePoolZero(
        NonPagedPool,
        sizeof (DEVICE_ACTION_REQUEST),
        TAG_OBIO
    );

    /* Reference so DeviceObject doesn't get killed imediately
       (since it's async work) */
    ObReferenceObject(DeviceObject);

    /* Fill out information */
    ActionRequest->DeviceObject = DeviceObject;
    ActionRequest->Action = Action;
    ActionRequest->CompletionEvent = CompletionEvent;
    ActionRequest->CompletionStatus = CompletionStatus;

    /* Lock first and modify TailList */
    KsAcquireSpinLock(&ObioDeviceActionLock, &OldIrql);
    InsertTailList(&ObioDeviceActionRequestList, &ActionRequest->RequestListEntry);

    /* Instant Work if the action is EnumRootDevice and ActionAddBotDevices */
    if (Action == PiActionEnumRootDevices || Action == PiActionAddBootDevices)
    {
        ObioDeviceActionInProgress = TRUE;
        KsReleaseSpinLock(&ObioDeviceActionLock, OldIrql);
        ObioDeviceActionWorker(NULL);
        return;
    }

    /* We can't async more than 2 Worker */
    if (ObioDeviceActionInProgress)
    {
        KsReleaseSpinLock(&ObioDeviceActionLock, OldIrql);
        return;
    }

    /* We work now and set DeviceActionInProgress to TRUE */
    ObioDeviceActionInProgress = TRUE;
    KsReleaseSpinLock(&ObioDeviceActionLock, OldIrql);

    UlInitializeWorkItem(&ObioDeviceActionWorkerItem, ObioDeviceActionWorker, NULL);
    UlQueueWorkItem(&ObioDeviceActionWorkerItem, DelayedWorkQueue);
}
