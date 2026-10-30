/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : namespace.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Namespace creating on VOCPI
 ----- Effective since 09-2026 til FOREVER ------------------------------- */

#include <veaddk.h>
#include "Include/vocpifunc.h"
#include "tag.h"

/* Revision History ------------------------------------------------------
 * DATE       : 23-09-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file namespace.c
 * --------------------------------------------------------------------- */

PVOCPI_NAMESPACE_NODE VOCPIRootNode;
PVOCPI_NAMESPACE_NODE VOCPICurrentScope;

VEASTATUS
VEAPI
VOCPIInitNamespace(VOID)
{
    VOCPIRootNode = UlAllocatePoolZero(
        NonPagedPool,
        sizeof(VOCPI_NAMESPACE_NODE),
        'IpcV'
    );

    if (!VOCPIRootNode) return STATUS_INSUFFICIENT_MEMORY;

    VOCPIRootNode->Name[0] = '\\';
    VOCPIRootNode->NodeType = VocpiNodeRoot;

    VOCPICurrentScope = VOCPIRootNode;

    return STATUS_SUCCESS;
}

PVOCPI_NAMESPACE_NODE
VEAPI
VOCPICreateNamespaceNode(
    IN PVOCPI_NAMESPACE_NODE Parent,
    IN PCHAR NameSeg,
    IN VOCPI_NODE_TYPE NodeType
)
{
    if (!Parent || !NameSeg) return NULL;

    for (ULONG Index = 0; Index < 4; Index++)
    {
        CHAR Character = NameSeg[Index];
        if (!((Character >= 'A' && Character <= 'Z') ||
              (Character >= '0' && Character <= '9') ||
              Character == '_'))
        {
            DPRINT("[VOCPI] Rejecting invalid NameSeg: %c%c%c%c\n",
                   NameSeg[0], NameSeg[1], NameSeg[2], NameSeg[3]);
            return NULL;
        }
    }

    /* AML may reopen the same scope from multiple declarations. */
    PVOCPI_NAMESPACE_NODE ExistingNode = Parent->Child;
    while (ExistingNode)
    {
        if (ExistingNode->Name[0] == NameSeg[0] &&
            ExistingNode->Name[1] == NameSeg[1] &&
            ExistingNode->Name[2] == NameSeg[2] &&
            ExistingNode->Name[3] == NameSeg[3])
        {
            if (NodeType == VocpiNodeDevice ||
                (NodeType == VocpiNodeName &&
                 ExistingNode->NodeType == VocpiNodeScope))
            {
                ExistingNode->NodeType = NodeType;
            }
            return ExistingNode;
        }
        ExistingNode = ExistingNode->Next;
    }

    /* Alokasikan node baru */
    PVOCPI_NAMESPACE_NODE NewNode = (PVOCPI_NAMESPACE_NODE)UlAllocatePoolWithTag(
        NonPagedPool,
        sizeof(VOCPI_NAMESPACE_NODE),
        'IpcV'
    );

    if (!NewNode) return NULL;

    RtlZeroMemory(NewNode, sizeof(VOCPI_NAMESPACE_NODE));
    RtlCopyMemory(NewNode->Name, NameSeg, 4);
    NewNode->NodeType = NodeType;
    NewNode->Parent = Parent;

    /* Sisipkan ke dalam rantai Child / Sibling */
    if (!Parent->Child)
    {
        Parent->Child = NewNode;
    }
    else
    {
        PVOCPI_NAMESPACE_NODE Sibling = Parent->Child;
        while (Sibling->Next)
        {
            Sibling = Sibling->Next;
        }
        Sibling->Next = NewNode;
    }

    return NewNode;
}

VOID
VEAPI
VOCPIDumpNamespace(
    IN PVOCPI_NAMESPACE_NODE Node,
    IN ULONG Depth
)
{
    if (!Node) return;

    /* Indentasi biar keliatan struktur pohonnya */
    CHAR Indent[32];
    RtlZeroMemory(Indent, 32);
    ULONG IndentLen = (Depth * 2 < 30) ? Depth * 2 : 30;
    for (ULONG i = 0; i < IndentLen; i++)
    {
        Indent[i] = ' ';
    }

    /* Print Node Name (4 Char) & Type */
    DPRINT("%s|-- [%c%c%c%c] (Type: %d)\n",
           Indent,
           Node->Name[0] ? Node->Name[0] : ' ',
           Node->Name[1] ? Node->Name[1] : ' ',
           Node->Name[2] ? Node->Name[2] : ' ',
           Node->Name[3] ? Node->Name[3] : ' ',
           Node->NodeType);

    /* Walk ke Anak (Child) */
    if (Node->Child)
    {
        VOCPIDumpNamespace(Node->Child, Depth + 1);
    }

    /* Walk ke Saudara (Next / Sibling) */
    if (Node->Next)
    {
        VOCPIDumpNamespace(Node->Next, Depth);
    }
}

