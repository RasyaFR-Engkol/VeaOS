/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : package.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Processing AML bytecode
 ----- Effective since 09-2026 til FOREVER ------------------------------- */

#include <veaddk.h>
#include "Include/vocpifunc.h"

/* Revision History ------------------------------------------------------
 * DATE       : 23-09-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file package.c
 * --------------------------------------------------------------------- */

ULONG
VEAPI
VOCPIPkgParseLength(OUT PULONG BytesRead OPTIONAL)
{
    PUCHAR BytePtr = VOCPIGetAndPushNextAmlByte();
    if (!BytePtr)
    {
        if (BytesRead) *BytesRead = 0;
        return 0;
    }

    UCHAR LeadByte = (UCHAR)*BytePtr;
    UCHAR ByteCount = (LeadByte >> 6) & 0x03;
    ULONG PkgLength = 0;

    if (ByteCount == 0)
    {
        /* Format 1 Byte (Nilai 0 - 63) */
        PkgLength = LeadByte & 0x3F;
        if (BytesRead) *BytesRead = 1;
    }
    else
    {
        /* Format Multi-Byte (2, 3, atau 4 Byte) */
        PkgLength = LeadByte & 0x0F; /* Bit 0-3 dari LeadByte */

        for (UCHAR i = 0; i < ByteCount; i++)
        {
            BytePtr = VOCPIGetAndPushNextAmlByte();
            if (!BytePtr) break;

            /* Gabungkan byte berikutnya dengan pergeseran 8-bit */
            PkgLength |= ((ULONG)(UCHAR)*BytePtr) << (4 + (i * 8));
        }

        if (BytesRead) *BytesRead = (ULONG)(ByteCount + 1);
    }

    return PkgLength;
}

static
VEASTATUS
VOCPIConsumeNameString(VOID)
{
    PUCHAR BytePtr = VOCPIGetAndPushNextAmlByte();
    if (!BytePtr) return STATUS_UNSUCCESSFUL;

    if (*BytePtr == '\\')
        BytePtr = VOCPIGetAndPushNextAmlByte();

    while (BytePtr && *BytePtr == '^')
        BytePtr = VOCPIGetAndPushNextAmlByte();

    if (!BytePtr) return STATUS_UNSUCCESSFUL;
    if (*BytePtr == 0x00) return STATUS_SUCCESS;

    if (*BytePtr == 0x2E)
    {
        if (!VOCPIGetAndPushNextAmlByte()) return STATUS_UNSUCCESSFUL;
        if (!VOCPIGetAndPushNextAmlByte()) return STATUS_UNSUCCESSFUL;
        if (!VOCPIGetAndPushNextAmlByte()) return STATUS_UNSUCCESSFUL;
        if (!VOCPIGetAndPushNextAmlByte()) return STATUS_UNSUCCESSFUL;
        if (!VOCPIGetAndPushNextAmlByte()) return STATUS_UNSUCCESSFUL;
        if (!VOCPIGetAndPushNextAmlByte()) return STATUS_UNSUCCESSFUL;
        if (!VOCPIGetAndPushNextAmlByte()) return STATUS_UNSUCCESSFUL;
        if (!VOCPIGetAndPushNextAmlByte()) return STATUS_UNSUCCESSFUL;
        return STATUS_SUCCESS;
    }

    if (*BytePtr == 0x2F)
    {
        PUCHAR CountPtr = VOCPIGetAndPushNextAmlByte();
        if (!CountPtr || *CountPtr == 0 || *CountPtr > 32)
            return STATUS_UNSUCCESSFUL;
        for (ULONG Index = 0; Index < (ULONG)*CountPtr * 4; Index++)
            if (!VOCPIGetAndPushNextAmlByte()) return STATUS_UNSUCCESSFUL;
        return STATUS_SUCCESS;
    }

    for (ULONG Index = 1; Index < 4; Index++)
        if (!VOCPIGetAndPushNextAmlByte()) return STATUS_UNSUCCESSFUL;
    return STATUS_SUCCESS;
}

