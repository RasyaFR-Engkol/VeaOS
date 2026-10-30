/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : bytecode.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Bytecode
 ----- Effective since 09-2026 til FOREVER ------------------------------- */

#include <veaddk.h>
#include "Include/vocpifunc.h"

/* Revision History ------------------------------------------------------
 * DATE       : 23-09-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file bytecode.c
 * --------------------------------------------------------------------- */

PUCHAR VOCPIAmlByte;
ULONG VOCPIPointerAmlByte;
ULONG VOCPIAmlLength;
static BOOLEAN VOCPIParseAttempted;
static VEASTATUS VOCPIParseStatus;

PUCHAR
VEAPI
VOCPIGetAndPushNextAmlByte()
{
    if (!VOCPIAmlByte || VOCPIPointerAmlByte >= VOCPIAmlLength) return NULL;

    return &VOCPIAmlByte[VOCPIPointerAmlByte++];
}

VEASTATUS
VEAPI
VOCPIRegisterAmlByte(
    IN PUCHAR AmlByte,
    IN ULONG Length
)
{
    if (!AmlByte) return STATUS_INVALID_PARAMETER;

    VOCPIAmlByte = AmlByte;
    VOCPIAmlLength = Length;
    VOCPIPointerAmlByte = 0;
    VOCPIParseAttempted = FALSE;
    VOCPIParseStatus = STATUS_SUCCESS;

    return STATUS_SUCCESS;
}

VOID
VEAPI
VOCPIDumpAmlBytecode(
    IN PUCHAR AmlByte,
    IN ULONG Length
)
{
    if (!AmlByte || Length == 0) return;

    DPRINT("DSDT AML dump: %u bytes\n", Length);

    for (ULONG Offset = 0; Offset < Length; Offset += 16)
    {
        ULONG LineLength = Length - Offset;
        if (LineLength > 16) LineLength = 16;

        KdPrintf("AML %04x:", Offset);
        for (ULONG Index = 0; Index < LineLength; Index++)
        {
            KdPrintf(" %02x", AmlByte[Offset + Index]);
        }
        KdPrintf("\n");
    }
}

BOOLEAN
VEAPI
VOCPIProcessByteCode(
    IN UCHAR ByteCode,
    OUT PVEASTATUS VeaStatus)
{
    VEASTATUS Status;

    /* Switch byte code */
    switch (ByteCode)
    {
        case AML_ZERO_OP:
        case AML_ONE_OP:
            /* Constant Opcode: NOP / Push constant ke execution stack */
            break;
        case AML_NAME_OP:
            Status = VOCPIProcessNameOp();
            if (!VEA_SUCCESS(Status))
            {
                if (VeaStatus) *VeaStatus = Status;
                return FALSE;
            }
            break;

        case AML_SCOPE_OP:
            Status = VOCPIProcessScopeOp();
            if (VEA_SUCCESS(Status))
            {
                break;
            }
            else
            {
                if (VeaStatus)
                {
                    *VeaStatus = Status;
                }

                return FALSE;
            }
            break;

        case AML_METHOD_OP:
            Status = VOCPIProcessMethodOp();
            if (!VEA_SUCCESS(Status))
            {
                if (VeaStatus) *VeaStatus = Status;
                return FALSE;
            }
            break;

        case AML_EXT_OP_PREFIX:
        {
            PUCHAR ExtendedBytePtr;
            UCHAR ExtendedOpCode;

            ExtendedBytePtr = VOCPIGetAndPushNextAmlByte();
            if (!ExtendedBytePtr) break;

            ExtendedOpCode = (UCHAR)*ExtendedBytePtr;

            switch (ExtendedOpCode)
            {
                case OpRegionOp:
                    Status = VOCPIProcessOpRegionOp();
                    if (!VEA_SUCCESS(Status))
                    {
                        if (VeaStatus) *VeaStatus = Status;
                        return FALSE;
                    }
                    break;

                case DataRegionOp:
                    Status = VOCPIProcessDataRegionOp();
                    if (!VEA_SUCCESS(Status))
                    {
                        if (VeaStatus) *VeaStatus = Status;
                        return FALSE;
                    }
                    break;

                case FieldOp:
                    Status = VOCPIProcessFieldOp();
                    if (!VEA_SUCCESS(Status))
                    {
                        if (VeaStatus) *VeaStatus = Status;
                        return FALSE;
                    }
                    break;

                case DeviceOp:
                    Status = VOCPIProcessDeviceOp();
                    if (!VEA_SUCCESS(Status))
                    {
                        if (VeaStatus) *VeaStatus = Status;
                        return FALSE;
                    }
                    break;

                case ProcessorOp:
                case PowerResourceOp:
                case ThermalZoneOp:
                case IndexFieldOp:
                case BankFieldOp:
                    Status = VOCPIProcessPackageBodyOp();
                    if (!VEA_SUCCESS(Status))
                    {
                        if (VeaStatus) *VeaStatus = Status;
                        return FALSE;
                    }
                    break;

                default:
                    break;
            }
            break;
        }

        case AML_RETURN_OP:
            break;

        case AML_IF_OP:
        case AML_WHILE_OP:
            Status = VOCPIProcessPackageBodyOp();
            if (!VEA_SUCCESS(Status))
            {
                if (VeaStatus) *VeaStatus = Status;
                return FALSE;
            }
            break;

        default:
            break;
    }

    if (VeaStatus)
    {
        *VeaStatus = STATUS_SUCCESS;
    }

    return TRUE;
}

