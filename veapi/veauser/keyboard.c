/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : keyboard.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Keyboard Processing
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veapi.h>

/* Revision History ------------------------------------------------------
 * DATE       : 20-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file keyboard.c
 * --------------------------------------------------------------------- */

static const UCHAR ScanCodeToVk[128] = {
    /* 0x00 - 0x0F */
    0,          VK_ESCAPE,  '1',        '2',        '3',        '4',        '5',        '6',
    '7',        '8',        '9',        '0',        VK_OEM_MINUS, VK_OEM_PLUS, VK_BACK,  VK_TAB,
    /* 0x10 - 0x1F */
    'Q',        'W',        'E',        'R',        'T',        'Y',        'U',        'I',
    'O',        'P',        VK_OEM_4,   VK_OEM_6,   VK_RETURN,  VK_LCONTROL,'A',        'S',
    /* 0x20 - 0x2F */
    'D',        'F',        'G',        'H',        'J',        'K',        'L',        VK_OEM_1,
    VK_OEM_7,   VK_OEM_3,   VK_LSHIFT,  VK_OEM_5,   'Z',        'X',        'C',        'V',
    /* 0x30 - 0x3F */
    'B',        'N',        'M',        VK_OEM_COMMA, VK_OEM_PERIOD, VK_OEM_2, VK_RSHIFT, VK_MULTIPLY,
    VK_LMENU,   VK_SPACE,   VK_CAPITAL, VK_F1,      VK_F2,      VK_F3,      VK_F4,      VK_F5,
    /* 0x40 - 0x4F */
    VK_F6,      VK_F7,      VK_F8,      VK_F9,      VK_F10,     VK_NUMLOCK, VK_SCROLL,  VK_NUMPAD7,
    VK_NUMPAD8, VK_NUMPAD9, VK_SUBTRACT,VK_NUMPAD4, VK_NUMPAD5, VK_NUMPAD6, VK_ADD,     VK_NUMPAD1,
    /* 0x50 - 0x5F */
    VK_NUMPAD2, VK_NUMPAD3, VK_NUMPAD0, VK_DECIMAL, 0,          0,          VK_OEM_102, VK_F11,
    VK_F12,     0,          0,          0,          0,          0,          0,          0
};

static const CHAR ScanCodeToAsciiNormal[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t','q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'','`',
    0,   '\\','z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0,   ' '
};

static const CHAR ScanCodeToAsciiShift[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t','Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0,   ' '
};

static UCHAR
UserTranslateExtendedVk(
    IN USHORT MakeCode
)
{
    switch (MakeCode)
    {
        case 0x1C: return VK_RETURN;
        case 0x1D: return VK_RCONTROL;
        case 0x35: return VK_DIVIDE;
        case 0x37: return VK_SNAPSHOT;
        case 0x38: return VK_RMENU;
        case 0x47: return VK_HOME;
        case 0x48: return VK_UP;
        case 0x49: return VK_PRIOR;
        case 0x4B: return VK_LEFT;
        case 0x4D: return VK_RIGHT;
        case 0x4F: return VK_END;
        case 0x50: return VK_DOWN;
        case 0x51: return VK_NEXT;
        case 0x52: return VK_INSERT;
        case 0x53: return VK_DELETE;
        case 0x5B: return VK_LWIN;
        case 0x5C: return VK_RWIN;
        case 0x5D: return VK_APPS;
        default:   return 0;
    }
}

static UCHAR
UserSimplifyVk(
    IN UCHAR Vk
)
{
    switch (Vk)
    {
        case VK_LSHIFT:
        case VK_RSHIFT:
            return VK_SHIFT;
        case VK_LCONTROL:
        case VK_RCONTROL:
            return VK_CONTROL;
        case VK_LMENU:
        case VK_RMENU:
            return VK_MENU;
        default:
            return Vk;
    }
}

static VOID
UserSetKeyDownState(
    IN UCHAR Vk,
    IN BOOLEAN IsDown
)
{
    if (Vk == 0) return;

    if (IsDown)
        IptAsyncKeyState[Vk] |= 0x80;
    else
        IptAsyncKeyState[Vk] &= ~0x80;
}

