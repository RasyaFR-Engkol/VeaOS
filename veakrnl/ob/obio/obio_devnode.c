/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : obio_devnode.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : DevNode
 ----- Effective since 08-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 02-08-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file obio_devnode.c
 * --------------------------------------------------------------------- */

KSPIN_LOCK ObioDeviceTreeLock;

#if 0

VOID
VEAPI
ObioPrintResourceList(
    IN POBIO_RESOURCE_LIST ResourceList
)
{
    ULONG i;

    if (!ResourceList)
    {
        DPRINT("[OBIO] ResourceList is NULL!\n");
        return;
    }

    DPRINT("=== OBIO ALLOCATED RESOURCE LIST ===\n");
    DPRINT("Total Descriptors: %u\n", ResourceList->Count);

    for (i = 0; i < ResourceList->Count; i++)
    {
        POBIO_RESOURCE_DESCRIPTOR Desc = &ResourceList->Descriptors[i];

        DPRINT("  [%u] Type: %u, Flags: 0x%02X\n", i, Desc->Type, Desc->Flags);

        switch (Desc->Type)
        {
            case OBIO_RES_TYPE_PORT:
                DPRINT("      [Port] Base: 0x%08X, Length: 0x%X\n", 
                       Desc->Port.Base, Desc->Port.Length);
                break;

            case OBIO_RES_TYPE_INTERRUPT:
                DPRINT("      [Interrupt] Vector: 0x%X, Level: 0x%X\n", 
                       Desc->Interrupt.Vector, Desc->Interrupt.Level);
                break;

            case OBIO_RES_TYPE_MEMORY:
                // Pake cast unsigned long long biar aman kalau PHYSICAL_ADDRESS itu 64-bit
                DPRINT("      [Memory] Base: 0x%08X, Length: 0x%08X\n", 
                       (unsigned long long)Desc->Memory.Base.QuadPart, Desc->Memory.Length);
                break;

            default:
                DPRINT("      [Unknown Type]\n");
                break;
        }
    }
    DPRINT("====================================\n");
}

VOID
VEAPI
ObioPrintRequiredResourcesListEx(
    IN POBIO_REQUIRED_RESOURCES_LIST_EX ReqListEx
)
{
    ULONG i, j;
    POBIO_REQUIRED_RESOURCES_LIST CurrentList;
    PUCHAR NextListPtr;

    if (!ReqListEx)
    {
        DPRINT("[OBIO] RequiredResourcesListEx is NULL!\n");
        return;
    }

    DPRINT("=== OBIO REQUIRED RESOURCES LIST EX ===\n");
    DPRINT("Total Size: %u Bytes | Bus: %u | Slot: %u | Alt Lists: %u\n", 
           ReqListEx->ListSize, ReqListEx->BusNumber, 
           ReqListEx->SlotNumber, ReqListEx->AlternativeLists);

    // Mulai dari list alternatif pertama
    CurrentList = &ReqListEx->List[0];

    for (i = 0; i < ReqListEx->AlternativeLists; i++)
    {
        DPRINT("  -> [Alternative List %u] Version: %u.%u, Count: %u\n", 
               i, CurrentList->Version, CurrentList->Revision, CurrentList->Count);

        for (j = 0; j < CurrentList->Count; j++)
        {
            POBIO_REQUIRED_RESOURCES Desc = &CurrentList->Descriptors[j];

            DPRINT("       [%u] Type: %u, Option: %u, Share: %u, Flags: 0x%04X\n", 
                   j, Desc->Type, Desc->Option, Desc->ShareDisposition, Desc->Flags);

            switch (Desc->Type)
            {
                case OBIO_RES_TYPE_PORT:
                case OBIO_RES_TYPE_MEMORY:
                    DPRINT("           [%s] Len: 0x%X, Align: 0x%X, Min: 0x%X, Max: 0x%X\n", 
                        (Desc->Type == OBIO_RES_TYPE_PORT) ? "Port" : "Memory",
                        Desc->Port.Length, Desc->Port.Alignment,
                        (ULONG)Desc->Port.MinimumAddress.QuadPart, 
                        (ULONG)Desc->Port.MaximumAddress.QuadPart);
                    break;

                case OBIO_RES_TYPE_INTERRUPT:
                    DPRINT("           [Interrupt] MinVector: 0x%X, MaxVector: 0x%X\n", 
                           Desc->Interrupt.MinimumVector, Desc->Interrupt.MaximumVector);
                    break;

                default:
                    DPRINT("           [Unknown Type]\n");
                    break;
            }
        }

        // TRICKY PART: Kalkulasi pointer ke Alternative List berikutnya!
        // Karena array Descriptors dinamis, kita gabisa pake CurrentList++.
        // Rumus: Base Address List + Header Size + (Jumlah Descriptor * Ukuran Descriptor)
        
        // Asumsi OS lu ada macro FIELD_OFFSET (bawaan C/NT). Kalau ga ada: #define FIELD_OFFSET(type, field) ((LONG)(LONG_PTR)&(((type *)0)->field))
        NextListPtr = (PUCHAR)CurrentList + 
                      FIELD_OFFSET(OBIO_REQUIRED_RESOURCES_LIST, Descriptors) + 
                      (CurrentList->Count * sizeof(OBIO_REQUIRED_RESOURCES));
        
        // Pindah ke list berikutnya
        CurrentList = (POBIO_REQUIRED_RESOURCES_LIST)NextListPtr;
    }
    DPRINT("=======================================\n");
}

