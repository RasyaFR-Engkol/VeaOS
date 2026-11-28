/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : main.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Explorer Shell Main
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <explorer.h>

/* Revision History ------------------------------------------------------
 * DATE       : 26-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file main.c
 * --------------------------------------------------------------------- */


#define TASKBAR_HEIGHT      30
#define IDT_SHELL_TICK      1
#define IDC_START_BUTTON    1001
#define TASK_AREA_LEFT      76
#define TASK_TRAY_WIDTH     76
#define TASK_BTN_MAX_WIDTH  160
#define TASK_BTN_GAP        4

static HWND GhTaskbarWnd = NULL;
static HWND GhStartBtn   = NULL;

#define MAX_TASKBAR_ITEMS   32
#define IDC_TASK_BTN_BASE   2000

typedef struct _TASK_ITEM {
    HWND hWnd;
    CHAR Title[64];
} TASK_ITEM, *PTASK_ITEM;

static TASK_ITEM gTaskItems[MAX_TASKBAR_ITEMS];
static INT       gTaskCount      = 0;
static HWND      gActiveTaskHwnd = NULL;

static BOOL
TaskGetButtonRect(
    IN  INT         Index,
    IN  const RECT *pBarRect,
    OUT RECT       *pBtnRect
)
{
    if (!pBarRect || !pBtnRect || gTaskCount <= 0 || Index < 0 || Index >= gTaskCount)
    {
        return FALSE;
    }

    INT AreaLeft  = TASK_AREA_LEFT;
    INT AreaRight = pBarRect->right - TASK_TRAY_WIDTH;
    INT AvailW    = AreaRight - AreaLeft;
    if (AvailW <= 20)
    {
        return FALSE;
    }

    INT TotalGaps = (gTaskCount - 1) * TASK_BTN_GAP;
    INT BtnW      = (AvailW - TotalGaps) / gTaskCount;
    if (BtnW > TASK_BTN_MAX_WIDTH)
    {
        BtnW = TASK_BTN_MAX_WIDTH;
    }
    if (BtnW < 24)
    {
        BtnW = 24;
    }

    pBtnRect->left   = AreaLeft + Index * (BtnW + TASK_BTN_GAP);
    pBtnRect->right  = pBtnRect->left + BtnW;
    pBtnRect->top    = pBarRect->top + 4;
    pBtnRect->bottom = pBarRect->bottom - 3;

    return (pBtnRect->left < AreaRight);
}