VOID
VEAPI
UserProcessKeyPacket(
    IN PKEYBOARD_INPUT_DATA KeyData
)
{
    BOOLEAN IsBreak = (KeyData->Flags & KEY_BREAK) != 0;
    BOOLEAN IsE0    = (KeyData->Flags & KEY_E0) != 0;
    USHORT Code     = KeyData->MakeCode;
    UCHAR Vk        = 0;

    if (IsE0)
    {
        Vk = UserTranslateExtendedVk(Code);
    }
    else if (Code < 128)
    {
        Vk = ScanCodeToVk[Code];
    }

    if (Vk == 0)
    {
        return;
    }

    /* Update state tombol & gabungkan modifier kiri/kanan */
    UserSetKeyDownState(Vk, !IsBreak);

    if (Vk == VK_LSHIFT || Vk == VK_RSHIFT)
    {
        BOOLEAN ShiftDown = ((IptAsyncKeyState[VK_LSHIFT] | IptAsyncKeyState[VK_RSHIFT]) & 0x80) != 0;
        UserSetKeyDownState(VK_SHIFT, ShiftDown);
    }
    else if (Vk == VK_LCONTROL || Vk == VK_RCONTROL)
    {
        BOOLEAN CtrlDown = ((IptAsyncKeyState[VK_LCONTROL] | IptAsyncKeyState[VK_RCONTROL]) & 0x80) != 0;
        UserSetKeyDownState(VK_CONTROL, CtrlDown);
    }
    else if (Vk == VK_LMENU || Vk == VK_RMENU)
    {
        BOOLEAN AltDown = ((IptAsyncKeyState[VK_LMENU] | IptAsyncKeyState[VK_RMENU]) & 0x80) != 0;
        UserSetKeyDownState(VK_MENU, AltDown);
    }
    else if (Vk == VK_CAPITAL && !IsBreak)
    {
        IptAsyncKeyState[VK_CAPITAL] ^= 0x01;
    }
    else if (Vk == VK_NUMLOCK && !IsBreak)
    {
        IptAsyncKeyState[VK_NUMLOCK] ^= 0x01;
    }

    /* Susun lParam standar Win32 */
    LONG_PTR lParam = (LONG_PTR)((Code & 0xFF) << 16) | 1;
    if (IsE0)    lParam |= (1 << 24);
    if (IsBreak) lParam |= (1 << 31);

    /* Kalau Alt ditekan (tanpa Ctrl), kirim WM_SYSKEYDOWN / WM_SYSKEYUP */
    BOOLEAN AltDown  = (IptAsyncKeyState[VK_MENU] & 0x80) != 0;
    BOOLEAN CtrlDown = (IptAsyncKeyState[VK_CONTROL] & 0x80) != 0;
    ULONG MsgId;

    if (AltDown && !CtrlDown)
    {
        MsgId = IsBreak ? WM_SYSKEYUP : WM_SYSKEYDOWN;
    }
    else
    {
        MsgId = IsBreak ? WM_KEYUP : WM_KEYDOWN;
    }

    MsgPostInputMessage(MsgId, Vk, lParam);
}

BOOLEAN
VEAPI
VeaUserTranslateMessage(
    IN const PMESSAGE_QUEUE UserMsg
)
{
    MESSAGE_QUEUE LocalMsg;

    if (!UserMsg)
    {
        return FALSE;
    }

    if (KeGetPreviousMode() == UserMode)
    {
        if ((ULONG_PTR)UserMsg >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS ||
            ((ULONG_PTR)UserMsg + sizeof(MESSAGE_QUEUE)) >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS)
        {
            return FALSE;
        }

        _VEA_TRY
        {
            RtlCopyMemory(&LocalMsg, UserMsg, sizeof(MESSAGE_QUEUE));
        }
        _VEA_EXCEPT(1)
        {
            return FALSE;
        }
        _VEA_END
    }
    else
    {
        RtlCopyMemory(&LocalMsg, UserMsg, sizeof(MESSAGE_QUEUE));
    }

    /* Hanya proses WM_KEYDOWN dan WM_SYSKEYDOWN */
    if (LocalMsg.message != WM_KEYDOWN && LocalMsg.message != WM_SYSKEYDOWN)
    {
        return FALSE;
    }

    UCHAR Vk       = (UCHAR)(LocalMsg.wParam & 0xFF);
    UCHAR ScanCode = (UCHAR)((LocalMsg.lParam >> 16) & 0xFF);
    BOOLEAN IsE0   = (LocalMsg.lParam & (1 << 24)) != 0;

    BOOLEAN ShiftDown = (IptAsyncKeyState[VK_SHIFT] & 0x80) != 0;
    BOOLEAN CtrlDown  = (IptAsyncKeyState[VK_CONTROL] & 0x80) != 0;
    BOOLEAN AltDown   = (IptAsyncKeyState[VK_MENU] & 0x80) != 0;
    BOOLEAN CapsLock  = (IptAsyncKeyState[VK_CAPITAL] & 0x01) != 0;

    CHAR Ch = 0;

    /* Handle kombinasi Ctrl + huruf/simbol (misal Ctrl+C -> 0x03) */
    if (CtrlDown && !AltDown)
    {
        if (Vk >= 'A' && Vk <= 'Z')
        {
            Ch = (CHAR)(Vk - 'A' + 1);
        }
        else if (Vk == VK_OEM_4) /* '[' */
        {
            Ch = 0x1B;
        }
        else if (Vk == VK_OEM_5) /* '\' */
        {
            Ch = 0x1C;
        }
        else if (Vk == VK_OEM_6) /* ']' */
        {
            Ch = 0x1D;
        }
        else
        {
            return FALSE;
        }
    }
    else if (!IsE0 && ScanCode < 128)
    {
        Ch = ShiftDown ? ScanCodeToAsciiShift[ScanCode] : ScanCodeToAsciiNormal[ScanCode];

        if (CapsLock && ((Ch >= 'a' && Ch <= 'z') || (Ch >= 'A' && Ch <= 'Z')))
        {
            Ch = ShiftDown ? ScanCodeToAsciiNormal[ScanCode] : ScanCodeToAsciiShift[ScanCode];
        }
    }
    else if (IsE0 && Vk == VK_RETURN)
    {
        Ch = '\r';
    }

    if (Ch == 0)
    {
        return FALSE;
    }

    ULONG CharMsg = (LocalMsg.message == WM_SYSKEYDOWN) ? WM_SYSCHAR : WM_CHAR;
    MsgPostInputMessage(CharMsg, (UCHAR)Ch, LocalMsg.lParam);
    return TRUE;
}