#endif

PDEVICE_NODE
VEAPI
ObioCreateDeviceNode(IN PDEVICE_OBJECT Pdo)
{
    VEASTATUS Status;
    OIP_INFORMATION Information;

    if(!Pdo) return NULL;

    PDEVICE_NODE NewNode = (PDEVICE_NODE)UlAllocatePoolWithTag(
        NonPagedPool,
        sizeof(DEVICE_NODE),
        VEA_DEVNODE_TAG
    );

    if (!NewNode)
    {
        DPRINT("[Ob] ERROR | Gagal alokasi DEVICE_NODE buat Pdo 0x%p\n", Pdo);
        return NULL;
    }

    RtlZeroMemory(NewNode, sizeof(DEVICE_NODE));

    NewNode->PhysicalDeviceObject = Pdo;
    NewNode->Parent               = NULL;
    NewNode->Sibling              = NULL;
    NewNode->Child                = NULL;
    NewNode->State                = DeviceNodeUninitialized;
    NewNode->Flags                = 0;
    
    Status = ObioCreateDeviceNodeInstancePath(NewNode, &NewNode->InstancePath);
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[Ob] ERROR | Gagal buat InstancePath untuk Pdo %p (Status 0x%x)\n", Pdo, Status);
        UlFreePoolWithTag(NewNode, VEA_DEVNODE_TAG);
        return NULL;
    }

    RtlZeroMemory(&Information, sizeof(OIP_INFORMATION));

    Status = ObioCallSpecificPnpOip(NewNode->PhysicalDeviceObject,
        OIP_MN_QUERY_RESOURCES,
        NULL,
        &Information
    );

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("Resource unavailable for Device %p and DevNode %p\n", Pdo, NewNode);
        NewNode->AdditionalResources = NULL;
    }
    else
    {
        NewNode->AdditionalResources = (POBIO_RESOURCE_LIST)Information.Information;
        NewNode->Flags |= DNF_HAS_BOOT_CONFIG;
        // ObioPrintResourceList(NewNode->AdditionalResources);
    }

    Status = ObioCallSpecificPnpOip(NewNode->PhysicalDeviceObject, OIP_MN_QUERY_RESOURCE_REQUIREMENTS, NULL, &Information);
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("Required resources unavailable for Device %p and DevNode %p\n", Pdo, NewNode);
        NewNode->RequiredResources = NULL;
    }
    else
    {
        NewNode->RequiredResources = (POBIO_REQUIRED_RESOURCES_LIST_EX)Information.Information;
        // ObioPrintRequiredResourcesListEx(NewNode->RequiredResources);
    }

    Pdo->DeviceNode = NewNode;

    return NewNode;
}

VEASTATUS
VEAPI
ObioAddDeviceNodeChild(
    IN PDEVICE_NODE Parent,
    IN PDEVICE_NODE Child
)
{
    if (!Parent || !Child || Parent == Child) {
        return STATUS_INVALID_PARAMETER;
    }

    Child->Parent = Parent;
    Child->Sibling = NULL;

    if (Parent->LastChild) {
        Parent->LastChild->Sibling = Child;
    } else {
        Parent->Child = Child;
    }

    Parent->LastChild = Child;
    return STATUS_SUCCESS;
}

