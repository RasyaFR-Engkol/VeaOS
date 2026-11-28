/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : kpccreate.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Creating Port for KPC
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 10-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file kpccreate.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
KpcpCreatePort(
    OUT PHANDLE PortHandle,
    IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL,
    IN ULONG MaxConnectionInfoLength,
    IN ULONG MaxMessageLength,
    IN KPC_PORT_MODE Mode
)
//
// Routine Description:
// Creating default and unlistened Kpc Port based by requested mode
// 
// Parameter:
// PHANDLE PortHandle
//      Argument to pass PortHandle
// 
// POBJECT_ATTRIBUTES ObjectAttributes
//      Pointer to Object Attributes
// 
// ULONG MaxConnectionInfoLength
//      Determine Maximum Connection Info Length
// 
// ULONG MaxMessageLength
//      Determine Maximum message length
// 
// KPC_PORT_MODE Mode
//      Requested mode
// 
// return VEASTATUS:
// (Failed returning means no port created)
//
{
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();
    PKPC_PORT_OBJECT Port = NULL;
    HANDLE hPort = NULL;
    VEASTATUS Status;

    // TODO:
    // Add Support for Flexible Length message with allocating memory

    if (MaxMessageLength > 4096)
    {
        MaxMessageLength = 4096;
    }

    if (MaxConnectionInfoLength > 256)
    {
        MaxConnectionInfoLength = 256;
    }

    //
    // Initializing the object. The cruciest part is Semaphore atm. Insert them
    // into the object. Make the type always Unconnected (Listen)
    //

    Status = ObCreateObject(
        PreviousMode,
        KpcPortObjectType,
        ObjectAttributes,
        PreviousMode,
        NULL,
        sizeof(KPC_PORT_OBJECT),
        0,
        0,
        (PVOID*)&Port
    );

    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    RtlZeroMemory(Port, sizeof(KPC_PORT_OBJECT));

    Port->Type = KpcUnconnectedServerPort;
    Port->Mode = Mode;
    Port->CreatorProcess = (PEPROCESS)PtGetCurrentProcess();
    Port->ConnectedPort = NULL;
    Port->ServerProcessPort = NULL;

    KsInitializeSpinlock(&Port->QueueLock);
    InitializeListHead(&Port->MessageQueueHead);
    InitializeListHead(&Port->LpcReplyChain);
    KsInitializeSemaphore(&Port->MesssageSemaphore, 0, 0x7FFFFFFF);

    Port->MaxMessageLength = MaxMessageLength;
    Port->MaxConnectionInfoLength = MaxConnectionInfoLength;

    Status = ObInsertObject(
        Port,
        NULL,
        PORT_ALL_ACCESS,
        0,
        NULL,
        &hPort
    );

    if (VEA_SUCCESS(Status))
    {
        *PortHandle = hPort;
    }

    return Status;
}

VEASTATUS
VEAPI
VeaCreatePort(
    OUT PHANDLE PortHandle,
    IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL,
    IN ULONG MaxConnectionInfoLength,
    IN ULONG MaxMessageLength,
    IN KPC_PORT_MODE Mode
)
//
// Routine Description:
// Wrapper routine to call KpcpCreatePort
// 
// Parameter:
// PHANDLE PortHandle
//      Argument to pass PortHandle
// 
// POBJECT_ATTRIBUTES ObjectAttributes
//      Pointer to Object Attributes
// 
// ULONG MaxConnectionInfoLength
//      Determine Maximum Connection Info Length
// 
// ULONG MaxMessageLength
//      Determine Maximum message length
// 
// KPC_PORT_MODE Mode
//      Requested mode
// 
// return VEASTATUS:
// (Failed returning means no port created)
//
{
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();
    HANDLE LocalPortHandle = NULL;
    OBJECT_ATTRIBUTES LocalObjectAttributes;
    POBJECT_ATTRIBUTES CapturedObjectAttributes = NULL;
    VEASTATUS Status;

    if (PreviousMode == UserMode)
    {
        if (!PortHandle || (ULONG_PTR)PortHandle >= 0x80000000)
        {
            return STATUS_ACCESS_VIOLATION;
        }

        _VEA_TRY
        {
            *(volatile HANDLE*)PortHandle = *(volatile HANDLE*)PortHandle;

            if (ObjectAttributes)
            {
                if ((ULONG_PTR)ObjectAttributes >= (ULONG)MM_HIGHEST_USER_ADDRESS)
                {
                    return STATUS_ACCESS_VIOLATION;
                }
                RtlCopyMemory(&LocalObjectAttributes, ObjectAttributes, sizeof(OBJECT_ATTRIBUTES));
                CapturedObjectAttributes = &LocalObjectAttributes;
            }
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            return STATUS_ACCESS_VIOLATION;
        }
        _VEA_END;
    }
    else
    {
        CapturedObjectAttributes = ObjectAttributes;
    }

    Status = KpcpCreatePort(
        &LocalPortHandle,
        CapturedObjectAttributes,
        MaxConnectionInfoLength,
        MaxMessageLength,
        Mode
    );

    if (VEA_SUCCESS(Status))
    {
        if (PreviousMode == UserMode)
        {
            _VEA_TRY
            {
                *PortHandle = LocalPortHandle;
            }
            _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
            {
                VzClose(LocalPortHandle);
                Status = STATUS_ACCESS_VIOLATION;
            }
            _VEA_END;
        }
        else
        {
            *PortHandle = LocalPortHandle;
        }
    }

    return Status;
}


/* EOF */