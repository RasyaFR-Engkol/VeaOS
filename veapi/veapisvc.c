#include "veapi.h"

#define SVC_ENTRY(Func, ArgCount) (ULONG_PTR)(Func),
ULONG_PTR VeapiSSDT[] = {
    SVC_ENTRY(VeaUserInitialize, 0)             /* Syscall ID: 0x1000 */
    SVC_ENTRY(VeaUserGetMessage, 4)             /* Syscall ID: 0x1001 */
    SVC_ENTRY(VeaUserGetAsyncKeyState, 1)       /* Syscall ID: 0x1002 */
    SVC_ENTRY(VeaUserTranslateMessage, 1)       /* Syscall ID: 0x1003 */
    SVC_ENTRY(VeaUserMapVirtualKeyEx, 3)        /* Syscall ID: 0x1004 */
    SVC_ENTRY(VeaUserRegisterClass, 1)          /* Syscall ID: 0x1005 */
    SVC_ENTRY(VeaUserCreateWindow, 3)           /* Syscall ID: 0x1006 */
    SVC_ENTRY(VeaUserDispatchMessage, 1)        /* Syscall ID: 0x1007 */
    SVC_ENTRY(VeaUserBeginPaint, 2)             /* Syscall ID: 0x1008 */
    SVC_ENTRY(VeaUserEndPaint, 2)               /* Syscall ID: 0x1009 */
    SVC_ENTRY(VeaUserInvalidateRect, 3)         /* Syscall ID: 0x100A */
    SVC_ENTRY(VeaGdiFillRect, 3)                /* Syscall ID: 0x100B */
    SVC_ENTRY(VeaUserCreateWindowEx, 12)        /* Syscall ID: 0x100C */
    SVC_ENTRY(VeaGdiDrawImage, 7)               /* Syscall ID: 0x100D */
    SVC_ENTRY(VeaGdiSetPixel, 4)                /* Syscall ID: 0x100E */
    SVC_ENTRY(VeaGdiGetPixel, 3)                /* Syscall ID: 0x100F */
    SVC_ENTRY(VeaGdiLineTo, 6)                  /* Syscall ID: 0x1010 */
    SVC_ENTRY(VeaGdiInvertRect, 2)              /* Syscall ID: 0x1011 */
    SVC_ENTRY(VeaGdiGradientFill, 5)            /* Syscall ID: 0x1012 */
    SVC_ENTRY(VeaGdiBitBlt, 10)                 /* Syscall ID: 0x1013 */
    SVC_ENTRY(VeaGdiTransparentBlt, 11)         /* Syscall ID: 0x1014 */
    SVC_ENTRY(VeaGdiAlphaBlend, 11)             /* Syscall ID: 0x1015 */
    SVC_ENTRY(VeaGdiDrawGlyph, 7)               /* Syscall ID: 0x1016 */
    SVC_ENTRY(VeaUserSendMessage, 4)            /* Syscall ID: 0x1017 */
    SVC_ENTRY(VeaGdiDrawText, 5)                /* Syscall ID: 0x1018 */
    SVC_ENTRY(VeaGdiGetTextExtent, 3)           /* Syscall ID: 0x1019 */
    SVC_ENTRY(VeaUserSetFocus, 1)               /* Syscall ID: 0x101A */
    SVC_ENTRY(VeaGdiCreateFont, 4)              /* Syscall ID: 0x101B */
    SVC_ENTRY(VeaGdiSelectFont, 2)              /* Syscall ID: 0x101C */
    SVC_ENTRY(VeaGdiDeleteFont, 1)              /* Syscall ID: 0x101D */
    SVC_ENTRY(VeaGdiGetTextExtentFont, 4)       /* Syscall ID: 0x101E */
    SVC_ENTRY(VeaUserDestroyWindow, 1)          /* Syscall ID: 0x101F */
    SVC_ENTRY(VeaUserShowWindow, 2)             /* Syscall ID: 0x1020 */
    SVC_ENTRY(VeaUserPostQuitMessage, 1)        /* Syscall ID: 0x1021 */
    SVC_ENTRY(VeaUserSetCursorShape, 5)         /* Syscall ID: 0x1022 */
    SVC_ENTRY(VeaUserShowCursor, 1)             /* Syscall ID: 0x1023 */
    SVC_ENTRY(VeaUserGetCursorPos, 1)           /* Syscall ID: 0x1024 */
    SVC_ENTRY(VeaUserSetCursorPos, 2)           /* Syscall ID: 0x1025 */
    SVC_ENTRY(VeaUserSetCapture, 1)             /* Syscall ID: 0x1026 */
    SVC_ENTRY(VeaUserReleaseCapture, 0)         /* Syscall ID: 0x1027 */
    SVC_ENTRY(VeaUserPeekMessage, 5)            /* Syscall ID: 0x1028 */
    SVC_ENTRY(VeaUserPostMessage, 4)            /* Syscall ID: 0x1029 */
    SVC_ENTRY(VeaUserGetDCEx, 3)                /* Syscall ID: 0x102A */
    SVC_ENTRY(VeaGdiDeleteDC, 1)                /* Syscall ID: 0x102B */
    SVC_ENTRY(VeaUserCreateDesktop, 6)          /* Syscall ID: 0x102C */
    SVC_ENTRY(VeaUserOpenDesktop, 4)            /* Syscall ID: 0x102D */
    SVC_ENTRY(VeaUserSetThreadDesktop, 1)       /* Syscall ID: 0x102E */
    SVC_ENTRY(VeaUserSwitchDesktop, 1)          /* Syscall ID: 0x102F */
    SVC_ENTRY(VeaUserSetLogonNotifyWindow, 1)   /* Syscall ID: 0x1030 */
    SVC_ENTRY(VeaUserBuildHwndList, 7)          /* Syscall ID: 0x1031 */
    SVC_ENTRY(VeaUserGetWindow, 2)              /* Syscall ID: 0x1032 */
    SVC_ENTRY(VeaUserFindWindowEx, 4)           /* Syscall ID: 0x1033 */
    SVC_ENTRY(VeaUserGetForegroundWindow, 0)    /* Syscall ID: 0x1034 */
    SVC_ENTRY(VeaUserSetForegroundWindow, 1)    /* Syscall ID: 0x1035 */
    SVC_ENTRY(VeaUserGetWindowText, 3)          /* Syscall ID: 0x1036 */
    SVC_ENTRY(VeaUserSetWindowText, 2)          /* Syscall ID: 0x1037 */
    SVC_ENTRY(VeaUserIsWindow, 1)               /* Syscall ID: 0x1038 */
    SVC_ENTRY(VeaUserIsWindowVisible, 1)        /* Syscall ID: 0x1039 */
    SVC_ENTRY(VeaUserGetWindowRect, 2)          /* Syscall ID: 0x103A */
    SVC_ENTRY(VeaUserGetClientRect, 2)          /* Syscall ID: 0x103B */
    SVC_ENTRY(VeaUserClientToScreen, 2)         /* Syscall ID: 0x103C */
    SVC_ENTRY(VeaUserScreenToClient, 2)         /* Syscall ID: 0x103D */
    SVC_ENTRY(VeaUserWindowFromPoint, 2)        /* Syscall ID: 0x103E */
    SVC_ENTRY(VeaUserGetSystemMetrics, 1)       /* Syscall ID: 0x103F */
    SVC_ENTRY(VeaUserGetWindowLong, 2)          /* Syscall ID: 0x1040 */
    SVC_ENTRY(VeaUserSetWindowLong, 3)          /* Syscall ID: 0x1041 */
    SVC_ENTRY(VeaUserSetWindowPos, 7)           /* Syscall ID: 0x1042 */
    SVC_ENTRY(VeaGdiCreateSolidBrush, 1)        /* Syscall ID: 0x1043 */
    SVC_ENTRY(VeaGdiCreatePen, 3)               /* Syscall ID: 0x1044 */
    SVC_ENTRY(VeaGdiGetStockObject, 1)          /* Syscall ID: 0x1045 */
    SVC_ENTRY(VeaGdiSelectObject, 2)            /* Syscall ID: 0x1046 */
    SVC_ENTRY(VeaGdiDeleteObject, 1)            /* Syscall ID: 0x1047 */
    SVC_ENTRY(VeaGdiMoveToEx, 4)                /* Syscall ID: 0x1048 */
    SVC_ENTRY(VeaGdiLineToCurrent, 3)           /* Syscall ID: 0x1049 */
    SVC_ENTRY(VeaGdiRectangle, 5)               /* Syscall ID: 0x104A */
    SVC_ENTRY(VeaGdiFillRectBrush, 3)           /* Syscall ID: 0x104B */
    SVC_ENTRY(VeaGdiFrameRect, 3)               /* Syscall ID: 0x104C */
    SVC_ENTRY(VeaUserGetClassLong, 2)           /* Syscall ID: 0x104D */
    SVC_ENTRY(VeaUserSetClassLong, 3)           /* Syscall ID: 0x104E */
    SVC_ENTRY(VeaGdiAddFontResourceEx, 3)       /* Syscall ID: 0x104F */
    SVC_ENTRY(VeaGdiRemoveFontResourceEx, 3)    /* Syscall ID: 0x1050 */
    SVC_ENTRY(VeaGdiAddFontMemResourceEx, 4)    /* Syscall ID: 0x1051 */
    SVC_ENTRY(VeaGdiRemoveFontMemResourceEx, 1) /* Syscall ID: 0x1052 */
    SVC_ENTRY(VeaUserSetTimer, 4)               /* Syscall ID: 0x1053 */
    SVC_ENTRY(VeaUserKillTimer, 2)              /* Syscall ID: 0x1054 */
    SVC_ENTRY(VeaUserSetCaptionButtonState, 3)  /* Syscall ID: 0x1055 */
    SVC_ENTRY(VeaUserSetTaskmanWindow, 1)       /* Syscall ID: 0x1056 */
    SVC_ENTRY(VeaUserRegisterShellHookWindow, 1)/* Syscall ID: 0x1057 */
    SVC_ENTRY(VeaUserDeregisterShellHookWindow, 1) /* Syscall ID: 0x1058 */
};
#undef SVC_ENTRY

