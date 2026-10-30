/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : querying.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Query ACPI DeviceID and InstanceID
 ----- Effective since 09-2026 til FOREVER ------------------------------- */

#include <veaddk.h>
#include "Include/vocpifunc.h"

/* Revision History ------------------------------------------------------
 * DATE       : 24-09-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file querying.c
 * --------------------------------------------------------------------- */

VOID
VEAPI
VOCPISanitizeIdString(
    IN OUT PCHAR Buffer
)
{
    if (!Buffer) return;

    for (PCHAR p = Buffer; *p != '\0'; p++)
    {
        /* 1. Ubah ke UPPERCASE */
        if (*p >= 'a' && *p <= 'z')
        {
            *p = *p - ('a' - 'A');
        }
        
        /* 2. Ganti spasi dan karakter ilegal dengan underscore */
        if (*p == ' ' || *p == '/' || *p == '\\' || *p == ',' || *p == '#')
        {
            *p = '_';
        }
    }
}

VEASTATUS
VEAPI
VOCPIEvaluateMethod(
    IN PVOCPI_NAMESPACE_NODE DeviceNode,
    IN PCHAR MethodName,
    OUT PVOCPI_OBJECT *OutObject
)
{
    PVOCPI_NAMESPACE_NODE TargetNode;

    if (!DeviceNode || !MethodName || !OutObject) 
        return STATUS_INVALID_PARAMETER;

    TargetNode = VOCPIGetChildNode(DeviceNode, MethodName);
    if (!TargetNode) 
        return STATUS_OBJECT_NAME_NOT_FOUND;

    /* if NodeName is static */
    if (TargetNode->NodeType == VocpiNodeName)
    {
        *OutObject = &TargetNode->Object;
        return STATUS_SUCCESS;
    }

    /* Need AML Execution */
    if (TargetNode->NodeType == VocpiNodeMethod)
    {
        DPRINT("[VOCPI] ERR: %s is a Method. AML Execution engine is required!\n", MethodName);
        
        /* TODO: LOOP METHOD EXECUTION */
        return STATUS_NOT_SUPPORTED;
    }

    /* 3. Tipe node lainnya (Scope, Device, OpRegion, dll) tidak bisa dievaluasi langsung ke Object */
    return STATUS_UNSUCCESSFUL;
}

VEASTATUS
VEAPI
VOCPIQueryDeviceId(
    IN PVOCPI_NAMESPACE_NODE DeviceNode,
    OUT PCHAR Buffer,
    IN ULONG BufferLength
)
{
    VEASTATUS Status;
    CHAR RawId[32];

    if (!DeviceNode || !Buffer || BufferLength == 0)
        return STATUS_INVALID_PARAMETER;

    RtlZeroMemory(RawId, sizeof(RawId));

    /* 1. Ambil raw _HID (Hardware ID utama) */
    Status = VOCPIEvaluateHardwareId(DeviceNode, "_HID", RawId, sizeof(RawId));
    if (!VEA_SUCCESS(Status))
    {
        /* 2. Jika _HID tidak ada, coba ambil _CID (Compatible ID) */
        Status = VOCPIEvaluateHardwareId(DeviceNode, "_CID", RawId, sizeof(RawId));
        if (!VEA_SUCCESS(Status))
        {
            return Status;
        }
    }

    /* 3. Gabungkan dengan prefix bus "ACPI\" -> Hasil: "ACPI\PNP0303" */
    return RtlStringCbPrintfA(Buffer, BufferLength, "ACPI/%s", RawId);
}

VEASTATUS
VEAPI
VOCPIQueryInstanceId(
    IN PVOCPI_NAMESPACE_NODE DeviceNode,
    OUT PCHAR Buffer,
    IN ULONG BufferLength
)
{
    if (!DeviceNode || !Buffer || BufferLength == 0)
        return STATUS_INVALID_PARAMETER;

    VEASTATUS Status = VOCPIEvaluateHardwareId(DeviceNode, "_UID", Buffer, BufferLength);
    
    if (VEA_SUCCESS(Status))
    {
        /* Jika _UID ada, bersihkan stringnya */
        VOCPISanitizeIdString(Buffer);
    }
    else
    {
        RtlStringCchCopyA(Buffer, BufferLength, "0");
        Status = STATUS_SUCCESS;
    }

    return Status;
}