static
VEASTATUS
ObioTraversePdoTreeNode(
    IN PDEVICE_NODE Node,
    IN POBIO_PDO_TRAVERSE_ROUTINE Routine,
    IN PVOID Context
)
{
    VEASTATUS Status;
    PDEVICE_NODE Child;

    if (!Node) {
        return STATUS_SUCCESS;
    }

    Status = Routine(Node->PhysicalDeviceObject, Node, Context);
    if (!VEA_SUCCESS(Status)) {
        return Status;
    }

    Child = Node->Child;
    while (Child) {
        Status = ObioTraversePdoTreeNode(Child, Routine, Context);
        if (!VEA_SUCCESS(Status)) {
            return Status;
        }

        Child = Child->Sibling;
    }

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
ObioTraversePdoTree(
    IN PDEVICE_NODE RootNode,
    IN POBIO_PDO_TRAVERSE_ROUTINE Routine,
    IN PVOID Context
)
{
    if (!RootNode || !Routine) {
        return STATUS_INVALID_PARAMETER;
    }

    return ObioTraversePdoTreeNode(RootNode, Routine, Context);
}

typedef struct _OBIO_FIND_DEVICE_INSTANCE_CONTEXT {
    PANSI_STRING InstancePath;
    PDEVICE_OBJECT DeviceObject;
} OBIO_FIND_DEVICE_INSTANCE_CONTEXT, *POBIO_FIND_DEVICE_INSTANCE_CONTEXT;

static
VEASTATUS
VEAPI
ObioFindDeviceInstanceTraverse(
    IN PDEVICE_OBJECT PhysicalDeviceObject,
    IN PDEVICE_NODE DeviceNode,
    IN PVOID Context
)
{
    POBIO_FIND_DEVICE_INSTANCE_CONTEXT FindContext;

    FindContext = (POBIO_FIND_DEVICE_INSTANCE_CONTEXT)Context;
    if (!FindContext || !DeviceNode || !PhysicalDeviceObject) {
        return STATUS_INVALID_PARAMETER;
    }

    if (RtlEqualString(&DeviceNode->InstancePath,
                       FindContext->InstancePath,
                       TRUE))
    {
        ObReferenceObject(PhysicalDeviceObject);
        FindContext->DeviceObject = PhysicalDeviceObject;
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

PDEVICE_OBJECT
VEAPI
ObioGetDeviceObjectFromDeviceInstance(
    IN PANSI_STRING DeviceInstance
)
{
    OBIO_FIND_DEVICE_INSTANCE_CONTEXT FindContext;
    PDEVICE_OBJECT RootDeviceObject;

    if (!ObioRootDeviceNode || !ObioRootDeviceNode->PhysicalDeviceObject) {
        return NULL;
    }

    RootDeviceObject = ObioRootDeviceNode->PhysicalDeviceObject;
    if (!DeviceInstance || DeviceInstance->Length == 0) {
        ObReferenceObject(RootDeviceObject);
        return RootDeviceObject;
    }

    FindContext.InstancePath = DeviceInstance;
    FindContext.DeviceObject = NULL;

    if (!VEA_SUCCESS(ObioTraversePdoTree(ObioRootDeviceNode,
                                         ObioFindDeviceInstanceTraverse,
                                         &FindContext)) &&
        FindContext.DeviceObject == NULL)
    {
        return NULL;
    }

    return FindContext.DeviceObject;
}

VEASTATUS
VEAPI
ObioRegisterRootDeviceNode(
    IN PDEVICE_OBJECT Pdo,
    IN PCSTR HardwareId        // <--- Masukkan HardwareID sebagai parameter!
)
{
    PDEVICE_NODE DevNode;
    PCHAR FinalPathBuffer = NULL;
    USHORT FinalPathLen = 0;

    if (!Pdo) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!ObioRootDeviceNode) {
        DPRINT("ERROR: ObioRootDeviceNode belum diinisialisasi!\n");
        return STATUS_UNSUCCESSFUL;
    }

    DevNode = ObioCreateDeviceNode(Pdo);
    if (!DevNode) {
        return STATUS_INSUFFICIENT_MEMORY;
    }

    /* 1. Sambungkan ke Root Tree */
    if (!VEA_SUCCESS(ObioAddDeviceNodeChild(ObioRootDeviceNode, DevNode))) {
        return STATUS_UNSUCCESSFUL;
    }

    /* 2. Format Path "ROOT\<HardwareId>" secara dinamis */
    PCSTR TargetHwId = (HardwareId != NULL) ? HardwareId : "ACPI_HAL/0000";
    USHORT HwIdLen   = RtlRawStringLength(TargetHwId);
    USHORT PrefixLen = 5; // "ROOT\"

    FinalPathLen = PrefixLen + HwIdLen;
    FinalPathBuffer = (PCHAR)UlAllocatePoolZero(
        NonPagedPool,
        FinalPathLen + 1,
        VEA_DEVNODE_TAG
    );

    if (!FinalPathBuffer) {
        DevNode->State |= DNF_PROBLEM;
        ObioPnpChangeState(DevNode, DeviceNodeUnspecified);
        return STATUS_INSUFFICIENT_MEMORY;
    }

    RtlCopyRawString(FinalPathBuffer, "ROOT/");
    RtlCopyRawString(FinalPathBuffer + PrefixLen, TargetHwId);

    /* 3. Assign ke InstancePath */
    DevNode->InstancePath.Buffer        = FinalPathBuffer;
    DevNode->InstancePath.Length        = FinalPathLen;
    DevNode->InstancePath.MaximumLength = FinalPathLen + 1;

    DevNode->Flags |= (OBIO_DEVNODE_SYNTHETIC |
                       OBIO_DEVNODE_ENUMERATED |
                       OBIO_DEVNODE_BOOT_CRITICAL |
                       DNF_REENUMERATE);
    
    ObioPnpChangeState(DevNode, DeviceNodeInitialized);

    DPRINT("Obio: Root Device Node [%s] successfully registered under HTREE/ROOT/0\n", DevNode->InstancePath.Buffer);

    return STATUS_SUCCESS;
}

PDEVICE_NODE
VEAPI
ObioGetDeviceNode(PDEVICE_OBJECT DeviceObject)
{
    return DeviceObject->DeviceNode;
}

VOID
VEAPI
ObioDevNodeStateMachine(IN PDEVICE_NODE DeviceNode)
{
    BOOLEAN DoProcessAgain;
    PDEVICE_NODE CurrentNode;
    VEASTATUS Status;

    if (!DeviceNode || !DeviceNode->PhysicalDeviceObject) {
        return;
    }

    CurrentNode = DeviceNode;

    do
    {
        /* Set it off */
        DoProcessAgain = FALSE;

        /* Reference the object so it doesn't get killed */
        ObReferenceObject(CurrentNode->PhysicalDeviceObject);

        /* Skip problem device */
        if (CurrentNode->Flags & DNF_PROBLEM &&
            CurrentNode->State != DeviceNodeAwaitingQueuedRemoval)
        {
            /* Todo: Do something */
        }

        switch (CurrentNode->State)
        {
            case DeviceNodeUnspecified:
                DPRINT("%Z state: DeviceNodeUnspecified\n", &CurrentNode->InstancePath);
                break;

            case DeviceNodeUninitialized:
                DPRINT("%Z state: DeviceNodeUninitialized\n", &CurrentNode->InstancePath);
                break;

            case DeviceNodeInitialized:
                DPRINT("%Z State: DeviceNodeInitialized\n", &CurrentNode->InstancePath);
                Status = ObioCallDriverAddDevice(CurrentNode);
                if (VEA_SUCCESS(Status))
                {
                    ObioSetDevNodeState(CurrentNode, DeviceNodeDriversAdded);
                    DoProcessAgain = TRUE;
                }
                else if (Status != STATUS_NOT_FOUND)
                {
                    DPRINT("AddDevice failed for %Z: 0x%x\n",
                           &CurrentNode->InstancePath,
                           Status);
                    ObioSetDevNodeProblem(CurrentNode, 1);
                    ObioSetDevNodeState(CurrentNode, DeviceNodeAwaitingQueuedRemoval);
                }
                break;

            case DeviceNodeDriversAdded:
                DPRINT("%Z State: DeviceNodeDriversAdded\n", &CurrentNode->InstancePath);
                Status = ObioAssignDeviceResources(CurrentNode);
                DoProcessAgain = VEA_SUCCESS(Status);
                break;

            case DeviceNodeResourcesAssigned:
                DPRINT("%Z State: DeviceNodeResourcesAssigned\n", &CurrentNode->InstancePath);
                ObioStartDevice(CurrentNode);

                ObioSetDevNodeState(CurrentNode, DeviceNodeStartCompletion);
                DoProcessAgain = TRUE;
                break;

            case DeviceNodeStartPending:
                DPRINT("%Z State: DeviceNodeStartPending\n", &CurrentNode->InstancePath);
                break;

            case DeviceNodeStartCompletion:
                DPRINT("%Z State: DeviceNodeStartCompletion\n", &CurrentNode->InstancePath);
                Status = CurrentNode->LastStateCompletionStatus;
                DoProcessAgain = TRUE;

                if (!VEA_SUCCESS(Status))
                {
                    ObioSetDevNodeProblem(CurrentNode, 1);
                    ObioSetDevNodeState(CurrentNode, DeviceNodeAwaitingQueuedRemoval);   
                }
                else
                {
                    ObioSetDevNodeState(CurrentNode, DeviceNodeStartPostWork);
                }

                break;

            case DeviceNodeStartPostWork:
                DPRINT("%Z State: DeviceNodeStartPostWork\n", &CurrentNode->InstancePath);
                Status = ObioStartDeviceFinal(CurrentNode);
                DoProcessAgain = TRUE;
                break;

            case DeviceNodeStarted:
                DPRINT("%Z State: DeviceNodeStarted\n", &CurrentNode->InstancePath);

                /* Check for reenumerate */
                if (CurrentNode->Flags & DNF_REENUMERATE)
                {
                    DPRINT("REENUMERATE\n");
                    CurrentNode->Flags &= ~DNF_REENUMERATE;
                    Status = ObioQueryDeviceRelations(CurrentNode, BusRelations);

                    /* Set DevNode state */
                    ObioSetDevNodeState(CurrentNode, DeviceNodeEnumerateCompletion);
                    DoProcessAgain = TRUE;
                }
                break;

            case DeviceNodeQueryStopped:
                DPRINT("%Z State: DeviceNodeQueryStopped\n", &CurrentNode->InstancePath);
                break;

            case DeviceNodeStopped:
                DPRINT("%Z State: DeviceNodeStopped\n", &CurrentNode->InstancePath);
                break;

            case DeviceNodeEnumerateCompletion:
                DPRINT("%Z State: DeviceNodeEnumerationCompletion\n", &CurrentNode->InstancePath);
                Status = ObioEnumerateDeviceNode(CurrentNode);
                DoProcessAgain = FALSE;
                break;

            case DeviceNodeRestartCompletion:
                 DPRINT("%Z State: DeviceNodeRestartCompletion\n", &CurrentNode->InstancePath);
                break;

            case DeviceNodeAwaitingQueuedDeletion:
                break;
            case DeviceNodeAwaitingQueuedRemoval:
                DPRINT("DeviceNodeAwaitingQueuedRemoval %wZ\n", &CurrentNode->InstancePath);
                break;
            case DeviceNodeQueryRemoved:
                break;
            case DeviceNodeRemovePendingCloses:
                break;
            case DeviceNodeRemoved:
                break;
            case DeviceNodeDeletePendingCloses:
                break;
            case DeviceNodeDeleted:
                break;
            default:
                break;
        }
SkipEnum:
        if (!DoProcessAgain)
        {
            KIRQL OldIrql;
            KsAcquireSpinLock(&ObioDeviceTreeLock, &OldIrql);

            if (CurrentNode->State != DeviceNodeRemoved && CurrentNode->Child != NULL)
            {
                CurrentNode = CurrentNode->Child;
            }
            else
            {
                while (CurrentNode != DeviceNode)
                {
                    if (CurrentNode->Sibling != NULL)
                    {
                        CurrentNode = CurrentNode->Sibling;
                        break;
                    }
                    else
                    {
                        CurrentNode = CurrentNode->Parent;
                    }
                }
            }
            KsReleaseSpinLock(&ObioDeviceTreeLock, OldIrql);
        }
        ObDereferenceObject(CurrentNode->PhysicalDeviceObject);
    } while (DoProcessAgain || CurrentNode != DeviceNode);
}

VEASTATUS
VEAPI
ObioUpdateDeviceState(PDEVICE_NODE DeviceNode)
{
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
ObioCreateDeviceNodeInstancePath
(
    IN PDEVICE_NODE DeviceNode,
    OUT PANSI_STRING InstancePath
)
{
    OIP_STACK Stack;
    VEASTATUS Status;
    OIP_INFORMATION StatusInformation;
    ANSI_STRING DeviceId, InstanceID;
    PDEVICE_OBJECT UpperDeviceObject;

    UpperDeviceObject = ObioGetTopDeviceObject(DeviceNode->PhysicalDeviceObject);

    Stack.Parameters.QueryId.IdType = BusQueryDeviceID;
    Status = ObioCallSpecificPnpOip(
        UpperDeviceObject,
        OIP_MN_QUERY_ID,
        &Stack,
        &StatusInformation
    );
    if(!VEA_SUCCESS(Status))
    {
        DPRINT("IopInitiatePnpIrp(BusQueryDeviceID) failed (Status %x)\n", Status);
        return Status;
    }

    /* Save the DeviceId */
    RtlInitAnsiString(&DeviceId, (PCHAR)StatusInformation.Information);

    /* IN ReactOS, there is DeviceCapabilities. But keep it simple for VeaOS */

    Stack.Parameters.QueryId.IdType = BusQueryInstanceID;
    Status = ObioCallSpecificPnpOip(
        UpperDeviceObject,
        OIP_MN_QUERY_ID,
        &Stack,
        &StatusInformation
    );
    if(!VEA_SUCCESS(Status))
    {
        DPRINT("IopInitiatePnpIrp(BusQueryDeviceID) failed (Status %x)\n", Status);
        return Status;
    }

    RtlInitAnsiString(&InstanceID, (PCHAR)StatusInformation.Information);

    if (InstancePath)
    {
        InstancePath->Length = 0;
        InstancePath->MaximumLength = 
            DeviceId.Length + sizeof(CHAR) + InstanceID.Length + sizeof(CHAR);
        InstancePath->Buffer = UlAllocatePoolZero(
            NonPagedPool,
            InstancePath->MaximumLength,
            TAG_OBIO
        );
        if (!InstancePath->Buffer)
        {
            DPRINT("ERR: InstancePath->Buffer memory allocate failed\n");
            return STATUS_INSUFFICIENT_MEMORY;
        }

        RtlCopyString(InstancePath, &DeviceId);
        RtlAppendRawStringToString(InstancePath, "/");
        RtlAppendStringToString(InstancePath, &InstanceID);
    }

    return STATUS_SUCCESS;
}

DEVNODE_STATE
VEAPI
ObioSetDevNodeState(
    IN PDEVICE_NODE DeviceNode,
    IN DEVNODE_STATE DevnodeState
)
{
    KIRQL OldIrql;

    /* Do not preempt */
    KsAcquireSpinLock(&ObioDeviceTreeLock, &OldIrql);

    DEVNODE_STATE PreviousState = DeviceNode->State;
    if (PreviousState != DevnodeState)
    {
        DeviceNode->State = DevnodeState;
        DeviceNode->PreviousState = PreviousState;
    }

    KsReleaseSpinLock(&ObioDeviceTreeLock, OldIrql);

    DPRINT("%Z changed state from 0x%x -> 0x%x\n", &DeviceNode->InstancePath, PreviousState, DeviceNode->State);

    return PreviousState;
}

VEASTATUS
VEAPI
ObioEnumerateDeviceNode(
    IN PDEVICE_NODE DeviceNode
)
{
    PDEVICE_RELATIONS DeviceRelations;
    PDEVICE_NODE Child;
    PDEVICE_OBJECT ChildDeviceObject;
    PDEVICE_NODE ChildDeviceNode;

    if (!VEA_SUCCESS(DeviceNode->LastStateCompletionStatus))
    {
        DeviceRelations = DeviceNode->PendingDeviceRelations;
        DeviceNode->PendingDeviceRelations = NULL;
        if (DeviceRelations)
        {
            for (ULONG i = 0; i < DeviceRelations->Count; i++)
            {
                ObDereferenceObject(DeviceRelations->Objects[i]);
            }
            UlFreePoolWithTag(DeviceRelations, TAG_OBIO);
        }

        DPRINT("ERR: Query Device Relations failed: 0x%x\n", DeviceNode->LastStateCompletionStatus);
        ObioSetDevNodeState(DeviceNode, DeviceNodeStarted);
        return DeviceNode->LastStateCompletionStatus;
    }

    DeviceRelations = DeviceNode->PendingDeviceRelations;
    DeviceNode->PendingDeviceRelations = NULL;

    /* It's ok to not have child */
    if (!DeviceRelations)
    {
        DPRINT("DeviceNode %Z doesn't have child\n", &DeviceNode->InstancePath);
        ObioSetDevNodeState(DeviceNode, DeviceNodeStarted);
        return STATUS_SUCCESS;
    }

    for (Child = DeviceNode->Child; Child != NULL; Child = Child->Sibling)
    {
        Child->Flags &= ~DNF_ENUMERATED;
    }

    DPRINT("DeviceNode %Z has %u child\n", &DeviceNode->InstancePath, DeviceRelations->Count);

    for (ULONG i = 0; i < DeviceRelations->Count; i++)
    {
        ChildDeviceObject = DeviceRelations->Objects[i];

        ChildDeviceNode = ObioGetDeviceNode(ChildDeviceObject);
        if (!ChildDeviceNode)
        {
            /* Make new DeviceNode */
            ChildDeviceNode = ObioAllocateZeroDeviceNode(ChildDeviceObject);
            if (ChildDeviceNode)
            {
                ObioInsertDeviceNode(ChildDeviceNode, DeviceNode);

                ObioSetDevNodeState(ChildDeviceNode, DeviceNodeInitialized);
                ChildDeviceNode->Flags |= DNF_REENUMERATE | DNF_ENUMERATED;
                ChildDeviceObject->Flags |= DO_BUS_ENUMERATED_DEVICE;
            }
            else
            {
                DPRINT("ObioAllocateZeroDeviceNode failed. Skipping PDO %u\n", i);
                ObDereferenceObject(ChildDeviceObject);
            }
        }
        else
        {
            ChildDeviceNode->Flags |= DNF_ENUMERATED;
            ObDereferenceObject(ChildDeviceObject);
        }
    }
    UlFreePoolWithTag(DeviceRelations, TAG_OBIO);

    for (PDEVICE_NODE Child = DeviceNode->Child;
         Child != NULL;
         Child = Child->Sibling)
    {
        if (!(Child->Flags & (DNF_ENUMERATED | DNF_DEVICE_GONE)))
        {
            Child->Flags |= DNF_DEVICE_GONE;
            DPRINT("DEVICE GONE!!!: %p\n", Child);
            ObioSetDevNodeState(Child, DeviceNodeAwaitingQueuedRemoval);
        }
    }

    ObioSetDevNodeState(DeviceNode, DeviceNodeStarted);
    return STATUS_SUCCESS;
}

PDEVICE_NODE
VEAPI
ObioAllocateZeroDeviceNode(
    IN PDEVICE_OBJECT PhysicalDeviceObject
)
{
    PDEVICE_NODE DeviceNode;

    DeviceNode = ObioCreateDeviceNode(PhysicalDeviceObject);
    if (!DeviceNode)
    {
        return NULL;
    }

    PhysicalDeviceObject->Flags &= ~DEVICE_INITIALIZING;

    DPRINT("Allocated DevNode %p (%Z)\n",
           DeviceNode,
           &DeviceNode->InstancePath);

    return DeviceNode;
}

VOID
VEAPI
ObioInsertDeviceNode(
    IN PDEVICE_NODE DeviceNode,
    IN PDEVICE_NODE ParentNode
)
{
    KIRQL OldIrql;

    KsAcquireSpinLock(&ObioDeviceTreeLock, &OldIrql);
    DeviceNode->Parent = ParentNode;
    DeviceNode->Sibling = NULL;

    /* Cek apakah Parent sudah punya anak, bukan si anak baru */
    if (ParentNode->LastChild == NULL)
    {
        ParentNode->Child = DeviceNode;
        ParentNode->LastChild = DeviceNode;
    }
    else
    {
        ParentNode->LastChild->Sibling = DeviceNode;
        ParentNode->LastChild = DeviceNode;
    }
    KsReleaseSpinLock(&ObioDeviceTreeLock, OldIrql);

    DeviceNode->Level = ParentNode->Level + 1;
}

VOID
VEAPI
ObioSetDevNodeProblem(
    IN PDEVICE_NODE DeviceNode,
    IN ULONG Problem
)
{
    DeviceNode->Flags |= DNF_PROBLEM;
    DeviceNode->Problem = Problem;
}

VEASTATUS
VEAPI
ObioStartDeviceFinal(
    IN PDEVICE_NODE DeviceNode
)
{
    /* TODO: Ask DEVICE_CAPABILITIES */
    ObioSetDevNodeState(DeviceNode, DeviceNodeStarted);

    return STATUS_SUCCESS;
}
