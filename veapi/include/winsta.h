#pragma once

#include <veakrnl.h>

typedef struct _DESKTOP_OBJECT DESKTOP_OBJECT, *PDESKTOP_OBJECT;

typedef struct _WINDOW_STATION_OBJECT
{
    ANSI_STRING     Name;
    ULONG           Flags;
    KSPIN_LOCK      Lock;
    LIST_ENTRY      DesktopListHead;  
    PDESKTOP_OBJECT ActiveDesktop;   
} WINDOW_STATION_OBJECT, *PWINDOW_STATION_OBJECT;

typedef struct _DESKTOP_OBJECT
{
    ANSI_STRING             Name;
    LIST_ENTRY              ListEntry;        
    PWINDOW_STATION_OBJECT  ParentWinSta;
    KSPIN_LOCK              Lock;
    LIST_ENTRY              WindowListHead;    
    PVOID                   ForegroundWindow;  
    PVOID                   FocusWindow;    
    ULONG                   BackgroundColor;  
    HWND                    TaskmanWindow;
    HWND                    ShellHookWindow; 
} DESKTOP_OBJECT, *PDESKTOP_OBJECT;

extern PWINDOW_STATION_OBJECT GpInteractiveWindowStation;
extern PDESKTOP_OBJECT        GpActiveDesktop;

VEASTATUS VEAPI UserInitializeWindowStations(VOID);