#define SVC_ENTRY(Func, ArgCount) ((ArgCount) * sizeof(ULONG_PTR)),
UCHAR VeapiSSPT[] = {
    SVC_ENTRY(VeaUserInitialize, 0)
    SVC_ENTRY(VeaUserGetMessage, 4)
    SVC_ENTRY(VeaUserGetAsyncKeyState, 1)
    SVC_ENTRY(VeaUserTranslateMessage, 1)
    SVC_ENTRY(VeaUserMapVirtualKeyEx, 3)
    SVC_ENTRY(VeaUserRegisterClass, 1)
    SVC_ENTRY(VeaUserCreateWindow, 3)
    SVC_ENTRY(VeaUserDispatchMessage, 1)
    SVC_ENTRY(VeaUserBeginPaint, 2)
    SVC_ENTRY(VeaUserEndPaint, 2)
    SVC_ENTRY(VeaUserInvalidateRect, 3)
    SVC_ENTRY(VeaGdiFillRect, 3)
    SVC_ENTRY(VeaUserCreateWindowEx, 12)
    SVC_ENTRY(VeaGdiDrawImage, 7)
    SVC_ENTRY(VeaGdiSetPixel, 4)
    SVC_ENTRY(VeaGdiGetPixel, 3)
    SVC_ENTRY(VeaGdiLineTo, 6)
    SVC_ENTRY(VeaGdiInvertRect, 2)
    SVC_ENTRY(VeaGdiGradientFill, 5)
    SVC_ENTRY(VeaGdiBitBlt, 10)
    SVC_ENTRY(VeaGdiTransparentBlt, 11)
    SVC_ENTRY(VeaGdiAlphaBlend, 11)
    SVC_ENTRY(VeaGdiDrawGlyph, 7)
    SVC_ENTRY(VeaUserSendMessage, 4)
    SVC_ENTRY(VeaGdiDrawText, 5)
    SVC_ENTRY(VeaGdiGetTextExtent, 3)
    SVC_ENTRY(VeaUserSetFocus, 1)
    SVC_ENTRY(VeaGdiCreateFont, 4)
    SVC_ENTRY(VeaGdiSelectFont, 2)
    SVC_ENTRY(VeaGdiDeleteFont, 1)
    SVC_ENTRY(VeaGdiGetTextExtentFont, 4)
    SVC_ENTRY(VeaUserDestroyWindow, 1)
    SVC_ENTRY(VeaUserShowWindow, 2)
    SVC_ENTRY(VeaUserPostQuitMessage, 1)
    SVC_ENTRY(VeaUserSetCursorShape, 5)
    SVC_ENTRY(VeaUserShowCursor, 1)
    SVC_ENTRY(VeaUserGetCursorPos, 1)
    SVC_ENTRY(VeaUserSetCursorPos, 2)
    SVC_ENTRY(VeaUserSetCapture, 1)
    SVC_ENTRY(VeaUserReleaseCapture, 0)
    SVC_ENTRY(VeaUserPeekMessage, 5)
    SVC_ENTRY(VeaUserPostMessage, 4)
    SVC_ENTRY(VeaUserGetDCEx, 3)
    SVC_ENTRY(VeaGdiDeleteDC, 1)
    SVC_ENTRY(VeaUserCreateDesktop, 6)
    SVC_ENTRY(VeaUserOpenDesktop, 4)
    SVC_ENTRY(VeaUserSetThreadDesktop, 1)
    SVC_ENTRY(VeaUserSwitchDesktop, 1)
    SVC_ENTRY(VeaUserSetLogonNotifyWindow, 1)
    SVC_ENTRY(VeaUserBuildHwndList, 7)
    SVC_ENTRY(VeaUserGetWindow, 2)
    SVC_ENTRY(VeaUserFindWindowEx, 4)
    SVC_ENTRY(VeaUserGetForegroundWindow, 0)
    SVC_ENTRY(VeaUserSetForegroundWindow, 1)
    SVC_ENTRY(VeaUserGetWindowText, 3)
    SVC_ENTRY(VeaUserSetWindowText, 2)
    SVC_ENTRY(VeaUserIsWindow, 1)
    SVC_ENTRY(VeaUserIsWindowVisible, 1)
    SVC_ENTRY(VeaUserGetWindowRect, 2)
    SVC_ENTRY(VeaUserGetClientRect, 2)
    SVC_ENTRY(VeaUserClientToScreen, 2)
    SVC_ENTRY(VeaUserScreenToClient, 2)
    SVC_ENTRY(VeaUserWindowFromPoint, 2)
    SVC_ENTRY(VeaUserGetSystemMetrics, 1)
    SVC_ENTRY(VeaUserGetWindowLong, 2)
    SVC_ENTRY(VeaUserSetWindowLong, 3)
    SVC_ENTRY(VeaUserSetWindowPos, 7)
    SVC_ENTRY(VeaGdiCreateSolidBrush, 1)
    SVC_ENTRY(VeaGdiCreatePen, 3)
    SVC_ENTRY(VeaGdiGetStockObject, 1)
    SVC_ENTRY(VeaGdiSelectObject, 2)
    SVC_ENTRY(VeaGdiDeleteObject, 1)
    SVC_ENTRY(VeaGdiMoveToEx, 4)
    SVC_ENTRY(VeaGdiLineToCurrent, 3)
    SVC_ENTRY(VeaGdiRectangle, 5)
    SVC_ENTRY(VeaGdiFillRectBrush, 3)
    SVC_ENTRY(VeaGdiFrameRect, 3)
    SVC_ENTRY(VeaUserGetClassLong, 2)
    SVC_ENTRY(VeaUserSetClassLong, 3)
    SVC_ENTRY(VeaGdiAddFontResourceEx, 3)
    SVC_ENTRY(VeaGdiRemoveFontResourceEx, 3)
    SVC_ENTRY(VeaGdiAddFontMemResourceEx, 4)
    SVC_ENTRY(VeaGdiRemoveFontMemResourceEx, 1)
    SVC_ENTRY(VeaUserSetTimer, 4)
    SVC_ENTRY(VeaUserKillTimer, 2)
    SVC_ENTRY(VeaUserSetCaptionButtonState, 3)
    SVC_ENTRY(VeaUserSetTaskmanWindow, 1)      
    SVC_ENTRY(VeaUserRegisterShellHookWindow, 1)
    SVC_ENTRY(VeaUserDeregisterShellHookWindow, 1) 
};
#undef SVC_ENTRY

ULONG VeapiNumberOfSysCalls = sizeof(VeapiSSDT) / sizeof(VeapiSSDT[0]);