VEASTATUS
VEAPI
VOCPIQueryNodeResourcesRequirement(
    IN PVOCPI_NAMESPACE_NODE NamespaceNode,
    OUT POBIO_REQUIRED_RESOURCES_LIST_EX *ListEx
)
{
    VEASTATUS Status;
    PVOCPI_OBJECT ReturnObject = NULL;

    if (!NamespaceNode || !ListEx)
        return STATUS_INVALID_PARAMETER;

    Status = VOCPIEvaluateMethod(NamespaceNode, "_CRS", &ReturnObject);
    if (!VEA_SUCCESS(Status) || !ReturnObject)
    {
        DPRINT("[VOCPI] _CRS not found or evaluation failed (0x%x)\n", Status);
        return Status;
    }

    if (ReturnObject->Type != VocpiTypeBuffer)
    {
        DPRINT("[VOCPI] _CRS returned invalid type: %d\n", ReturnObject->Type);
        return STATUS_INVALID_DATATYPE;
    }

    PUCHAR CrsBuffer = ReturnObject->Value.Buffer.Pointer;
    ULONG BufferSize = ReturnObject->Value.Buffer.Length;

    if (!CrsBuffer || BufferSize == 0)
        return STATUS_UNSUCCESSFUL;
    
    ULONG ResourceCount = 0;
    ULONG Index = 0;
    while (Index < BufferSize)
    {
        UCHAR Header = CrsBuffer[Index];
        if (!ACPI_RESOURCE_IS_LARGE(Header))
        {
            UCHAR Tag = ACPI_SMALL_RESOURCE_TAG(Header);
            if (Tag == ACPI_SMALL_TAG_END) break;
            
            if (Tag == ACPI_SMALL_TAG_IO || Tag == ACPI_SMALL_TAG_IRQ)
                ResourceCount++;
                
            Index += (1 + ACPI_SMALL_RESOURCE_LENGTH(Header));
        }
        else
        {
            USHORT LargeLength = *(PUSHORT)&CrsBuffer[Index + 1];
            Index += (3 + LargeLength);
        }
    }

    if (ResourceCount == 0)
        return STATUS_NOT_FOUND;

    ULONG AllocationSize = sizeof(OBIO_REQUIRED_RESOURCES_LIST_EX) +
                       ((ResourceCount - 1) * sizeof(OBIO_REQUIRED_RESOURCES));
                           
    POBIO_REQUIRED_RESOURCES_LIST_EX ResList = UlAllocatePoolZero(
        PagedPool, 
        AllocationSize, 
        'RsCV' // Tag for VOCPI Resource
    );

    if (!ResList) 
        return STATUS_INSUFFICIENT_MEMORY;

    
    ResList->ListSize = AllocationSize;
    ResList->AlternativeLists = 1;

    POBIO_REQUIRED_RESOURCES_LIST InnerList = &ResList->List[0];
    InnerList->Version = 1;
    InnerList->Revision = 1;
    InnerList->Count = 0;

    Index = 0;
    while (Index < BufferSize)
    {
        UCHAR Header = CrsBuffer[Index];
        if (!ACPI_RESOURCE_IS_LARGE(Header))
        {
            UCHAR Tag = ACPI_SMALL_RESOURCE_TAG(Header);
            if (Tag == ACPI_SMALL_TAG_END) break;

            if (Tag == ACPI_SMALL_TAG_IO)
            {
                USHORT MinPort = *(PUSHORT)&CrsBuffer[Index + 2];
                USHORT MaxPort = *(PUSHORT)&CrsBuffer[Index + 4];
                UCHAR Align    = CrsBuffer[Index + 6];
                UCHAR Length   = CrsBuffer[Index + 7];

                InnerList->Descriptors[InnerList->Count].Type = 1; // Port Resource
                InnerList->Descriptors[InnerList->Count].Port.MinimumAddress.QuadPart = MinPort;
                InnerList->Descriptors[InnerList->Count].Port.MaximumAddress.QuadPart = MaxPort;
                InnerList->Descriptors[InnerList->Count].Port.Length = Length;
                InnerList->Descriptors[InnerList->Count].Port.Alignment = Align;
                InnerList->Count++;
            }
            else if (Tag == ACPI_SMALL_TAG_IRQ)
            {
                USHORT IrqMask = *(PUSHORT)&CrsBuffer[Index + 1];
                ULONG IrqNum = 0;
                while ((IrqMask & 1) == 0 && IrqNum < 16) {
                    IrqMask >>= 1;
                    IrqNum++;
                }
                
                InnerList->Descriptors[InnerList->Count].Type = 2; // Asumsi macro untuk tipe Interrupt
                InnerList->Descriptors[InnerList->Count].Interrupt.MinimumVector = IrqNum;
                InnerList->Descriptors[InnerList->Count].Interrupt.MaximumVector = IrqNum;
                
                InnerList->Count++;
            }

            Index += (1 + ACPI_SMALL_RESOURCE_LENGTH(Header));
        }
        else
        {
            USHORT LargeLength = *(PUSHORT)&CrsBuffer[Index + 1];
            Index += (3 + LargeLength);
        }
    }

    *ListEx = ResList;
    return STATUS_SUCCESS;
}