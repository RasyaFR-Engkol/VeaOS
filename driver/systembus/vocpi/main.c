/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : main.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : VOCPI Entry Point
 ----- Effective since 09-2026 til FOREVER ------------------------------- */

#include <veaddk.h>
#include "Include/vocpifunc.h"
#include "tag.h"

/* Revision History ------------------------------------------------------
 * DATE       : 22-09-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file main.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
VOCPIStartDevice(
    IN PDEVICE_OBJECT Fdo
)
{
    PVOCPI_DEVICE_EXTENSION DeviceExtension = (PVOCPI_DEVICE_EXTENSION)Fdo->DeviceExtension;
    PACPI_HEADER DSDT, FADT;
    ACPI_GET_FADT GetFadt;
    ACPI_GET_DSDT GetDsdt;
    VEASTATUS Status;

    Status = VOCPIInitNamespace();

    /* Assign every function */
    DPRINT("GETDSDT\n");
    GetDsdt = DeviceExtension->AcpiInterface->GetDsdt;
    DPRINT("GETFADT\n");
    GetFadt = DeviceExtension->AcpiInterface->GetFadt;
    DPRINT("DONE\n");

    /* Get ACPI_HEADER */
    DPRINT("TRY GETDSDT\n");
    DSDT = GetDsdt(NULL);
    DPRINT("DONE\n");

    /* Check DSDT first */
    if (DSDT)
    {
        PACPI_DSDT_TABLE DsdtTable = (PACPI_DSDT_TABLE)DSDT;
        ULONG AmlLength;
        PUCHAR AmlByte;

        AmlLength = (DsdtTable->AcpiHeader.Length - sizeof(ACPI_HEADER));
        AmlByte = (PUCHAR)DsdtTable->AMLData;

        Status = VOCPIRegisterAmlByte(
            AmlByte,
            AmlLength
        );
        
        if (!VEA_SUCCESS(Status))
        {
            return Status;
        }

        Status = VOCPIStateMachine();
        if (!VEA_SUCCESS(Status))
        {
            return Status;
        }

        VOCPIDumpNamespace(VOCPIRootNode, 5);
    }
    else
    {
        /* No AML? but.. */
        KsBugCheck(PC_ACPI_UNAVAILABLE);
    }

    return Status;
}

VEASTATUS
VEAPI
VOCPIRequestInterface(
    IN PDEVICE_OBJECT Fdo
)
{
    POIP OIP;
    POBJECT_HEADER ObjectHeader = OBJECT_TO_OBJECT_HEADER(Fdo);
    PVOCPI_DEVICE_EXTENSION Extension = (PVOCPI_DEVICE_EXTENSION)Fdo->DeviceExtension;
    ULONG StackCount = ObjectHeader->StackDepth;
    PVEA_ACPI_INTERFACE AcpiInterface;
    POIP_STACK OipStack;
    VEASTATUS Status;

    /* Allocate OIP */
    OIP = ObioAllocateOip(StackCount);
    if (!OIP)
    {
        return STATUS_INSUFFICIENT_MEMORY;
    }

    /* Allocate AcpiInterface */
    AcpiInterface = (PVEA_ACPI_INTERFACE)UlAllocatePoolZero(
        NonPagedPool,
        sizeof(VEA_ACPI_INTERFACE),
        'VCPI'
    );
    if (!AcpiInterface) return STATUS_INSUFFICIENT_MEMORY;

    AcpiInterface->Size = sizeof(VEA_ACPI_INTERFACE);
    AcpiInterface->Version = 10;

    /* Set STACK */
    OipStack = ObioGetNextOipStackLocation(OIP);
    OipStack->MajorFunction = OIP_MJ_PNP;
    OipStack->MinorFunction = OIP_MN_QUERY_INTERFACE;

    OipStack->Parameters.QueryInterface.InterfaceType = &VEA_ACPI_INTERFACE_ID;
    OipStack->Parameters.QueryInterface.InterfaceSpecificData = (PVOID)AcpiInterface;
    OipStack->Parameters.QueryInterface.Size = sizeof(VEA_ACPI_INTERFACE);
    OipStack->Parameters.QueryInterface.Version = 10;
    
    Status = ObioGoInterruptObject(
        Extension->LowerDevice,
        OIP
    );

    if (VEA_SUCCESS(Status))
    {
        DPRINT("[VOCPI] Successfully acquired ACPI Interface from PDO!\n");
        // Simpan function pointer dari AcpiInterface ke Extension FDO lu
        Extension->AcpiInterface = AcpiInterface;
    }
    else
    {
        DPRINT("[VOCPI] ERR: QueryInterface failed with Status = 0x%x\n", Status);
    }

    ObioFreeOip(OIP);

    return Status;
}

