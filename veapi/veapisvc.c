#include "veapi.h"

#define SVC_ENTRY(Func, ArgCount) (ULONG_PTR)(Func),
ULONG_PTR VeapiSSDT[] = {
    SVC_ENTRY(VeaUserInitialize, 0)         /* Syscall ID: 0x1000 */
    SVC_ENTRY(VeaUserGetMessage, 4)         /* Syscall ID: 0x1001 */
    SVC_ENTRY(VeaUserGetAsyncKeyState, 1)   /* Syscall ID: 0x1002 */
    SVC_ENTRY(VeaUserTranslateMessage, 1)   /* Syscall ID: 0x1003 */
    SVC_ENTRY(VeaUserMapVirtualKeyEx, 3)    /* Syscall ID: 0x1004 */
    SVC_ENTRY(VeaUserRegisterClass, 1)      /* Syscall ID: 0x1005 */
    SVC_ENTRY(VeaUserCreateWindow, 3)       /* Syscall ID: 0x1006 */
    SVC_ENTRY(VeaUserDispatchMessage, 1)    /* Syscall ID: 0x1007 */
    SVC_ENTRY(VeaUserBeginPaint, 2)         /* Syscall ID: 0x1008 */
    SVC_ENTRY(VeaUserEndPaint, 2)           /* Syscall ID: 0x1009 */
    SVC_ENTRY(VeaUserInvalidateRect, 3)     /* Syscall ID: 0x100A */
    SVC_ENTRY(VeaGdiFillRect, 3)            /* Syscall ID: 0x100B */
    SVC_ENTRY(VeaUserCreateWindowEx, 12)    /* Syscall ID: 0x100C */
    SVC_ENTRY(VeaGdiDrawImage, 7)           /* Syscall ID: 0x100D */
    SVC_ENTRY(VeaGdiSetPixel, 4)            /* Syscall ID: 0x100E */
    SVC_ENTRY(VeaGdiGetPixel, 3)            /* Syscall ID: 0x100F */
    SVC_ENTRY(VeaGdiLineTo, 6)              /* Syscall ID: 0x1010 */
    SVC_ENTRY(VeaGdiInvertRect, 2)          /* Syscall ID: 0x1011 */
    SVC_ENTRY(VeaGdiGradientFill, 5)        /* Syscall ID: 0x1012 */
    SVC_ENTRY(VeaGdiBitBlt, 10)             /* Syscall ID: 0x1013 */
    SVC_ENTRY(VeaGdiTransparentBlt, 11)     /* Syscall ID: 0x1014 */
    SVC_ENTRY(VeaGdiAlphaBlend, 11)         /* Syscall ID: 0x1015 */
    SVC_ENTRY(VeaGdiDrawGlyph, 7)           /* Syscall ID: 0x1016 */
    SVC_ENTRY(VeaUserSendMessage, 4)        /* Syscall ID: 0x1017 */
    SVC_ENTRY(VeaGdiDrawText, 5)            /* Syscall ID: 0x1018 */
    SVC_ENTRY(VeaGdiGetTextExtent, 3)       /* Syscall ID: 0x1019 */
    SVC_ENTRY(VeaUserSetFocus, 1)           /* Syscall ID: 0x101A */
    SVC_ENTRY(VeaGdiCreateFont, 2)          /* Syscall ID: 0x101B */
    SVC_ENTRY(VeaGdiSelectFont, 2)          /* Syscall ID: 0x101C */
    SVC_ENTRY(VeaGdiDeleteFont, 1)          /* Syscall ID: 0x101D */
    SVC_ENTRY(VeaGdiGetTextExtentFont, 4)   /* Syscall ID: 0x101E */
    SVC_ENTRY(VeaUserDestroyWindow, 1)      /* Syscall ID: 0x101F */
    SVC_ENTRY(VeaUserShowWindow, 2)         /* Syscall ID: 0x1020 */
    SVC_ENTRY(VeaUserPostQuitMessage, 1)    /* Syscall ID: 0x1021 */
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
    SVC_ENTRY(VeaGdiCreateFont, 2)          
    SVC_ENTRY(VeaGdiSelectFont, 2)          
    SVC_ENTRY(VeaGdiDeleteFont, 1)         
    SVC_ENTRY(VeaGdiGetTextExtentFont, 4) 
    SVC_ENTRY(VeaUserDestroyWindow, 1)   
    SVC_ENTRY(VeaUserShowWindow, 2)    
    SVC_ENTRY(VeaUserPostQuitMessage, 1) 
};
#undef SVC_ENTRY

ULONG VeapiNumberOfSysCalls = sizeof(VeapiSSDT) / sizeof(VeapiSSDT[0]);