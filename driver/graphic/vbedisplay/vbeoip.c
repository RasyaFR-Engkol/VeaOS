/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : vbeoip.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : VBE OIP
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include "vbe.h"

/* Revision History ------------------------------------------------------
 * DATE       : 21-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file vbeoip.c
 * --------------------------------------------------------------------- */

static
VOID
VbeFillModeInfo(
    IN PVBE_DEVICE_EXTENSION DevExt,
    OUT PVIDEO_MODE_INFORMATION ModeInfo
)
{
    RtlZeroMemory(ModeInfo, sizeof(VIDEO_MODE_INFORMATION));

    ModeInfo->Length          = sizeof(VIDEO_MODE_INFORMATION);
    ModeInfo->ModeIndex       = 0;
    ModeInfo->VisScreenWidth  = DevExt->Width;
    ModeInfo->VisScreenHeight = DevExt->Height;
    ModeInfo->ScreenStride    = DevExt->Pitch;
    ModeInfo->NumberOfPlanes  = 1;
    ModeInfo->BitsPerPlane    = DevExt->BitsPerPixel;
    ModeInfo->Frequency       = 60;

    /* 32-bpp format ARGB/XRGB standard of VBE LFB */
    ModeInfo->NumberRedBits   = 8;
    ModeInfo->NumberGreenBits = 8;
    ModeInfo->NumberBlueBits  = 8;
    ModeInfo->RedMask         = 0x00FF0000;
    ModeInfo->GreenMask       = 0x0000FF00;
    ModeInfo->BlueMask        = 0x000000FF;
}