UINT
VEAPI
VeaUserMapVirtualKeyEx(
    IN UINT uCode,
    IN UINT uType,
    IN ULONG dwhkl
)
{
    UNREFERENCED_PARAMETER(dwhkl);

    switch (uType)
    {
        case MAPVK_VK_TO_VSC:
        case MAPVK_VK_TO_VSC_EX:
        {
            UCHAR TargetVk = (UCHAR)(uCode & 0xFF);
            if (uType == MAPVK_VK_TO_VSC)
            {
                if (TargetVk == VK_SHIFT)   TargetVk = VK_LSHIFT;
                if (TargetVk == VK_CONTROL) TargetVk = VK_LCONTROL;
                if (TargetVk == VK_MENU)    TargetVk = VK_LMENU;
            }
            else
            {
                if (TargetVk == VK_RCONTROL) return 0xE01D;
                if (TargetVk == VK_RMENU)    return 0xE038;
                if (TargetVk == VK_UP)       return 0xE048;
                if (TargetVk == VK_DOWN)     return 0xE050;
                if (TargetVk == VK_LEFT)     return 0xE04B;
                if (TargetVk == VK_RIGHT)    return 0xE04D;
                if (TargetVk == VK_DELETE)   return 0xE053;
                if (TargetVk == VK_INSERT)   return 0xE052;
                if (TargetVk == VK_HOME)     return 0xE047;
                if (TargetVk == VK_END)      return 0xE04F;
                if (TargetVk == VK_PRIOR)    return 0xE049;
                if (TargetVk == VK_NEXT)     return 0xE051;
            }

            for (UINT sc = 1; sc < 128; sc++)
            {
                if (ScanCodeToVk[sc] == TargetVk)
                {
                    return sc;
                }
            }
            return 0;
        }

        case MAPVK_VSC_TO_VK:
        case MAPVK_VSC_TO_VK_EX:
        {
            UCHAR Vk = 0;
            if ((uCode & 0xFF00) == 0xE000)
            {
                Vk = UserTranslateExtendedVk((USHORT)(uCode & 0xFF));
            }
            else if (uCode < 128)
            {
                Vk = ScanCodeToVk[uCode];
            }

            if (uType == MAPVK_VSC_TO_VK)
            {
                Vk = UserSimplifyVk(Vk);
            }
            return Vk;
        }

        case MAPVK_VK_TO_CHAR:
        {
            UCHAR TargetVk = (UCHAR)(uCode & 0xFF);
            for (UINT sc = 1; sc < 128; sc++)
            {
                if (ScanCodeToVk[sc] == TargetVk)
                {
                    CHAR Ch = ScanCodeToAsciiNormal[sc];
                    if (Ch >= 'a' && Ch <= 'z')
                    {
                        Ch = (CHAR)(Ch - 'a' + 'A');
                    }
                    return (UINT)(UCHAR)Ch;
                }
            }
            return 0;
        }

        default:
            return 0;
    }
}

/* EOF */