VOID
VEAPI
VOCPIWalkNamespace(
    IN PVOCPI_NAMESPACE_NODE Node,
    IN VOCPI_NODE_CALLBACK Callback,
    IN PVOID Context
)
{
    if (!Node) return;

    Callback(Node, Context);

    /* Turun ke Child dulu (depth-first) */
    if (Node->Child)
        VOCPIWalkNamespace(Node->Child, Callback, Context);

    /* Baru ke Sibling */
    if (Node->Next)
        VOCPIWalkNamespace(Node->Next, Callback, Context);
}

VOID
VEAPI
VOCPICountDeviceType2(
    IN PVOCPI_NAMESPACE_NODE NamespaceNode,
    IN PVOID Context
)
{
    PULONG Counter = (PVOID)Context;

    if (NamespaceNode->NodeType == VocpiNodeDevice)
    {
        (*Counter)++;
    }
}

typedef struct _VOCPI_ENUM_CONTEXT {
    PDRIVER_OBJECT DriverObject;
    PDEVICE_RELATIONS Relations;
    PDEVICE_OBJECT Fdo;
} VOCPI_ENUM_CONTEXT, *PVOCPI_ENUM_CONTEXT;

VOID
VEAPI
VOCPIInsertDeviceNodeToDeviceRelations(
    IN PVOCPI_NAMESPACE_NODE NamespaceNode,
    IN PVOID Context
)
{
    PVOCPI_ENUM_CONTEXT EnumContext = (PVOCPI_ENUM_CONTEXT)Context;
    PDEVICE_RELATIONS DeviceRelations = EnumContext->Relations;
    PDRIVER_OBJECT DriverObject = EnumContext->DriverObject;
    PDEVICE_OBJECT ACPIDO;
    VEASTATUS Status;

    if (NamespaceNode->NodeType == VocpiNodeDevice)
    {      
        PVOCPI_PDO_EXTENSION DevExt;

        Status = ObioCreateNewDevice(
            DriverObject,
            sizeof(VOCPI_PDO_EXTENSION),
            NULL,
            FILE_DEVICE_BUS_EXTENDER,
            &ACPIDO
        );
        
        if (VEA_SUCCESS(Status))
        {
            ACPIDO->Flags &= ~DO_DEVICE_INITIALIZING;

            DeviceRelations->Objects[DeviceRelations->Count] = ACPIDO;
            DeviceRelations->Count++;

            DevExt = (PVOCPI_PDO_EXTENSION)ACPIDO->DeviceExtension;

            DevExt->AcpiNode = NamespaceNode;
            DevExt->Fdo = EnumContext->Fdo;
            DevExt->IsStarted = FALSE;
            DevExt->Signature = VOCPI_PDO_SIGNATURE;
        }
    }
}

VEASTATUS
VEAPI
VOCPIQueryDeviceRelations(
    IN PDEVICE_OBJECT Fdo,
    IN OUT PDEVICE_RELATIONS *OutRelations
)
{
    ULONG Count = 0;
    PDEVICE_RELATIONS DeviceRelations;

    /* Count the device first */
    DPRINT("TRY TO WALK NAMESPACE FOR COUNT\n");
    VOCPIWalkNamespace(VOCPIRootNode, VOCPICountDeviceType2, (PVOID)&Count);
    DPRINT("WALK DONE\n");

    if (Count > 0)
    {
        ULONG AllocationSize;

        DPRINT("[VOCPI] DeviceObject count: %d\n", Count);

        AllocationSize = sizeof(DEVICE_RELATIONS) + (sizeof(PDEVICE_OBJECT) * (Count - 1));

        DeviceRelations = UlAllocatePoolZero(
            NonPagedPool,
            AllocationSize,
            TAG_OBIO
        );

        if (!DeviceRelations)
        {
            DPRINT("[VOCPI] ERR: DeviceRelations can't be allocated\n");
            return STATUS_INSUFFICIENT_MEMORY;
        }

        DeviceRelations->Count = 0;

        VOCPI_ENUM_CONTEXT EnumCtx;
        EnumCtx.DriverObject = Fdo->DriverObject; // <-- INI JAWABAN PERTANYAANMU
        EnumCtx.Relations = DeviceRelations;
        EnumCtx.Fdo = Fdo;

        VOCPIWalkNamespace(VOCPIRootNode, VOCPIInsertDeviceNodeToDeviceRelations, &EnumCtx);

        *OutRelations = DeviceRelations;
    }
    else
    {
        *OutRelations = NULL;
    }

    return STATUS_SUCCESS;
}