static
VEASTATUS
VOCPIConsumeTermArg(PVOCPI_NAMESPACE_NODE Node)
{
    PUCHAR BytePtr = VOCPIGetAndPushNextAmlByte();
    if (!BytePtr) return STATUS_UNSUCCESSFUL;

    switch (*BytePtr)
    {
        case AML_ZERO_OP:
            if (Node) { Node->Object.Type = VocpiTypeInteger; Node->Object.Value.Integer = 0; }
            return STATUS_SUCCESS;
        case AML_ONE_OP:
            if (Node) { Node->Object.Type = VocpiTypeInteger; Node->Object.Value.Integer = 1; }
            return STATUS_SUCCESS;
        case AML_BYTE_PREFIX:
        {
            PUCHAR Val = VOCPIGetAndPushNextAmlByte();
            if (!Val) return STATUS_UNSUCCESSFUL;
            if (Node) { Node->Object.Type = VocpiTypeInteger; Node->Object.Value.Integer = *Val; }
            return STATUS_SUCCESS;
        }
        case AML_WORD_PREFIX:
        {
            ULONG Val = 0;
            for (ULONG i = 0; i < 2; i++) {
                PUCHAR B = VOCPIGetAndPushNextAmlByte();
                if (!B) return STATUS_UNSUCCESSFUL;
                Val |= (*B << (i * 8));
            }
            if (Node) { Node->Object.Type = VocpiTypeInteger; Node->Object.Value.Integer = Val; }
            return STATUS_SUCCESS;
        }
        case AML_DWORD_PREFIX:
        {
            ULONG Val = 0;
            for (ULONG i = 0; i < 4; i++) {
                PUCHAR B = VOCPIGetAndPushNextAmlByte();
                if (!B) return STATUS_UNSUCCESSFUL;
                Val |= (*B << (i * 8));
            }
            if (Node) { Node->Object.Type = VocpiTypeInteger; Node->Object.Value.Integer = Val; }
            return STATUS_SUCCESS;
        }
        case AML_STRING_PREFIX:
        {
            /* Hitung panjang string dulu (scan sampai NULL) */
            ULONG StartOffset = VOCPIPointerAmlByte;
            ULONG Length = 0;
            PUCHAR StringByte;
            while ((StringByte = VOCPIGetAndPushNextAmlByte()) != NULL && *StringByte != 0) {
                Length++;
            }
            
            if (Node && Length > 0) {
                Node->Object.Type = VocpiTypeString;
                Node->Object.Value.String.Length = Length;
                Node->Object.Value.String.String = UlAllocatePoolZero(NonPagedPool, Length + 1, 'IpcV');
                if (Node->Object.Value.String.String) {
                    RtlCopyMemory(Node->Object.Value.String.String, &VOCPIAmlByte[StartOffset], Length);
                }
            }
            return STATUS_SUCCESS;
        }
        case AML_BUFFER_OP:
        {
            ULONG PackageLengthBytes = 0;
            ULONG PackageLength = VOCPIPkgParseLength(&PackageLengthBytes);
            ULONG PackageEnd;

            if (PackageLengthBytes == 0 || PackageLength < PackageLengthBytes)
                return STATUS_UNSUCCESSFUL;

            PackageEnd = (VOCPIPointerAmlByte - PackageLengthBytes) + PackageLength;
            if (PackageEnd > VOCPIAmlLength)
                return STATUS_UNSUCCESSFUL;

            /* 1. Konsumsi BufferSize (TermArg) untuk mencapai awal raw Resource Descriptor */
            VOCPIConsumeTermArg(NULL);

            /* 2. Simpan pointer dan panjang raw buffer data */
            ULONG RawBytesStart = VOCPIPointerAmlByte;
            ULONG RawBytesLength = (PackageEnd > RawBytesStart) ? (PackageEnd - RawBytesStart) : 0;

            if (Node)
            {
                Node->Object.Type = VocpiTypeBuffer;
                Node->Object.Value.Buffer.Pointer = &VOCPIAmlByte[RawBytesStart];
                Node->Object.Value.Buffer.Length = RawBytesLength;
            }

            VOCPIPointerAmlByte = PackageEnd;
            return STATUS_SUCCESS;
        }
        case AML_PACKAGE_OP:
        case AML_VAR_PACKAGE_OP:
        {
            ULONG PackageLengthBytes = 0;
            ULONG PackageLength = VOCPIPkgParseLength(&PackageLengthBytes);
            ULONG PackageEnd;

            if (PackageLengthBytes == 0 || PackageLength < PackageLengthBytes)
                return STATUS_UNSUCCESSFUL;

            PackageEnd = (VOCPIPointerAmlByte - PackageLengthBytes) + PackageLength;
            if (PackageEnd > VOCPIAmlLength)
                return STATUS_UNSUCCESSFUL;

            VOCPIPointerAmlByte = PackageEnd;
            return STATUS_SUCCESS;
        }
        default:
            VOCPIPointerAmlByte--;
            return VOCPIConsumeNameString();
    }
}

