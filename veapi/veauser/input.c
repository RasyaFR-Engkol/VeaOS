/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : input.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Input
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veapi.h>

/* Revision History ------------------------------------------------------
 * DATE       : 20-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file input.c
 * --------------------------------------------------------------------- */

BOOLEAN IptRITRunning = FALSE;
BOOLEAN IptMouseRITRunning = FALSE;
UCHAR IptAsyncKeyState[256] = {0};
LONG   gCursorX          = 320;
LONG   gCursorY          = 240;
ULONG  gMouseButtonState = 0;   
HWND   gCaptureWindow    = NULL;

VOID
VEAPI
IptUserRawInputThread(
    IN PVOID StartContext
)
{
    VEASTATUS Status;
    HANDLE hKeyboard = NULL;
    OIP_INFORMATION OipInfo;
    OBJECT_ATTRIBUTES ObjAttr;
    ANSI_STRING KeyboardName;
    KEYBOARD_INPUT_DATA KeyData[8];
    ULONG ReadCount = 0;
    ULONG TotalPackets = 0;

    UNREFERENCED_PARAMETER(StartContext);

    RtlInitAnsiString(&KeyboardName, "/Device/Keyboard0");

    InitializeObjectAttributes(&ObjAttr, &KeyboardName, OBJ_KERNEL_HANDLE, NULL, NULL);

    Status = VzCreateFile(
        &hKeyboard,
        FILE_READ_DATA | VEA_SYNCHRONIZE,
        &ObjAttr,
        &OipInfo,
        NULL,
        FILE_ATTRIBUTE_NORMAL,
        FILE_SHARE_READ,
        FILE_OPEN,
        FILE_SYNCHRONOUS_IO_NONALERT,
        NULL,
        0
    );

    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[VEAPI] RIT gagal buka /Device/Keyboard0: 0x%08X\n", Status);
        return;
    }

    IptRITRunning = TRUE;

    while (IptRITRunning)
    {
        RtlZeroMemory(&OipInfo, sizeof(OipInfo));

        Status = VzReadFile(
            hKeyboard,
            NULL,
            NULL,
            NULL,
            &OipInfo,
            KeyData,
            sizeof(KeyData),
            NULL,
            NULL
        );

        if (!VEA_SUCCESS(Status))
        {
            KdPrintf("[VEAPI-RIT] read=%u error=0x%08X\n",
                     ReadCount,
                     Status);
            break;
        }

        ULONG PacketCount = (ULONG)(OipInfo.Information / sizeof(KEYBOARD_INPUT_DATA));
        TotalPackets += PacketCount;

        for (ULONG i = 0; i < PacketCount; i++)
        {
            UserProcessKeyPacket(&KeyData[i]);
        }
    }

    if (hKeyboard)
    {
        VzClose(hKeyboard);
    }
}

static SHORT
IptClampToShort(
    IN LONG Value
)
{
    if (Value > 32767)  return 32767;
    if (Value < -32768) return -32768;
    return (SHORT)Value;
}

VOID
VEAPI
IptMouseRawInputThread(
    IN PVOID StartContext
)
{
    VEASTATUS Status;
    HANDLE hMouse = NULL;
    OIP_INFORMATION OipInfo;
    OBJECT_ATTRIBUTES ObjAttr;
    ANSI_STRING MouseName;
    MOUSE_INPUT_DATA MouData[8];

    UNREFERENCED_PARAMETER(StartContext);

    RtlInitAnsiString(&MouseName, "/Device/Mouse0");
    InitializeObjectAttributes(&ObjAttr, &MouseName, OBJ_KERNEL_HANDLE, NULL, NULL);

    Status = VzCreateFile(
        &hMouse,
        FILE_READ_DATA | VEA_SYNCHRONIZE,
        &ObjAttr,
        &OipInfo,
        NULL,
        FILE_ATTRIBUTE_NORMAL,
        FILE_SHARE_READ,
        FILE_OPEN,
        FILE_SYNCHRONOUS_IO_NONALERT,
        NULL,
        0
    );

    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[VEAPI] RIT failed to open /Device/Mouse0: 0x%08X\n", Status);
        return;
    }

    IptMouseRITRunning = TRUE;

    while (IptMouseRITRunning)
    {
        RtlZeroMemory(&OipInfo, sizeof(OipInfo));

        Status = VzReadFile(
            hMouse,
            NULL,
            NULL,
            NULL,
            &OipInfo,
            MouData,
            sizeof(MouData),
            NULL,
            NULL
        );

        if (!VEA_SUCCESS(Status))
        {
            KdPrintf("[VEAPI] RIT Mouse VzReadFile error: 0x%08X\n", Status);
            break;
        }

        ULONG PacketCount = (ULONG)(OipInfo.Information / sizeof(MOUSE_INPUT_DATA));
        LONG PrevCursorX = gCursorX;
        LONG PrevCursorY = gCursorY;
        LONG AccDx = 0;
        LONG AccDy = 0;

        for (ULONG i = 0; i < PacketCount; i++)
        {
            PMOUSE_INPUT_DATA Pkt = &MouData[i];

            /* Gabungkan paket gerak relatif murni agar tidak spam message & VRAM */
            if (Pkt->ButtonFlags == 0 && !(Pkt->Flags & MOUSE_MOVE_ABSOLUTE))
            {
                AccDx += Pkt->LastX;
                AccDy += Pkt->LastY;
                continue;
            }

            /* Jika ada klik/wheel, gabungkan sisa delta sebelumnya dulu agar titik klik akurat */
            if (!(Pkt->Flags & MOUSE_MOVE_ABSOLUTE))
            {
                Pkt->LastX = IptClampToShort((LONG)Pkt->LastX + AccDx);
                Pkt->LastY = IptClampToShort((LONG)Pkt->LastY + AccDy);
            }
            AccDx = 0;
            AccDy = 0;

            UserProcessMousePacket(Pkt);
        }

        /* Flush sisa gerakan relatif di akhir batch */
        if (AccDx != 0 || AccDy != 0)
        {
            MOUSE_INPUT_DATA MovePkt;
            RtlZeroMemory(&MovePkt, sizeof(MovePkt));
            MovePkt.LastX = IptClampToShort(AccDx);
            MovePkt.LastY = IptClampToShort(AccDy);
            UserProcessMousePacket(&MovePkt);
        }

        /* Gambar kursor di layar cukup 1x setelah seluruh batch selesai diproses */
        if (gCursorX != PrevCursorX || gCursorY != PrevCursorY)
        {
            EngMovePointer(gCursorX, gCursorY);
        }
    }

    if (hMouse)
    {
        VzClose(hMouse);
    }
}

SHORT
VEAPI
VeaUserGetAsyncKeyState(
    IN INT vKey
)
{
    if (vKey < 0 || vKey >= 256)
    {
        return 0;
    }

    return (IptAsyncKeyState[vKey] & 0x80) ? (SHORT)0x8000 : 0;
}

/* EOF */