static VOID
TaskDrawButtons(
    IN HDC         hdc,
    IN const RECT *pBarRect
)
{
    if (gTaskCount <= 0)
    {
        return;
    }

    HBRUSH hBrushNormal = CreateSolidBrush(RGB(60, 132, 243));
    HBRUSH hBrushActive = CreateSolidBrush(RGB(25, 82, 185));
    HPEN   hPenLight    = CreatePen(PS_SOLID, 1, RGB(114, 174, 255));
    HPEN   hPenDark     = CreatePen(PS_SOLID, 1, RGB(20, 62, 148));

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));

    for (INT i = 0; i < gTaskCount; i++)
    {
        RECT BtnRc;
        if (!TaskGetButtonRect(i, pBarRect, &BtnRc))
        {
            break;
        }

        BOOL IsActive = (gTaskItems[i].hWnd == gActiveTaskHwnd);

        /* 1. Isi background tombol */
        FillRect(hdc, &BtnRc, IsActive ? hBrushActive : hBrushNormal);

        /* 2. Bevel 3D ala Luna (pressed kalau aktif, raised kalau normal) */
        HPEN hOldPen = (HPEN)SelectObject(hdc, IsActive ? hPenDark : hPenLight);
        MoveToEx(hdc, BtnRc.left, BtnRc.bottom - 1, NULL);
        LineTo(hdc, BtnRc.left, BtnRc.top);
        LineTo(hdc, BtnRc.right - 1, BtnRc.top);

        SelectObject(hdc, IsActive ? hPenLight : hPenDark);
        MoveToEx(hdc, BtnRc.right - 1, BtnRc.top, NULL);
        LineTo(hdc, BtnRc.right - 1, BtnRc.bottom - 1);
        LineTo(hdc, BtnRc.left, BtnRc.bottom - 1);
        SelectObject(hdc, hOldPen);

        /* 3. Potong teks judul secara aman sesuai lebar tombol */
        CHAR DisplayTitle[64];
        INT MaxChars = (BtnRc.right - BtnRc.left - 14) / 8;
        if (MaxChars < 1)
        {
            MaxChars = 1;
        }
        if (MaxChars > (INT)sizeof(DisplayTitle) - 1)
        {
            MaxChars = (INT)sizeof(DisplayTitle) - 1;
        }

        INT Len = 0;
        while (gTaskItems[i].Title[Len] != '\0' && Len < MaxChars)
        {
            DisplayTitle[Len] = gTaskItems[i].Title[Len];
            Len++;
        }
        DisplayTitle[Len] = '\0';

        RECT TextRc = BtnRc;
        TextRc.left += IsActive ? 9 : 8;
        TextRc.top  += IsActive ? 1 : 0;
        DrawTextA(hdc, DisplayTitle, -1, &TextRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    DeleteObject(hBrushNormal);
    DeleteObject(hBrushActive);
    DeleteObject(hPenLight);
    DeleteObject(hPenDark);
}

static INT
TaskFindIndex(IN HWND hWnd)
{
    for (INT i = 0; i < gTaskCount; i++)
    {
        if (gTaskItems[i].hWnd == hWnd)
        {
            return i;
        }
    }
    return -1;
}

static VOID
TaskAddOrUpdate(IN HWND hWnd)
{
    if (!hWnd || hWnd == GhTaskbarWnd)
    {
        return;
    }

    INT Idx = TaskFindIndex(hWnd);
    if (Idx < 0)
    {
        if (gTaskCount >= MAX_TASKBAR_ITEMS)
        {
            return;
        }
        Idx = gTaskCount++;
        gTaskItems[Idx].hWnd = hWnd;
    }

    GetWindowTextA(hWnd, gTaskItems[Idx].Title, sizeof(gTaskItems[Idx].Title));
    if (gTaskItems[Idx].Title[0] == '\0')
    {
        RtlCopyMemory(gTaskItems[Idx].Title, "Application", 12);
    }

    InvalidateRect(GhTaskbarWnd, NULL, FALSE);
}

static VOID
TaskRemove(IN HWND hWnd)
{
    INT Idx = TaskFindIndex(hWnd);
    if (Idx < 0)
    {
        return;
    }

    for (INT i = Idx; i < gTaskCount - 1; i++)
    {
        gTaskItems[i] = gTaskItems[i + 1];
    }
    gTaskCount--;

    if (gActiveTaskHwnd == hWnd)
    {
        gActiveTaskHwnd = NULL;
    }

    InvalidateRect(GhTaskbarWnd, NULL, FALSE);
}

BOOLEAN CALLBACK
InitialEnumWindowsProc(IN HWND hWnd, IN LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    LONG Style   = GetWindowLongA(hWnd, GWL_STYLE);
    LONG ExStyle = GetWindowLongA(hWnd, GWL_EXSTYLE);

    if (hWnd == GhTaskbarWnd || (Style & WS_CHILD) || !(Style & WS_VISIBLE))
    {
        return TRUE;
    }

    if (!(ExStyle & WS_EX_APPWINDOW))
    {
        if ((ExStyle & WS_EX_TOOLWINDOW) || GetWindow(hWnd, GW_OWNER) != NULL)
        {
            return TRUE;
        }
    }

    TaskAddOrUpdate(hWnd);
    return TRUE;
}

/* Dipanggil saat tombol aplikasi ke-Idx di taskbar diklik */
static VOID
TaskHandleButtonClick(IN INT Idx)
{
    if (Idx < 0 || Idx >= gTaskCount)
    {
        return;
    }

    HWND TargetHwnd = gTaskItems[Idx].hWnd;
    LONG Style = GetWindowLongA(TargetHwnd, GWL_STYLE);

    if (Style & WS_MINIMIZE)
    {
        ShowWindow(TargetHwnd, SW_RESTORE);
        SetForegroundWindow(TargetHwnd);
        gActiveTaskHwnd = TargetHwnd;
    }
    else if (gActiveTaskHwnd != TargetHwnd)
    {
        SetForegroundWindow(TargetHwnd);
        gActiveTaskHwnd = TargetHwnd;
    }
    else
    {
        ShowWindow(TargetHwnd, SW_MINIMIZE);
        gActiveTaskHwnd = NULL;
    }

    InvalidateRect(GhTaskbarWnd, NULL, FALSE);
}

static LRESULT CALLBACK
ShellTrayWndProc(
    IN HWND   hWnd,
    IN UINT   Msg,
    IN WPARAM wParam,
    IN LPARAM lParam
)
{
    switch (Msg)
    {
        case WM_CREATE:
        {
            GhTaskbarWnd = hWnd;

            SetTaskmanWindow(hWnd);
            RegisterShellHookWindow(hWnd);

            EnumWindows(InitialEnumWindowsProc, 0);
            gActiveTaskHwnd = GetForegroundWindow();

            /* Buat tombol Start memakai class BUTTON bawaan userapi.so */
            GhStartBtn = CreateWindowExA(
                WS_EX_TOOLWINDOW,
                "BUTTON",
                "Start",
                WS_CHILD | WS_VISIBLE,
                4,
                3,
                64,
                TASKBAR_HEIGHT - 6,
                hWnd,
                (HMENU)(ULONG_PTR)IDC_START_BUTTON,
                NULL,
                NULL
            );
            return 0;
        }

        case WM_SHELLHOOKMESSAGE:
        {
            HWND TargetHwnd = (HWND)lParam;

            DPRINT("Got WMSHELLHOOKMESSAGE with wParam %d\n", wParam);

            switch (wParam)
            {
                case HSHELL_WINDOWCREATED:
                case HSHELL_REDRAW:
                    TaskAddOrUpdate(TargetHwnd);
                    break;

                case HSHELL_WINDOWDESTROYED:
                    TaskRemove(TargetHwnd);
                    break;

                case HSHELL_WINDOWACTIVATED:
                    if (TargetHwnd != NULL && TargetHwnd != GhTaskbarWnd)
                    {
                        gActiveTaskHwnd = TargetHwnd;
                    }
                    else if (gActiveTaskHwnd != NULL &&
                             (GetWindowLongA(gActiveTaskHwnd, GWL_STYLE) & WS_MINIMIZE))
                    {
                        gActiveTaskHwnd = NULL;
                    }
                    InvalidateRect(hWnd, NULL, FALSE);
                    break;
            }
            return 0;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            if (hdc)
            {
                RECT rc;
                GetClientRect(hWnd, &rc);

                /* 1. Background biru taskbar Luna */
                HBRUSH hBarBrush = CreateSolidBrush(RGB(36, 93, 219));
                FillRect(hdc, &rc, hBarBrush);
                DeleteObject(hBarBrush);

                /* 2. Garis highlight atas 2px ala XP Luna */
                HPEN hTopPen1 = CreatePen(PS_SOLID, 1, RGB(62, 134, 247));
                HPEN hTopPen2 = CreatePen(PS_SOLID, 1, RGB(115, 175, 255));
                HPEN hOldPen  = (HPEN)SelectObject(hdc, hTopPen1);
                MoveToEx(hdc, 0, 0, NULL);
                LineTo(hdc, rc.right, 0);

                SelectObject(hdc, hTopPen2);
                MoveToEx(hdc, 0, 1, NULL);
                LineTo(hdc, rc.right, 1);
                SelectObject(hdc, hOldPen);
                DeleteObject(hTopPen1);
                DeleteObject(hTopPen2);

                /* 3. Area System Tray kanan */
                RECT TrayRc = rc;
                TrayRc.left = rc.right - TASK_TRAY_WIDTH;
                TrayRc.top  = 2;
                HBRUSH hTrayBrush = CreateSolidBrush(RGB(18, 126, 232));
                FillRect(hdc, &TrayRc, hTrayBrush);
                DeleteObject(hTrayBrush);

                /* 4. Gambar semua tombol aplikasi yang terbuka */
                TaskDrawButtons(hdc, &rc);

                EndPaint(hWnd, &ps);
            }
            return 0;
        }

        case WM_LBUTTONDOWN:
        {
            POINT pt;
            pt.x = (SHORT)LOWORD(lParam);
            pt.y = (SHORT)HIWORD(lParam);

            RECT rc;
            GetClientRect(hWnd, &rc);

            for (INT i = 0; i < gTaskCount; i++)
            {
                RECT BtnRc;
                if (TaskGetButtonRect(i, &rc, &BtnRc) && PtInRect(&BtnRc, pt))
                {
                    TaskHandleButtonClick(i);
                    break;
                }
            }
            return 0;
        }

        case WM_COMMAND:
        {
            if (LOWORD(wParam) == IDC_START_BUTTON)
            {
                /* TODO: Toggle Start Menu popup */
            }
            return 0;
        }

        case WM_TIMER:
        {
            if (wParam == IDT_SHELL_TICK)
            {
                /* TODO: Refresh daftar tombol window aktif & jam tray */
            }
            return 0;
        }

        case WM_CLOSE:
            /* Standar shell Windows: Alt+F4 di taskbar tidak menutup explorer */
            return 0;

        case WM_DESTROY:
            DeregisterShellHookWindow(hWnd);
            SetTaskmanWindow(NULL);
            KillTimer(hWnd, IDT_SHELL_TICK);
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcA(hWnd, Msg, wParam, lParam);
}

INT
WINAPI
WinMain
(
    IN HINSTANCE hInstance,
    IN HINSTANCE hPrevInstance,
    IN PSTR      lpCmdLine,
    IN INT       nCmdShow
)
{
    if (FindWindowExA(NULL, NULL, "Shell_TrayWnd", NULL) != NULL)
    {
        return 0;
    }

    INT ScreenW = GetSystemMetrics(SM_CXSCREEN);
    INT ScreenH = GetSystemMetrics(SM_CYSCREEN);
    if (ScreenW <= 0) ScreenW = 800;
    if (ScreenH <= 0) ScreenH = 600;

    WNDCLASS wc;
    RtlZeroMemory(&wc, sizeof(wc));
    wc.Style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = ShellTrayWndProc;
    wc.hInstance     = hInstance;
    wc.hbrBackground = (HBRUSH)GetStockObject(LTGRAY_BRUSH);
    wc.lpszClassName = "Shell_TrayWnd";

    if (!RegisterClassA((const WNDCLASSA*)&wc))
    {
        return 1;
    }

    GhTaskbarWnd = CreateWindowExA(
        0,
        "Shell_TrayWnd",
        "",
        WS_POPUP | WS_VISIBLE,
        0,
        ScreenH - TASKBAR_HEIGHT,
        ScreenW,
        TASKBAR_HEIGHT,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (!GhTaskbarWnd)
    {
        return 1;
    }

    ShowWindow(GhTaskbarWnd, SW_SHOW);
    // UpdateWindow(GhTaskbarWnd);

    SetTimer(GhTaskbarWnd, IDT_SHELL_TICK, 1000, NULL);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return (INT)msg.wParam;
}