VEASTATUS
VEAPI
VOCPIStateMachine()
{
    BOOLEAN DoProcess = TRUE;
    VEASTATUS Status = STATUS_SUCCESS;

    DPRINT("StateMachine\n");

    if (!VOCPIAmlByte || VOCPIAmlLength == 0)
    {
        DPRINT("[VOCPI] No AML bytecode registered\n");
        return STATUS_UNSUCCESSFUL;
    }

    if (VOCPIParseAttempted)
    {
        DPRINT("[VOCPI] AML parse already attempted: 0x%x\n", VOCPIParseStatus);
        return VOCPIParseStatus;
    }

    VOCPIParseAttempted = TRUE;

    while (DoProcess)
    {
        PUCHAR ByteCodePtr = VOCPIGetAndPushNextAmlByte();

        /* Check NULL before dereference pointer */
        if (!ByteCodePtr)
        {
            /* EOF AML Stream */
            break; 
        }

        ULONG OpcodeOffset = VOCPIPointerAmlByte - 1;
        UCHAR Opcode = (UCHAR)*ByteCodePtr;
        DoProcess = VOCPIProcessByteCode(Opcode, &Status);

        if (!VEA_SUCCESS(Status))
        {
            DPRINT("[VOCPI] Parse failed at AML offset 0x%x opcode 0x%x status 0x%x\n",
                   OpcodeOffset, Opcode, Status);
            break;
        }
    }

    VOCPIParseStatus = Status;
    return VOCPIParseStatus;
}

VEASTATUS
VEAPI
VOCPIInitExecutionContext(
    OUT PVOCPI_EXECUTION_CONTEXT Context,
    IN PCHAR MethodAml,
    IN ULONG MethodLength,
    IN PVOCPI_OBJECT Args OPTIONAL,
    IN UCHAR ArgCount
)
{
    if (!Context || !MethodAml) return STATUS_INVALID_PARAMETER;

    RtlZeroMemory(Context, sizeof(VOCPI_EXECUTION_CONTEXT));

    Context->MethodAmlByte = MethodAml;
    Context->MethodLength = MethodLength;
    Context->MethodPointer = 0;
    Context->EvaluationStack.Top = 0;

    /* Copy argumen masuk ke slot Arg0 - Arg6 */
    if (Args && ArgCount > 0)
    {
        for (UCHAR i = 0; i < ArgCount && i < 7; i++)
        {
            Context->Arg[i] = Args[i];
        }
    }

    return STATUS_SUCCESS;
}