VEASTATUS
VEAPI
VOCPIParseNameSeg(
    OUT PCHAR NameSegBuffer
)
{
    if (!NameSegBuffer) return STATUS_INVALID_PARAMETER;

    for (ULONG i = 0; i < 4; i++)
    {
        PUCHAR BytePtr = VOCPIGetAndPushNextAmlByte();
        if (!BytePtr) return STATUS_UNSUCCESSFUL;

        NameSegBuffer[i] = *BytePtr;
    }
    NameSegBuffer[4] = '\0'; /* BAHAYA: Dulu hilang, wajib ada null-terminator! */

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
VOCPIProcessNameOp(VOID)
{
    CHAR NamePath[8][5];
    ULONG NamePathCount = 0;
    PVOCPI_NAMESPACE_NODE NameParent = VOCPICurrentScope;
    PUCHAR NameByte = VOCPIGetAndPushNextAmlByte();

    if (!NameByte) return STATUS_UNSUCCESSFUL;

    if (*NameByte == '\\')
    {
        NameParent = VOCPIRootNode;
        NameByte = VOCPIGetAndPushNextAmlByte();
        if (!NameByte) return STATUS_UNSUCCESSFUL;
    }

    while (*NameByte == '^')
    {
        if (NameParent->Parent) NameParent = NameParent->Parent;
        NameByte = VOCPIGetAndPushNextAmlByte();
        if (!NameByte) return STATUS_UNSUCCESSFUL;
    }

    if (*NameByte == 0x2E)
    {
        if (VOCPIParseNameSeg(NamePath[0]) != STATUS_SUCCESS ||
            VOCPIParseNameSeg(NamePath[1]) != STATUS_SUCCESS)
            return STATUS_UNSUCCESSFUL;
        NamePathCount = 2;
    }
    else if (*NameByte == 0x2F)
    {
        PUCHAR CountByte = VOCPIGetAndPushNextAmlByte();
        if (!CountByte || *CountByte == 0 || *CountByte > 8)
            return STATUS_UNSUCCESSFUL;
        NamePathCount = *CountByte;
        for (ULONG Index = 0; Index < NamePathCount; Index++)
        {
            if (VOCPIParseNameSeg(NamePath[Index]) != STATUS_SUCCESS)
                return STATUS_UNSUCCESSFUL;
        }
    }
    else
    {
        NamePath[0][0] = *NameByte;
        for (ULONG Index = 1; Index < 4; Index++)
        {
            NameByte = VOCPIGetAndPushNextAmlByte();
            if (!NameByte) return STATUS_UNSUCCESSFUL;
            NamePath[0][Index] = *NameByte;
        }
        NamePath[0][4] = '\0'; /* Tambahkan ini */
        NamePathCount = 1;
    }

    for (ULONG Index = 0; Index < NamePathCount; Index++)
    {
        NameParent = VOCPICreateNamespaceNode(
            NameParent,
            NamePath[Index],
            Index + 1 == NamePathCount ? VocpiNodeName : VocpiNodeScope
        );
        if (!NameParent) return STATUS_INSUFFICIENT_MEMORY;
    }

    return VOCPIConsumeTermArg(NameParent);
}

VEASTATUS
VEAPI
VOCPIProcessScopeOp(VOID)
{
    ULONG PkgLengthRead = 0;
    CHAR NameSeg[5];
    CHAR NamePath[8][5];
    ULONG NamePathCount = 0;
    ULONG PackageEnd;
    PVOCPI_NAMESPACE_NODE ScopeParent;
    PVOCPI_NAMESPACE_NODE ScopeNode;

    /* 1. Read total panjang blok Scope */
    ULONG ScopeLength = VOCPIPkgParseLength(&PkgLengthRead);

    if (PkgLengthRead == 0 || ScopeLength < PkgLengthRead)
    {
        DPRINT("[VOCPI] Scope package length invalid: length=%u bytes=%u offset=0x%x\n",
               ScopeLength, PkgLengthRead, VOCPIPointerAmlByte);
        return STATUS_UNSUCCESSFUL;
    }

    PackageEnd = (VOCPIPointerAmlByte - PkgLengthRead) + ScopeLength;
    if (PackageEnd > VOCPIAmlLength)
    {
        DPRINT("[VOCPI] Scope package exceeds AML: end=0x%x length=0x%x\n",
               PackageEnd, VOCPIAmlLength);
        return STATUS_UNSUCCESSFUL;
    }

    ScopeParent = VOCPICurrentScope;

    /* Scope NameString may contain RootChar, ParentPrefix, or path prefixes. */
    PUCHAR NameByte = VOCPIGetAndPushNextAmlByte();
        if (!NameByte)
        {
         DPRINT("[VOCPI] Scope NameString missing at offset=0x%x\n",
             VOCPIPointerAmlByte);
         return STATUS_UNSUCCESSFUL;
        }

        DPRINT("[VOCPI] Scope package: length=%u bytes=%u end=0x%x name0=0x%x\n",
            ScopeLength, PkgLengthRead, PackageEnd, *NameByte);

    if (*NameByte == '\\')
    {
        ScopeParent = VOCPIRootNode;
        NameByte = VOCPIGetAndPushNextAmlByte();
        if (!NameByte) return STATUS_UNSUCCESSFUL;
    }
    else
    {
        while (*NameByte == '^')
        {
            if (ScopeParent->Parent) ScopeParent = ScopeParent->Parent;
            NameByte = VOCPIGetAndPushNextAmlByte();
            if (!NameByte) return STATUS_UNSUCCESSFUL;
        }
    }

    if (*NameByte == 0x00)
    {
        /* NullName: the package body stays in the current scope. */
        NamePathCount = 0;
    }
    else if (*NameByte == 0x2E)
    {
        if (VOCPIParseNameSeg(NamePath[0]) != STATUS_SUCCESS ||
            VOCPIParseNameSeg(NamePath[1]) != STATUS_SUCCESS)
            return STATUS_UNSUCCESSFUL;
        NamePathCount = 2;
    }
    else if (*NameByte == 0x2F)
    {
        PUCHAR CountByte = VOCPIGetAndPushNextAmlByte();
        if (!CountByte || *CountByte == 0 || *CountByte > 8)
            return STATUS_UNSUCCESSFUL;
        NamePathCount = *CountByte;
        for (ULONG Index = 0; Index < NamePathCount; Index++)
        {
            if (VOCPIParseNameSeg(NamePath[Index]) != STATUS_SUCCESS)
                return STATUS_UNSUCCESSFUL;
        }
    }
    else
    {
        NameSeg[0] = *NameByte;
        for (ULONG Index = 1; Index < 4; Index++)
        {
            NameByte = VOCPIGetAndPushNextAmlByte();
            if (!NameByte) return STATUS_UNSUCCESSFUL;
            NameSeg[Index] = *NameByte;
        }
        NameSeg[4] = '\0'; /* Tambahkan ini */
        RtlCopyMemory(NamePath[0], NameSeg, sizeof(NameSeg));
        NamePathCount = 1;
    }

    ScopeNode = ScopeParent;
    for (ULONG Index = 0; Index < NamePathCount; Index++)
    {
        ScopeNode = VOCPICreateNamespaceNode(
            ScopeNode,
            NamePath[Index],
            VocpiNodeScope
        );
        if (!ScopeNode)
        {
            DPRINT("[VOCPI] Skipping invalid Scope package at end=0x%x\n",
                   PackageEnd);
            VOCPIPointerAmlByte = PackageEnd;
            return STATUS_SUCCESS;
        }
    }

    if (ScopeNode)
    {
        PVOCPI_NAMESPACE_NODE PreviousScope = VOCPICurrentScope;
        VOCPICurrentScope = ScopeNode;

        while (VOCPIPointerAmlByte < PackageEnd)
        {
            PUCHAR ByteCodePtr = VOCPIGetAndPushNextAmlByte();
            if (!ByteCodePtr) break;

            VEASTATUS Status;
            if (!VOCPIProcessByteCode((UCHAR)*ByteCodePtr, &Status) ||
                !VEA_SUCCESS(Status))
            {
                DPRINT("[VOCPI] Scope body failed at offset=0x%x opcode=0x%x status=0x%x\n",
                       VOCPIPointerAmlByte - 1,
                       (UCHAR)*ByteCodePtr,
                       Status);
                VOCPICurrentScope = PreviousScope;
                return STATUS_UNSUCCESSFUL;
            }
        }

        VOCPICurrentScope = PreviousScope;
        return STATUS_SUCCESS;
    }

    return STATUS_INSUFFICIENT_MEMORY;
}

VEASTATUS
VEAPI
VOCPIProcessMethodOp(VOID)
{
    ULONG PackageLengthBytes = 0;
    ULONG PackageLength = VOCPIPkgParseLength(&PackageLengthBytes);
    ULONG PackageEnd;
    ULONG BodyStart;
    PUCHAR NameByte;
    PUCHAR FlagsByte;
    PVOCPI_NAMESPACE_NODE MethodParent;
    PVOCPI_NAMESPACE_NODE MethodNode;
    CHAR MethodPath[8][5];
    ULONG MethodPathCount = 0;
    MethodParent = VOCPICurrentScope;

    if (PackageLengthBytes == 0 || PackageLength < PackageLengthBytes)
        return STATUS_UNSUCCESSFUL;

    PackageEnd = (VOCPIPointerAmlByte - PackageLengthBytes) + PackageLength;
    if (PackageEnd > VOCPIAmlLength)
        return STATUS_UNSUCCESSFUL;

    NameByte = VOCPIGetAndPushNextAmlByte();
    if (!NameByte) return STATUS_UNSUCCESSFUL;

    if (*NameByte == '\\')
    {
        MethodParent = VOCPIRootNode;
        NameByte = VOCPIGetAndPushNextAmlByte();
        if (!NameByte) return STATUS_UNSUCCESSFUL;
    }

    while (*NameByte == '^')
    {
        if (MethodParent->Parent) MethodParent = MethodParent->Parent;
        NameByte = VOCPIGetAndPushNextAmlByte();
        if (!NameByte) return STATUS_UNSUCCESSFUL;
    }

    if (*NameByte == 0x2E)
    {
        if (VOCPIParseNameSeg(MethodPath[0]) != STATUS_SUCCESS ||
            VOCPIParseNameSeg(MethodPath[1]) != STATUS_SUCCESS)
            return STATUS_UNSUCCESSFUL;
        MethodPathCount = 2;
    }
    else if (*NameByte == 0x2F)
    {
        PUCHAR CountByte = VOCPIGetAndPushNextAmlByte();
        if (!CountByte || *CountByte == 0 || *CountByte > 8)
            return STATUS_UNSUCCESSFUL;
        MethodPathCount = *CountByte;
        for (ULONG Index = 0; Index < MethodPathCount; Index++)
        {
            if (VOCPIParseNameSeg(MethodPath[Index]) != STATUS_SUCCESS)
                return STATUS_UNSUCCESSFUL;
        }
    }
    else
    {
        MethodPath[0][0] = *NameByte;
        for (ULONG Index = 1; Index < 4; Index++)
        {
            NameByte = VOCPIGetAndPushNextAmlByte();
            if (!NameByte) return STATUS_UNSUCCESSFUL;
            MethodPath[0][Index] = *NameByte;
        }
        MethodPath[0][4] = '\0'; /* Tambahkan ini */
        MethodPathCount = 1;
    }

    /* MethodFlags: bit 0-2 ArgCount, bit 3 SerializeFlag, 
       bit 4-7 SyncLevel --- */
    FlagsByte = VOCPIGetAndPushNextAmlByte();
    if (!FlagsByte) return STATUS_UNSUCCESSFUL;

    MethodNode = MethodParent;
    for (ULONG Index = 0; Index < MethodPathCount; Index++)
    {
        MethodNode = VOCPICreateNamespaceNode(
            MethodNode,
            MethodPath[Index],
            Index + 1 == MethodPathCount ? VocpiNodeMethod : VocpiNodeScope
        );
        if (!MethodNode) return STATUS_INSUFFICIENT_MEMORY;
    }

    BodyStart = VOCPIPointerAmlByte;
    MethodNode->Object.Type = VocpiTypeBuffer;
    MethodNode->Object.Value.Buffer.Pointer = &VOCPIAmlByte[BodyStart];
    MethodNode->Object.Value.Buffer.Length  = PackageEnd - BodyStart;

    VOCPIPointerAmlByte = PackageEnd;
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
VOCPIProcessOpRegionOp(VOID)
{
    VEASTATUS Status = VOCPIConsumeNameString();
    if (!VEA_SUCCESS(Status)) return Status;

    if (!VOCPIGetAndPushNextAmlByte())
        return STATUS_UNSUCCESSFUL;

    Status = VOCPIConsumeTermArg(NULL);
    if (!VEA_SUCCESS(Status)) return Status;
    return VOCPIConsumeTermArg(NULL);
}

VEASTATUS
VEAPI
VOCPIProcessDataRegionOp(VOID)
{
    VEASTATUS Status = VOCPIConsumeNameString();
    if (!VEA_SUCCESS(Status)) return Status;

    Status = VOCPIConsumeTermArg(NULL);
    if (!VEA_SUCCESS(Status)) return Status;
    Status = VOCPIConsumeTermArg(NULL);
    if (!VEA_SUCCESS(Status)) return Status;
    return VOCPIConsumeTermArg(NULL);
}

VEASTATUS
VEAPI
VOCPIProcessFieldOp(VOID)
{
    ULONG PackageLengthBytes = 0;
    ULONG PackageLength = VOCPIPkgParseLength(&PackageLengthBytes);
    ULONG PackageEnd;

    if (PackageLengthBytes == 0 || PackageLength < PackageLengthBytes)
        return STATUS_UNSUCCESSFUL;

    PackageEnd = (VOCPIPointerAmlByte - PackageLengthBytes) + PackageLength;
    if (PackageEnd > VOCPIAmlLength)
        return STATUS_UNSUCCESSFUL;

    VOCPIPointerAmlByte = PackageEnd;
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
VOCPIProcessPackageBodyOp(VOID)
{
    ULONG PackageLengthBytes = 0;
    ULONG PackageLength = VOCPIPkgParseLength(&PackageLengthBytes);
    ULONG PackageEnd;

    if (PackageLengthBytes == 0 || PackageLength < PackageLengthBytes)
        return STATUS_UNSUCCESSFUL;

    PackageEnd = (VOCPIPointerAmlByte - PackageLengthBytes) + PackageLength;
    if (PackageEnd > VOCPIAmlLength)
        return STATUS_UNSUCCESSFUL;

    VOCPIPointerAmlByte = PackageEnd;
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
VOCPIProcessDeviceOp(VOID)
{
    ULONG PackageLengthBytes = 0;
    ULONG PackageLength = VOCPIPkgParseLength(&PackageLengthBytes);
    ULONG PackageEnd;
    CHAR DevicePath[8][5];
    ULONG DevicePathCount = 0;
    PVOCPI_NAMESPACE_NODE DeviceParent;
    PUCHAR NameByte;
    PVOCPI_NAMESPACE_NODE DeviceNode;
    PVOCPI_NAMESPACE_NODE PreviousScope;

    if (PackageLengthBytes == 0 || PackageLength < PackageLengthBytes)
        return STATUS_UNSUCCESSFUL;

    PackageEnd = (VOCPIPointerAmlByte - PackageLengthBytes) + PackageLength;
    if (PackageEnd > VOCPIAmlLength)
        return STATUS_UNSUCCESSFUL;

    DeviceParent = VOCPICurrentScope;
    NameByte = VOCPIGetAndPushNextAmlByte();
    if (!NameByte) return STATUS_UNSUCCESSFUL;

    if (*NameByte == '\\')
    {
        DeviceParent = VOCPIRootNode;
        NameByte = VOCPIGetAndPushNextAmlByte();
        if (!NameByte) return STATUS_UNSUCCESSFUL;
    }

    while (*NameByte == '^')
    {
        if (DeviceParent->Parent) DeviceParent = DeviceParent->Parent;
        NameByte = VOCPIGetAndPushNextAmlByte();
        if (!NameByte) return STATUS_UNSUCCESSFUL;
    }

    if (*NameByte == 0x2E)
    {
        if (VOCPIParseNameSeg(DevicePath[0]) != STATUS_SUCCESS ||
            VOCPIParseNameSeg(DevicePath[1]) != STATUS_SUCCESS)
            return STATUS_UNSUCCESSFUL;
        DevicePathCount = 2;
    }
    else if (*NameByte == 0x2F)
    {
        PUCHAR CountByte = VOCPIGetAndPushNextAmlByte();
        if (!CountByte || *CountByte == 0 || *CountByte > 8)
            return STATUS_UNSUCCESSFUL;
        DevicePathCount = *CountByte;
        for (ULONG Index = 0; Index < DevicePathCount; Index++)
        {
            if (VOCPIParseNameSeg(DevicePath[Index]) != STATUS_SUCCESS)
                return STATUS_UNSUCCESSFUL;
        }
    }
    else
    {
        DevicePath[0][0] = *NameByte;
        for (ULONG Index = 1; Index < 4; Index++)
        {
            NameByte = VOCPIGetAndPushNextAmlByte();
            if (!NameByte) return STATUS_UNSUCCESSFUL;
            DevicePath[0][Index] = *NameByte;
        }
        DevicePath[0][4] = '\0'; /* Tambahkan ini */
        DevicePathCount = 1;
    }

    DeviceNode = DeviceParent;
    for (ULONG Index = 0; Index < DevicePathCount; Index++)
    {
        DeviceNode = VOCPICreateNamespaceNode(
            DeviceNode,
            DevicePath[Index],
            Index + 1 == DevicePathCount ? VocpiNodeDevice : VocpiNodeScope
        );
        if (!DeviceNode)
        {
            DPRINT("[VOCPI] Skipping invalid Device package at end=0x%x\n",
                   PackageEnd);
            VOCPIPointerAmlByte = PackageEnd;
            return STATUS_SUCCESS;
        }
    }

    PreviousScope = VOCPICurrentScope;
    VOCPICurrentScope = DeviceNode;

    while (VOCPIPointerAmlByte < PackageEnd)
    {
        PUCHAR ByteCodePtr = VOCPIGetAndPushNextAmlByte();
        VEASTATUS Status;

        if (!ByteCodePtr) break;
        if (!VOCPIProcessByteCode((UCHAR)*ByteCodePtr, &Status) ||
            !VEA_SUCCESS(Status))
        {
            DPRINT("[VOCPI] Device body failed at offset=0x%x opcode=0x%x status=0x%x\n",
                   VOCPIPointerAmlByte - 1,
                   (UCHAR)*ByteCodePtr,
                   Status);
            VOCPICurrentScope = PreviousScope;
            return STATUS_UNSUCCESSFUL;
        }
    }

    VOCPICurrentScope = PreviousScope;
    return STATUS_SUCCESS;
}

PVOCPI_NAMESPACE_NODE
VEAPI
VOCPIGetChildNode(
    IN PVOCPI_NAMESPACE_NODE Parent,
    IN PCHAR NameSeg
)
{
    if (!Parent || !Parent->Child || !NameSeg) return NULL;

    PVOCPI_NAMESPACE_NODE Current = Parent->Child;
    while (Current)
    {
        if (Current->Name[0] == NameSeg[0] &&
            Current->Name[1] == NameSeg[1] &&
            Current->Name[2] == NameSeg[2] &&
            Current->Name[3] == NameSeg[3])
        {
            return Current;
        }
        Current = Current->Next;
    }

    return NULL;
}

VOID
VEAPI
VOCPIDecodeEisaId(
    IN ULONG EisaId,
    OUT PCHAR OutString,
    IN SIZE_T BufferLength
)
{
    /* Decompressed 3 ASCII */
    ULONG Vendor = (EisaId & 0xFFFF);
    ULONG VendorRev = ((Vendor & 0xFF) << 8) | (Vendor >> 8);
    
    CHAR c1 = (CHAR)(((VendorRev >> 10) & 0x1F) + '@');
    CHAR c2 = (CHAR)(((VendorRev >> 5)  & 0x1F) + '@');
    CHAR c3 = (CHAR)(((VendorRev >> 0)  & 0x1F) + '@');

    /* Got 4 digit hex */
    ULONG Product = (EisaId >> 16);
    UCHAR ProdLow = (UCHAR)(Product & 0xFF);
    UCHAR ProdHigh = (UCHAR)(Product >> 8);

    /* Format to AAA0000 */
    RtlStringCbPrintfA(OutString, BufferLength, "%c%c%c%02X%02X", c1, c2, c3, ProdLow, ProdHigh);
}

VEASTATUS
VEAPI
VOCPIEvaluateHardwareId(
    IN PVOCPI_NAMESPACE_NODE DeviceNode,
    IN PCHAR TargetName,
    OUT PCHAR OutBuffer,
    IN ULONG BufferLength
)
{
    if (!DeviceNode || !TargetName || !OutBuffer || BufferLength == 0) 
        return STATUS_INVALID_PARAMETER;
    
    RtlZeroMemory(OutBuffer, BufferLength);

    PVOCPI_NAMESPACE_NODE TargetNode = VOCPIGetChildNode(DeviceNode, TargetName);
    if (!TargetNode) return STATUS_OBJECT_NAME_NOT_FOUND;

    if (TargetNode->Object.Type == VocpiTypeString && TargetNode->Object.Value.String.String)
    {
        /* Batasi panjang copy sesuai dengan panjang asli string ATAU kapasitas buffer */
        ULONG CopyLen = TargetNode->Object.Value.String.Length;
        if (CopyLen >= BufferLength)
        {
            CopyLen = BufferLength - 1;
        }

        RtlCopyMemory(OutBuffer, TargetNode->Object.Value.String.String, CopyLen);
        OutBuffer[CopyLen] = '\0';
        return STATUS_SUCCESS;
    }
    else if (TargetNode->Object.Type == VocpiTypeInteger)
    {
        /* Jika integer, cek apakah kita parsing _UID atau _HID */
        if (RtlCompareMemory(TargetName, "_UID", 4) == 4)
        {
            /* UID berupa angka biasa, ubah ke string */
            RtlStringCbPrintfA(OutBuffer, BufferLength, "%lu", (ULONG)TargetNode->Object.Value.Integer);
        }
        else
        {
            /* _HID atau _CID di-decode sebagai EISA ID */
            VOCPIDecodeEisaId((ULONG)TargetNode->Object.Value.Integer, OutBuffer, BufferLength);
        }
        return STATUS_SUCCESS;
    }
    else if (TargetNode->NodeType == VocpiNodeMethod)
    {
        /* TODO: Panggil AML Interpreter / VOCPIInitExecutionContext untuk Method */
        DPRINT("[VOCPI] %s is a Method! Stack execution needed.\n", TargetName);
        return STATUS_NOT_SUPPORTED;
    }

    return STATUS_UNSUCCESSFUL;
}