VEASTATUS
VEAPI
VOCPIProcessMnAddDevice(
    IN PVOID Object,
    IN POIP OIP,
    IN POIP_STACK OipStack
)
{
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
VOCPIAddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT DeviceObject
)
{
    VEASTATUS Status;
    PDEVICE_OBJECT Fdo = NULL;
    PDEVICE_OBJECT Pdo = DeviceObject; // Langsung pakai DeviceObject!
    PVOCPI_DEVICE_EXTENSION Extension = NULL;
    PVOID AttachedDevice = NULL;

    if (!DriverObject || !Pdo)
    {
        DPRINT("[VOCPI] ERR: AddDevice failed, DriverObject or PDO is NULL!\n");
        return STATUS_INVALID_PARAMETER;
    }

    DPRINT("[VOCPI] AddDevice called for PDO: 0x%p\n", Pdo);

    /* 1. Buat FDO (Functional Device Object) untuk VOCPI */
    Status = ObioCreateNewDevice(
        DriverObject, /* FIX: Pakai DriverObject, bukan 'Object' */
        sizeof(VOCPI_DEVICE_EXTENSION),
        "VOCPI",
        FILE_DEVICE_BUS_EXTENDER,
        &Fdo
    );

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[VOCPI] ERR: Failed to create FDO (0x%x)\n", Status);
        return Status;
    }

    /* 2. Setup Device Extension */
    Extension = (PVOCPI_DEVICE_EXTENSION)Fdo->DeviceExtension;
    RtlZeroMemory(Extension, sizeof(VOCPI_DEVICE_EXTENSION));
    Extension->Pdo = Pdo;
    Extension->IsStarted = FALSE;

    /* 3. Attach FDO ke atas PDO */
    AttachedDevice = ObioAttachObject(Fdo, Pdo);
    if (AttachedDevice == NULL)
    {
        DPRINT("[VOCPI] ERR: Failed to attach FDO to PDO Stack\n");
        ObioDeleteDevice(Fdo);
        return STATUS_UNSUCCESSFUL;
    }

    Extension->LowerDevice = (PDEVICE_OBJECT)AttachedDevice;

    /* 4. Attach Interrupt Handler untuk VOCPI (SCI / ACPI Event) */
    ObioAttachInterruptToThisObject(Fdo, VOCPIObInterruptHandler);

    /* 5. Request ACPI Interface (jika butuh) */
    Status = VOCPIRequestInterface(Fdo);
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[VOCPI] ERR: VOCPIRequestInterface failed: 0x%x\n", Status);
        goto cleanup;
    }

    /* 6. Tandai FDO siap (Clear Flag Initializing) */
    Fdo->Flags &= ~DO_DEVICE_INITIALIZING;

    DPRINT("[VOCPI] AddDevice Success. FDO: 0x%p attached to PDO: 0x%p\n", Fdo, Pdo);

    return STATUS_SUCCESS;

cleanup:
    /* Rollback jika ada step yang gagal setelah FDO di-attach */
    if (Extension && Extension->LowerDevice)
    {
        // TODO: (note: on another file, there is line of code like this)
        // ObioDetachObject(Fdo, Extension->LowerDevice);
    }
    if (Fdo)
    {
        ObioDeleteDevice(Fdo);
    }

    return Status;
}

VEASTATUS
VEAPI
VOCPIHandleMnStartDevice(
    IN PVOID Object,
    IN POIP Oip,
    IN POIP_STACK OipStack
)
{
    PDEVICE_OBJECT Fdo = (PDEVICE_OBJECT)Object;
    PVOCPI_DEVICE_EXTENSION Extension = (PVOCPI_DEVICE_EXTENSION)Fdo->DeviceExtension;
    VEASTATUS Status;

    ObioSkipCurrentOipStackLocation(Oip);
    Status = ObioGoInterruptObject(Extension->LowerDevice, Oip);

    if (VEA_SUCCESS(Status))
    {
        // 2. Jika driver di bawah sukses START, baru VOCPI jalankan Start Device milik sendiri
        DPRINT("[VOCPI] Lower device started successfully. Starting VOCPI...\n");
        
        Status = VOCPIStartDevice(Fdo);
        if (VEA_SUCCESS(Status))
        {
            Extension->IsStarted = TRUE;
        }
    }

    // 3. Complete OIP di sini jika VOCPI adalah penanggung jawab akhir
    Oip->StatusInformation.Status = Status;
    ObioCompleteRequest(Oip);

    return Status;
}