static
VEASTATUS
VbeHandleDeviceControl(
    IN PVBE_DEVICE_EXTENSION DevExt,
    IN POIP Oip,
    IN POIP_STACK Stack
)
{
    ULONG IoControlCode = Stack->Parameters.DeviceControl.IoControlCode;
    ULONG OutLen        = Stack->Parameters.DeviceControl.OutputBufferLength;
    PVOID SysBuf        = Oip->AssociatedOip.SystemBuffer;
    VEASTATUS Status    = STATUS_NOT_IMPLEMENTED;
    ULONG_PTR Info      = 0;

    switch (IoControlCode)
    {
        case IOCTL_VIDEO_QUERY_NUM_AVAIL_MODES:
        {
            PVIDEO_NUM_MODES NumModes = (PVIDEO_NUM_MODES)SysBuf;
            if (!NumModes || OutLen < sizeof(VIDEO_NUM_MODES))
            {
                Status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            NumModes->NumModes              = 1;
            NumModes->ModeInformationLength = sizeof(VIDEO_MODE_INFORMATION);
            Info   = sizeof(VIDEO_NUM_MODES);
            Status = STATUS_SUCCESS;
            break;
        }

        case IOCTL_VIDEO_QUERY_AVAIL_MODES:
        case IOCTL_VIDEO_QUERY_CURRENT_MODE:
        {
            PVIDEO_MODE_INFORMATION ModeInfo = (PVIDEO_MODE_INFORMATION)SysBuf;
            if (!ModeInfo || OutLen < sizeof(VIDEO_MODE_INFORMATION))
            {
                Status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            VbeFillModeInfo(DevExt, ModeInfo);
            Info   = sizeof(VIDEO_MODE_INFORMATION);
            Status = STATUS_SUCCESS;
            break;
        }

        case IOCTL_VIDEO_SET_CURRENT_MODE:
        {
            Status = STATUS_SUCCESS;
            break;
        }

        case IOCTL_VIDEO_MAP_VIDEO_MEMORY:
        {
            PVIDEO_MEMORY_INFORMATION MemInfo = (PVIDEO_MEMORY_INFORMATION)SysBuf;
            if (!MemInfo || OutLen < sizeof(VIDEO_MEMORY_INFORMATION))
            {
                Status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            RtlZeroMemory(MemInfo, sizeof(VIDEO_MEMORY_INFORMATION));
            MemInfo->VideoRamBase      = DevExt->FrameBufferBase;
            MemInfo->VideoRamLength    = (ULONG)DevExt->FrameBufferSize;
            MemInfo->FrameBufferBase   = DevExt->FrameBufferBase;
            MemInfo->FrameBufferLength = (ULONG)DevExt->FrameBufferSize;

            Info   = sizeof(VIDEO_MEMORY_INFORMATION);
            Status = STATUS_SUCCESS;
            break;
        }

        case IOCTL_VIDEO_UNMAP_VIDEO_MEMORY:
        case IOCTL_VIDEO_RESET_DEVICE:
        {
            Status = STATUS_SUCCESS;
            break;
        }

        default:
        {
            DPRINT("[VBE] Unknown IOCTL: 0x%08X\n", IoControlCode);
            Status = STATUS_INVALID_DEVICE_REQUEST;
            break;
        }
    }

Complete:
    Oip->StatusInformation.Status      = Status;
    Oip->StatusInformation.Information = Info;
    ObioCompleteRequest(Oip);
    return Status;
}

VEASTATUS
VEAPI
VbeObIntHandler(
    IN PVOID Object,
    IN POIP Oip
)
{
    PDEVICE_OBJECT DeviceObject = (PDEVICE_OBJECT)Object;
    PVBE_DEVICE_EXTENSION DevExt = (PVBE_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
    POIP_STACK Stack = ObioGetCurrentOipStackLocation(Oip);

    switch (Stack->MajorFunction)
    {
        case OIP_MJ_CREATE:
        case OIP_MJ_CLOSE:
        {
            Oip->StatusInformation.Status = STATUS_SUCCESS;
            Oip->StatusInformation.Information = 0;
            ObioCompleteRequest(Oip);
            return STATUS_SUCCESS;
        }

        case OIP_MJ_DEVICE_CONTROL:
        {
            return VbeHandleDeviceControl(DevExt, Oip, Stack);
        }

        case OIP_MJ_PNP:
        {
            /* TODO: Handle MinorFunction PnP jika dibutuhkan */
            Oip->StatusInformation.Status = STATUS_NOT_SUPPORTED;
            Oip->StatusInformation.Information = 0;
            ObioCompleteRequest(Oip);
            return STATUS_NOT_SUPPORTED;
        }

        default:
        {
            Oip->StatusInformation.Status = STATUS_NOT_IMPLEMENTED;
            Oip->StatusInformation.Information = 0;
            ObioCompleteRequest(Oip);
            return STATUS_NOT_IMPLEMENTED;
        }
    }
}

VEASTATUS
VEAPI
VbeCreateDevice(
    IN PDRIVER_OBJECT DriverObject
)
{
    VEASTATUS Status;
    PDEVICE_OBJECT Pdo = NULL;
    PDEVICE_OBJECT Fdo = NULL;
    PVBE_DEVICE_EXTENSION DevExt;

    Status = ObioReportDetectedDevice(
        DriverObject,
        "DISPLAY",
        "vbedisplay",
        NULL,
        &Pdo
    );
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    Status = ObioCreateNewDevice(
        DriverObject,
        sizeof(VBE_DEVICE_EXTENSION),
        "/Device/Vbe0",
        FILE_DEVICE_VIDEO,
        &Fdo
    );
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[VBE] Gagal membuat /Device/Vbe0: 0x%08X\n", Status);
        return Status;
    }

    DevExt = (PVBE_DEVICE_EXTENSION)Fdo->DeviceExtension;
    RtlZeroMemory(DevExt, sizeof(VBE_DEVICE_EXTENSION));
    DevExt->Self = Fdo;

    Status = BvGetVideoInfo(
        &DevExt->FrameBufferBase,
        &DevExt->Width,
        &DevExt->Height,
        &DevExt->Pitch,
        &DevExt->BitsPerPixel,
        &DevExt->FrameBufferSize
    );
    if (VEA_SUCCESS(Status))
    {
        DevExt->IsInitialized = TRUE;
        DPRINT("[VBE] BvGetVideoInfo OK: %lux%lu @ %lu bpp (Pitch=%lu, LFB=0x%p)\n",
               DevExt->Width,
               DevExt->Height,
               DevExt->BitsPerPixel,
               DevExt->Pitch,
               DevExt->FrameBufferBase);
    }
    else
    {
        DPRINT("[VBE] Peringatan: BvGetVideoInfo gagal: 0x%08X\n", Status);
    }

    Fdo->Flags |= DO_BUFFERED_IO;
    Fdo->Flags &= ~DO_DEVICE_INITIALIZING;

    ObioAttachObject(Fdo, Pdo);

    Status = ObioRegisterDeviceInterface(
        Pdo,
        &VEA_GUID_DEVINTERFACE_GRAPHIC,
        NULL,
        &DevExt->SymbolicLinkName
    );
    if (VEA_SUCCESS(Status))
    {
        ObioSetDeviceInterfaceState(&DevExt->SymbolicLinkName, TRUE);
        DevExt->InterfaceEnabled = TRUE;
        DPRINT("[VBE] Interface GRAPHIC aktif: %Z\n", &DevExt->SymbolicLinkName);
    }
    else
    {
        DPRINT("[VBE] Gagal register interface GRAPHIC: 0x%08X\n", Status);
    }

    return Status;
}

/* EOF */