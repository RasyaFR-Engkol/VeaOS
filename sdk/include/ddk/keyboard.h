#pragma once

#define KBD_BUFFER_SIZE 128

typedef struct _KEYBOARD_INPUT_DATA {
    USHORT UnitId;
    USHORT MakeCode;
    USHORT Flags;
    USHORT Reserved;
    ULONG  ExtraInformation;
} KEYBOARD_INPUT_DATA, *PKEYBOARD_INPUT_DATA;

#define KEY_MAKE                0x0000  // Tombol ditekan (press)
#define KEY_BREAK               0x0001  // Tombol dilepas (release)
#define KEY_E0                  0x0002  // Prefix extended scancode 0xE0 (Arrow, Right Ctrl/Alt, dll.)
#define KEY_E1                  0x0004  // Prefix extended scancode 0xE1 (Pause/Break)

#define IOCTL_KEYBOARD_QUERY_ATTRIBUTES     CTL_CODE(FILE_DEVICE_KEYBOARD, 0x0000, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_KEYBOARD_SET_TYPEMATIC        CTL_CODE(FILE_DEVICE_KEYBOARD, 0x0001, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_KEYBOARD_SET_INDICATORS       CTL_CODE(FILE_DEVICE_KEYBOARD, 0x0002, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_KEYBOARD_QUERY_TYPEMATIC      CTL_CODE(FILE_DEVICE_KEYBOARD, 0x0008, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_KEYBOARD_QUERY_INDICATORS     CTL_CODE(FILE_DEVICE_KEYBOARD, 0x0010, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define KEYBOARD_LED_INJECTED               0x8000
#define KEYBOARD_SHADOW                     0x4000
#define KEYBOARD_KANA_LOCK_ON               0x0008
#define KEYBOARD_CAPS_LOCK_ON               0x0004
#define KEYBOARD_NUM_LOCK_ON                0x0002
#define KEYBOARD_SCROLL_LOCK_ON             0x0001

typedef struct _KEYBOARD_INDICATOR_PARAMETERS {
    USHORT UnitId;
    USHORT LedFlags;
} KEYBOARD_INDICATOR_PARAMETERS, *PKEYBOARD_INDICATOR_PARAMETERS;

typedef struct _KEYBOARD_TYPEMATIC_PARAMETERS {
    USHORT UnitId;
    USHORT Rate;    // Karakter per detik
    USHORT Delay;   // Delay sebelum repeat (ms)
} KEYBOARD_TYPEMATIC_PARAMETERS, *PKEYBOARD_TYPEMATIC_PARAMETERS;

typedef struct _KEYBOARD_ID {
    UCHAR Type;
    UCHAR Subtype;
} KEYBOARD_ID, *PKEYBOARD_ID;

typedef struct _KEYBOARD_ATTRIBUTES {
    KEYBOARD_ID KeyboardIdentifier;
    USHORT KeyboardMode;
    USHORT NumberOfFunctionKeys;
    USHORT NumberOfIndicators;
    USHORT NumberOfKeysTotal;
    ULONG  InputDataQueueLength;
    KEYBOARD_TYPEMATIC_PARAMETERS KeyRepeatMinimum;
    KEYBOARD_TYPEMATIC_PARAMETERS KeyRepeatMaximum;
} KEYBOARD_ATTRIBUTES, *PKEYBOARD_ATTRIBUTES;