VEASTATUS
VEAPI
VOCPIHandleMnQueryDeviceRelations(
    IN PVOID Object,
    IN POIP Oip,
    IN POIP_STACK OipStack
)
{
    PDEVICE_OBJECT Fdo = (PDEVICE_OBJECT)Object;
    ULONG Signature;
    PVOCPI_DEVICE_EXTENSION Extension;
    VEASTATUS Status;

    if (!Fdo || !Fdo->DeviceExtension)
    {
        Oip->StatusInformation.Status = STATUS_INVALID_PARAMETER;
        Oip->StatusInformation.Information = 0;
        return STATUS_INVALID_PARAMETER;
    }

    Signature = *(PULONG)Fdo->DeviceExtension;
    if (Signature == VOCPI_PDO_SIGNATURE)
    {
        Oip->StatusInformation.Status = STATUS_SUCCESS;
        Oip->StatusInformation.Information = 0;
        return STATUS_SUCCESS;
    }

    Extension = (PVOCPI_DEVICE_EXTENSION)Fdo->DeviceExtension;

    /* Only handle BusRelations */
    if (OipStack->Parameters.QueryDeviceRelation.Type == BusRelations)
    {
        DPRINT("[VOCPI] Handling BusRelations Query...\n");

        // 1. Lempar OIP ke LowerDevice dulu untuk ambil child dari HCT/PDO bawah
        ObioSkipCurrentOipStackLocation(Oip);
        Status = ObioGoInterruptObject(Extension->LowerDevice, Oip);
        if (!VEA_SUCCESS(Status))
        {
            Oip->StatusInformation.Status = Status;
            Oip->StatusInformation.Information = 0;
            return Status;
        }

        DPRINT("VOCPI A\n");

        // Biarpun LowerDevice gagal, kita tetap lanjut cek apakah ACPI punya child
        PDEVICE_RELATIONS HCTRelations = (PDEVICE_RELATIONS)(ULONG_PTR)Oip->StatusInformation.Information;
        PDEVICE_RELATIONS ACPIRelations = NULL;
        PDEVICE_RELATIONS CombinedRelations = NULL;
        ULONG HctCount = HCTRelations ? HCTRelations->Count : 0;
        ULONG AcpiCount = 0;
        ULONG AllRelationsCount = 0;

        DPRINT("VOCPI B\n");

        // 2. Ambil PDO child milik VOCPI (ACPI)
        Status = VOCPIQueryDeviceRelations(Fdo, &ACPIRelations);
        if (VEA_SUCCESS(Status) && ACPIRelations)
        {
            AcpiCount = ACPIRelations->Count;
        }

        DPRINT("VOCPI C\n");

        AllRelationsCount = HctCount + AcpiCount;

        // 3. Jika ada device (baik dari HCT maupun ACPI)
        if (AllRelationsCount > 0)
        {
            // Gunakan PDEVICE_OBJECT (pointer), bukan DEVICE_OBJECT (struct)!
            ULONG AllocationSize = sizeof(DEVICE_RELATIONS) + (sizeof(PDEVICE_OBJECT) * (AllRelationsCount - 1));
            
            CombinedRelations = (PDEVICE_RELATIONS)UlAllocatePoolZero(
                NonPagedPool,
                AllocationSize,
                TAG_OBIO
            );

            if (CombinedRelations)
            {
                // SET COUNT-NYA!
                CombinedRelations->Count = AllRelationsCount;

                ULONG CurrentIndex = 0;

                // Copy PDO dari HCT
                if (HctCount > 0)
                {
                    RtlCopyMemory(
                        &CombinedRelations->Objects[CurrentIndex], 
                        HCTRelations->Objects, 
                        sizeof(PDEVICE_OBJECT) * HctCount
                    );
                    CurrentIndex += HctCount;
                }

                // Copy PDO dari ACPI
                if (AcpiCount > 0)
                {
                    RtlCopyMemory(
                        &CombinedRelations->Objects[CurrentIndex], 
                        ACPIRelations->Objects, 
                        sizeof(PDEVICE_OBJECT) * AcpiCount
                    );
                }

                // Update OIP Information dengan buffer gabungan yang baru
                Oip->StatusInformation.Information = (ULONG_PTR)CombinedRelations;
            }

            // 4. CLEANUP MEMORY LEAK!
            // Bebaskan buffer lama karena datanya sudah disalin ke CombinedRelations
            if (HCTRelations)
            {
                UlFreePoolWithTag(HCTRelations, TAG_OBIO);
            }
            if (ACPIRelations)
            {
                UlFreePoolWithTag(ACPIRelations, TAG_OBIO);
            }
        }

        Oip->StatusInformation.Status = STATUS_SUCCESS;
        ObioCompleteRequest(Oip);
        return STATUS_SUCCESS;
    }

    /* If it doesn't, pass down */
    ObioSkipCurrentOipStackLocation(Oip);
    return ObioGoInterruptObject(Extension->LowerDevice, Oip);
}

