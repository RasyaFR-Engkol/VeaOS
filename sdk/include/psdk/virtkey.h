#pragma once

/* Virtual Key Codes (Win32 / NT Standard) */
#define VK_BACK         0x08
#define VK_TAB          0x09
#define VK_RETURN       0x0D
#define VK_SHIFT        0x10
#define VK_CONTROL      0x11
#define VK_MENU         0x12    /* Alt */
#define VK_PAUSE        0x13
#define VK_CAPITAL      0x14    /* CapsLock */
#define VK_ESCAPE       0x1B
#define VK_SPACE        0x20
#define VK_PRIOR        0x21    /* Page Up */
#define VK_NEXT         0x22    /* Page Down */
#define VK_END          0x23
#define VK_HOME         0x24
#define VK_LEFT         0x25
#define VK_UP           0x26
#define VK_RIGHT        0x27
#define VK_DOWN         0x28
#define VK_SNAPSHOT     0x2C    /* Print Screen */
#define VK_INSERT       0x2D
#define VK_DELETE       0x2E

/* '0' - '9' sama dengan ASCII 0x30 - 0x39 */
/* 'A' - 'Z' sama dengan ASCII 0x41 - 0x5A */

#define VK_LWIN         0x5B
#define VK_RWIN         0x5C
#define VK_APPS         0x5D

#define VK_NUMPAD0      0x60
#define VK_NUMPAD1      0x61
#define VK_NUMPAD2      0x62
#define VK_NUMPAD3      0x63
#define VK_NUMPAD4      0x64
#define VK_NUMPAD5      0x65
#define VK_NUMPAD6      0x66
#define VK_NUMPAD7      0x67
#define VK_NUMPAD8      0x68
#define VK_NUMPAD9      0x69
#define VK_MULTIPLY     0x6A
#define VK_ADD          0x6B
#define VK_SUBTRACT     0x6D
#define VK_DECIMAL      0x6E
#define VK_DIVIDE       0x6F

#define VK_F1           0x70
#define VK_F2           0x71
#define VK_F3           0x72
#define VK_F4           0x73
#define VK_F5           0x74
#define VK_F6           0x75
#define VK_F7           0x76
#define VK_F8           0x77
#define VK_F9           0x78
#define VK_F10          0x79
#define VK_F11          0x7A
#define VK_F12          0x7B

#define VK_NUMLOCK      0x90
#define VK_SCROLL       0x91

#define VK_LSHIFT       0xA0
#define VK_RSHIFT       0xA1
#define VK_LCONTROL     0xA2
#define VK_RCONTROL     0xA3
#define VK_LMENU        0xA4
#define VK_RMENU        0xA5

#define VK_OEM_1        0xBA    /* ';:' */
#define VK_OEM_PLUS     0xBB    /* '=+' */
#define VK_OEM_COMMA    0xBC    /* ',<' */
#define VK_OEM_MINUS    0xBD    /* '-_' */
#define VK_OEM_PERIOD   0xBE    /* '.>' */
#define VK_OEM_2        0xBF    /* '/?' */
#define VK_OEM_3        0xC0    /* '`~' */
#define VK_OEM_4        0xDB    /* '[{' */
#define VK_OEM_5        0xDC    /* '\|' */
#define VK_OEM_6        0xDD    /* ']}' */
#define VK_OEM_7        0xDE    /* ''"' */
#define VK_OEM_102      0xE2    /* '<>' */

#define MAPVK_VK_TO_VSC     0
#define MAPVK_VSC_TO_VK     1
#define MAPVK_VK_TO_CHAR    2
#define MAPVK_VSC_TO_VK_EX  3
#define MAPVK_VK_TO_VSC_EX  4