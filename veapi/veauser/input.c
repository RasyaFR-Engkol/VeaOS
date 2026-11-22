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
UCHAR IptAsyncKeyState[256] = {0};

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
            KdPrintf("[VEAPI] RIT VzReadFile error: 0x%08X\n", Status);
            break;
        }

        ULONG PacketCount = (ULONG)(OipInfo.Information / sizeof(KEYBOARD_INPUT_DATA));
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