VEASTATUS
VEAPI
VOCPIHandleMnQueryId(
    IN PDEVICE_OBJECT DeviceObject,
    IN POIP Oip,
    IN POIP_STACK OipStack,
    IN BUS_QUERY_ID_TYPE QueryIdType
)
{
    PVOCPI_PDO_EXTENSION PdoExt;
    CHAR LocalBuffer[128];
    PCHAR ReturnedBuffer = NULL;
    SIZE_T BufferLen = 0;
    VEASTATUS Status = STATUS_NOT_SUPPORTED;

    PdoExt = (PVOCPI_PDO_EXTENSION)DeviceObject->DeviceExtension;

    if (!PdoExt || !PdoExt->AcpiNode)
        return STATUS_NOT_SUPPORTED;
    
    RtlZeroMemory(LocalBuffer, sizeof(LocalBuffer));

    switch (QueryIdType)
    {
        case BusQueryDeviceID:
        {
            /* Meminta Device ID (Format: ACPI/PNP0303) */
            Status = VOCPIQueryDeviceId(PdoExt->AcpiNode, LocalBuffer, sizeof(LocalBuffer));
            break;
        }

        case BusQueryInstanceID:
        {
            /* Meminta Instance ID / _UID (Format: 0, 1, atau String UID) */
            Status = VOCPIQueryInstanceId(PdoExt->AcpiNode, LocalBuffer, sizeof(LocalBuffer));
            break;
        }

        default:
            /* Untuk HardwareIDs / CompatibleIDs yang belum dihandle */
            return STATUS_NOT_SUPPORTED;
    }

    if (VEA_SUCCESS(Status))
    {
        /* Hitung panjang string + 1 untuk Null-Terminator '\0' */
        BufferLen = RtlRawStringLength(LocalBuffer) + 1;

        /* Alokasikan dari PagedPool. PnP Manager yang akan meng-free pointer ini */
        ReturnedBuffer = (PCHAR)UlAllocatePoolZero(PagedPool, BufferLen, 'IdcV');
        if (!ReturnedBuffer)
        {
            Oip->StatusInformation.Information = 0;
            return STATUS_INSUFFICIENT_MEMORY;
        }

        /* Salin string dari buffer lokal ke buffer heap */
        RtlCopyMemory(ReturnedBuffer, LocalBuffer, BufferLen);

        /* Berikan pointer buffer ke PnP Manager */
        Oip->StatusInformation.Information = (ULONG_PTR)ReturnedBuffer;
        return STATUS_SUCCESS;
    }

    return Status;
}

VEASTATUS
VEAPI
VOCPIHandleMnQueryResource(
    IN PVOID Object,
    IN POIP Oip,
    IN POIP_STACK OipStack
)
{
    PDEVICE_OBJECT DeviceObject = (PDEVICE_OBJECT)Object;
    PVOCPI_PDO_EXTENSION PdoExtension = (PVOCPI_PDO_EXTENSION)DeviceObject->DeviceExtension;

    if (PdoExtension->Signature != VOCPI_PDO_SIGNATURE)
    {
        Oip->StatusInformation.Information = STATUS_NOT_SUPPORTED;
        return STATUS_NOT_SUPPORTED;
    }

    

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
VOCPIHandleMnQueryResourceList(
    IN PVOID Object,
    IN POIP Oip,
    IN POIP_STACK OipStack
)
{
    PDEVICE_OBJECT DeviceObject = (PDEVICE_OBJECT)Object;
    PVOCPI_PDO_EXTENSION PdoExtension = (PVOCPI_PDO_EXTENSION)DeviceObject->DeviceExtension;
    POBIO_REQUIRED_RESOURCES_LIST_EX RequiredResources = NULL;
    VEASTATUS Status;

    if (PdoExtension->Signature != VOCPI_PDO_SIGNATURE)
    {
        Oip->StatusInformation.Information = STATUS_NOT_SUPPORTED;
        return STATUS_NOT_SUPPORTED;
    }

    Status = VOCPIQueryNodeResourcesRequirement(
        PdoExtension->AcpiNode,
        &RequiredResources
    );

    if (!VEA_SUCCESS(Status))
    {
        Oip->StatusInformation.Status = STATUS_QUERY_FAILED;
        Oip->StatusInformation.Information = 0;
        return STATUS_QUERY_FAILED;
    }

    Oip->StatusInformation.Status = STATUS_SUCCESS;
    Oip->StatusInformation.Information = (ULONG_PTR)RequiredResources;

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
VOCPIHandleMjPnp(
    IN PVOID Object,
    IN POIP Oip
)
{
    PDEVICE_OBJECT Fdo = (PDEVICE_OBJECT)Object;
    PVOCPI_DEVICE_EXTENSION Extension = (PVOCPI_DEVICE_EXTENSION)Fdo->DeviceExtension;
    POIP_STACK CurrentStack = ObioGetCurrentOipStackLocation(Oip);
    
    switch (CurrentStack->MinorFunction)
    {
        case OIP_MN_ADD_DEVICE:
        {
            return VOCPIProcessMnAddDevice(Object, Oip, CurrentStack);
            break;
        }

        case OIP_MN_START_DEVICE:
        {
            return VOCPIHandleMnStartDevice(Object, Oip, CurrentStack);
            break;
        }

        case OIP_MN_QUERY_DEVICE_RELATIONS:
        {
            return VOCPIHandleMnQueryDeviceRelations(Object, Oip, CurrentStack);
            break;
        }

        case OIP_MN_QUERY_ID:
        {
            return VOCPIHandleMnQueryId(
                (PDEVICE_OBJECT)Object,
                Oip,
                CurrentStack,
                CurrentStack->Parameters.QueryId.IdType
            );
            break;
        }

        case OIP_MN_QUERY_RESOURCE_REQUIREMENTS:
        {
            return VOCPIHandleMnQueryResourceList(
                Object,
                Oip,
                CurrentStack
            );
        }

        case OIP_MN_QUERY_RESOURCES:
        {
            return VOCPIHandleMnQueryResource(
                Object,
                Oip,
                CurrentStack
            );
        }

        default:
        {
            DPRINT("[VOCPI] Unhandled PnP Minor 0x%x -> Passing down\n", CurrentStack->MinorFunction);
            ObioSkipCurrentOipStackLocation(Oip);
            return ObioGoInterruptObject(Extension->LowerDevice, Oip);
        }
    }

    return STATUS_NOT_SUPPORTED;
}

VEASTATUS
VEAPI
VOCPIObInterruptHandler(IN PVOID Object, IN POIP Oip)
{
    POIP_STACK CurrentStack = ObioGetCurrentOipStackLocation(Oip);

    switch (CurrentStack->MajorFunction)
    {
        case OIP_MJ_PNP:
            return VOCPIHandleMjPnp(Object, Oip);

        case OIP_MJ_INTERNAL_DEVICE_CTL:
            break;
    }

    return STATUS_NOT_SUPPORTED;
}

VEASTATUS
VEAPI
DriverEntry(IN PDRIVER_OBJECT DriverObject)
{
    DPRINT("Successfully added VOCPI.ko\n");

    DriverObject->AddDeviceRoutine = VOCPIAddDevice;

    return ObioAttachInterruptToThisObject((PVOID)DriverObject, VOCPIObInterruptHandler);
}