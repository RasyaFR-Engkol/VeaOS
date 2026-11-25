/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : main.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Vea Login Screen
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include "logon.h"
#include <ptfunc.h>
#include <rtl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 22-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file main.c
 * --------------------------------------------------------------------- */

#define WM_LOGON_SHELL_EXITED   (WM_APP + 1)
#define DEFAULT_SHELL_PATH      "D1P1:/VeaOS/SystemReserved/TestVeapiApp.elf"
#define DEFAULT_USER_DESKTOP    "Default"

#define ID_LBL_SAS_TITLE        201
#define ID_BTN_SAS_LOCK         202
#define ID_BTN_SAS_LOGOFF       203
#define ID_BTN_SAS_TASKMGR      204
#define ID_BTN_SAS_SHUTDOWN     205
#define ID_BTN_SAS_CANCEL       206

static HDESK   ghDeskDefault       = NULL;
static HDESK   ghDeskWinlogon      = NULL;
static HWND    ghLogonWnd          = NULL;
static HWND    ghSasWnd            = NULL;
static HWND    ghEditUser          = NULL;
static HWND    ghEditPass          = NULL;
static HWND    ghBtnLogin          = NULL;
static HWND    ghLblStatus         = NULL;
static HANDLE  ghShellProcess      = NULL;
static BOOLEAN gbIntentionalLogoff = FALSE;
static BOOLEAN gbSessionLocked     = FALSE;
static CHAR    gszActiveUser[64]   = { 0 };

static
ULONG
VEAPI
LogonShellWaitThread(
    IN PVOID Context
)
{
    HANDLE hProcess = (HANDLE)Context;

    if (hProcess != NULL)
    {
        VeaWaitForSingleObject(hProcess, FALSE, NULL);
        VeaClose(hProcess);
    }

    if (ghLogonWnd != NULL)
    {
        if (!PostMessageA(ghLogonWnd, WM_LOGON_SHELL_EXITED, 0, 0))
        {
            SendMessageA(ghLogonWnd, WM_LOGON_SHELL_EXITED, 0, 0);
        }
    }

    return (ULONG)VeaTerminateThread(NtCurrentThread(), STATUS_SUCCESS);
}

static
BOOLEAN
LogonSpawnOnDefaultDesktop(
    IN  PCHAR   ImagePath,
    OUT PHANDLE OutProcessHandle OPTIONAL
)
{
    ANSI_STRING ImageAnsi;
    ANSI_STRING DesktopAnsi;
    PRTL_USER_PROCESS_PARAMETERS ProcParams = NULL;
    RTL_USER_PROCESS_INFORMATION ProcInfo;
    VEASTATUS Status;

    if (OutProcessHandle != NULL)
    {
        *OutProcessHandle = NULL;
    }

    RtlInitAnsiString(&ImageAnsi, ImagePath);
    RtlInitAnsiString(&DesktopAnsi, DEFAULT_USER_DESKTOP);

    RtlZeroMemory(&ProcInfo, sizeof(ProcInfo));
    ProcInfo.Size = sizeof(ProcInfo);

    Status = RtlCreateProcessParameters(
        &ProcParams,
        &ImageAnsi,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        &DesktopAnsi,   /* Ikat proses anak ke Desktop 'Default' */
        NULL,
        NULL
    );
    if (!VEA_SUCCESS(Status) || !ProcParams)
    {
        return FALSE;
    }

    Status = RtlCreateUserProcess(
        &ImageAnsi,
        OBJ_CASE_INSENSITIVE,
        ProcParams,
        NULL,
        NULL,
        NULL,
        FALSE,
        NULL,
        NULL,
        &ProcInfo
    );

    RtlDestroyProcessParameters(ProcParams);

    if (!VEA_SUCCESS(Status))
    {
        return FALSE;
    }

    if (!ProcInfo.ProcessHandle || !ProcInfo.ThreadHandle)
    {
        if (ProcInfo.ThreadHandle != NULL)
        {
            VeaClose(ProcInfo.ThreadHandle);
        }
        if (ProcInfo.ProcessHandle != NULL)
        {
            VeaTerminateProcess(ProcInfo.ProcessHandle, STATUS_UNSUCCESSFUL);
            VeaClose(ProcInfo.ProcessHandle);
        }
        return FALSE;
    }

    Status = VeaResumeThread(ProcInfo.ThreadHandle, NULL);
    if (!VEA_SUCCESS(Status))
    {
        VeaTerminateProcess(ProcInfo.ProcessHandle, Status);
        VeaClose(ProcInfo.ThreadHandle);
        VeaClose(ProcInfo.ProcessHandle);
        return FALSE;
    }
    VeaClose(ProcInfo.ThreadHandle);

    if (OutProcessHandle != NULL)
    {
        *OutProcessHandle = ProcInfo.ProcessHandle;
    }
    else
    {
        /* Untuk proses fire-and-forget seperti Task Manager */
        VeaClose(ProcInfo.ProcessHandle);
    }

    return TRUE;
}

static
BOOLEAN
LogonLaunchUserShell(
    IN PCHAR ImagePath
)
{
    HANDLE hShell = NULL;
    HANDLE hWaitThread = NULL;
    VEASTATUS Status;

    if (!LogonSpawnOnDefaultDesktop(ImagePath, &hShell) || hShell == NULL)
    {
        return FALSE;
    }

    ghShellProcess = hShell;

    /* Pantau proses shell di thread terpisah agar message loop Winlogon tetap hidup */
    Status = RtlCreateUserThread(
        NtCurrentProcess(),
        NULL,
        FALSE,
        0,
        0,
        0,
        LogonShellWaitThread,
        ghShellProcess,
        &hWaitThread,
        NULL
    );
    if (!VEA_SUCCESS(Status) || hWaitThread == NULL)
    {
        VeaTerminateProcess(ghShellProcess, Status);
        VeaClose(ghShellProcess);
        ghShellProcess = NULL;
        return FALSE;
    }

    VeaClose(hWaitThread);
    return TRUE;
}

static
VOID
LogonPerformShutdown(
    VOID
)
{
    ShowWindow(ghSasWnd, SW_HIDE);
    ShowWindow(ghLogonWnd, SW_SHOW);
    SetWindowTextA(ghLblStatus, "Status: Shutdown VeaOS...");

    if (ghShellProcess != NULL)
    {
        gbIntentionalLogoff = TRUE;
        VeaTerminateProcess(ghShellProcess, STATUS_SUCCESS);
    }

    /* TODO: Hubungkan ke syscall shutdown kernel VeaOS, misal:
       VzShutdownSystem(ShutdownPowerOff); */
    SetWindowTextA(ghLblStatus, "Status: It is safe now for turning off process...");
}

static
VOID
LogonShowLoginScreen(
    IN PCHAR StatusText,
    IN BOOLEAN PreserveUser
)
{
    ShowWindow(ghSasWnd, SW_HIDE);

    if (!PreserveUser)
    {
        SetWindowTextA(ghEditUser, "");
    }
    SetWindowTextA(ghEditPass, "");
    SetWindowTextA(ghLblStatus, StatusText);

    ShowWindow(ghLogonWnd, SW_SHOW);

    if (ghDeskWinlogon != NULL)
    {
        SwitchDesktop(ghDeskWinlogon);
    }

    SetFocus(PreserveUser ? ghEditPass : ghEditUser);
}

static
VOID
LogonShowSasMenu(
    VOID
)
{
    if (ghDeskWinlogon != NULL)
    {
        SwitchDesktop(ghDeskWinlogon);
    }

    ShowWindow(ghLogonWnd, SW_HIDE);
    ShowWindow(ghSasWnd, SW_SHOW);
}

static
VOID
LogonAttemptSignIn(
    IN HWND hWnd
)
{
    CHAR UserBuf[64];
    CHAR PassBuf[64];

    UNREFERENCED_PARAMETER(hWnd);

    RtlZeroMemory(UserBuf, sizeof(UserBuf));
    RtlZeroMemory(PassBuf, sizeof(PassBuf));

    GetWindowTextA(ghEditUser, UserBuf, sizeof(UserBuf));
    GetWindowTextA(ghEditPass, PassBuf, sizeof(PassBuf));

    if (UserBuf[0] == '\0')
    {
        SetWindowTextA(ghLblStatus, "Status: Username tidak boleh kosong.");
        SetFocus(ghEditUser);
        return;
    }

    /* Jika sesi sedang dikunci, tolak username selain pemilik sesi aktif */
    if (gbSessionLocked && gszActiveUser[0] != '\0')
    {
        if (!RtlRawStringEqual(UserBuf, gszActiveUser))
        {
            RtlZeroMemory(PassBuf, sizeof(PassBuf));
            SetWindowTextA(ghEditPass, "");
            SetWindowTextA(ghLblStatus, "Status: Sesi ini dikunci oleh pengguna lain.");
            SetFocus(ghEditUser);
            return;
        }
    }

    if (RtlRawStringEqual(UserBuf, "rasya") && RtlRawStringEqual(PassBuf, "1234"))
    {
        RtlZeroMemory(PassBuf, sizeof(PassBuf));
        SetWindowTextA(ghEditPass, "");

        /* Cabang 1: Sesi sedang terkunci -> buka kunci dan balik ke Desktop Default */
        if (ghShellProcess != NULL)
        {
            gbSessionLocked = FALSE;
            SetWindowTextA(ghLblStatus, "Status: Sesi dibuka kembali.");
            if (ghDeskDefault != NULL)
            {
                SwitchDesktop(ghDeskDefault);
            }
            return;
        }

        /* Cabang 2: Login baru -> simpan identitas sesi, pindah ke Default, spawn shell */
        gbIntentionalLogoff = FALSE;
        gbSessionLocked = FALSE;
        RtlCopyMemory(gszActiveUser, UserBuf, sizeof(gszActiveUser));

        SetWindowTextA(ghLblStatus, "Status: Memuat sesi desktop...");
        if (ghDeskDefault != NULL)
        {
            SwitchDesktop(ghDeskDefault);
        }

        if (!LogonLaunchUserShell(DEFAULT_SHELL_PATH))
        {
            RtlZeroMemory(gszActiveUser, sizeof(gszActiveUser));
            LogonShowLoginScreen("Status: Gagal menjalankan shell user!", FALSE);
        }
    }
    else
    {
        RtlZeroMemory(PassBuf, sizeof(PassBuf));
        SetWindowTextA(ghLblStatus, "Status: Username atau password salah.");
        SetWindowTextA(ghEditPass, "");
        SetFocus(ghEditPass);
    }
}

LRESULT
CALLBACK
SasWndProc(
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
            CreateWindowExA(
                0, "STATIC", "Keamanan VeaOS - Pilih tindakan:",
                WS_CHILD | WS_VISIBLE,
                20, 16, 320, 22,
                hWnd, (HMENU)ID_LBL_SAS_TITLE, NULL, NULL
            );

            CreateWindowExA(
                0, "BUTTON", "Kunci Komputer (Lock)",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                20, 48, 320, 30,
                hWnd, (HMENU)ID_BTN_SAS_LOCK, NULL, NULL
            );

            CreateWindowExA(
                0, "BUTTON", "Keluar dari Sesi (Logoff)",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                20, 86, 320, 30,
                hWnd, (HMENU)ID_BTN_SAS_LOGOFF, NULL, NULL
            );

            CreateWindowExA(
                0, "BUTTON", "Task Manager",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                20, 124, 320, 30,
                hWnd, (HMENU)ID_BTN_SAS_TASKMGR, NULL, NULL
            );

            CreateWindowExA(
                0, "BUTTON", "Matikan Sistem (Shutdown)",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                20, 162, 320, 30,
                hWnd, (HMENU)ID_BTN_SAS_SHUTDOWN, NULL, NULL
            );

            CreateWindowExA(
                0, "BUTTON", "Batal",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                20, 204, 320, 28,
                hWnd, (HMENU)ID_BTN_SAS_CANCEL, NULL, NULL
            );

            return 0;
        }

        case WM_COMMAND:
        {
            WORD CtrlId = LOWORD(wParam);

            if (CtrlId == ID_BTN_SAS_LOCK)
            {
                gbSessionLocked = TRUE;
                SetWindowTextA(ghEditUser, gszActiveUser);
                LogonShowLoginScreen(
                    "Status: Sesi dikunci. Masukkan password untuk melanjutkan.",
                    TRUE
                );
                return 0;
            }
            else if (CtrlId == ID_BTN_SAS_LOGOFF)
            {
                gbIntentionalLogoff = TRUE;
                gbSessionLocked = FALSE;
                ShowWindow(ghSasWnd, SW_HIDE);

                if (ghShellProcess != NULL)
                {
                    VeaTerminateProcess(ghShellProcess, STATUS_SUCCESS);
                }
                else
                {
                    LogonShowLoginScreen("Status: Sesi berakhir.", FALSE);
                }
                return 0;
            }
            else if (CtrlId == ID_BTN_SAS_TASKMGR)
            {
                ShowWindow(ghSasWnd, SW_HIDE);
                ShowWindow(ghLogonWnd, SW_SHOW);

                // LogonSpawnOnDefaultDesktop(DEFAULT_TASKMGR_PATH, NULL);

                if (ghDeskDefault != NULL)
                {
                    SwitchDesktop(ghDeskDefault);
                }
                return 0;
            }
            else if (CtrlId == ID_BTN_SAS_SHUTDOWN)
            {
                LogonPerformShutdown();
                return 0;
            }
            else if (CtrlId == ID_BTN_SAS_CANCEL)
            {
                ShowWindow(ghSasWnd, SW_HIDE);
                ShowWindow(ghLogonWnd, SW_SHOW);

                if (ghDeskDefault != NULL)
                {
                    SwitchDesktop(ghDeskDefault);
                }
                return 0;
            }
            break;
        }

        case WM_CLOSE:
            /* Tutup menu SAS = Batal kembali ke sesi user */
            ShowWindow(ghSasWnd, SW_HIDE);
            ShowWindow(ghLogonWnd, SW_SHOW);
            if (ghDeskDefault != NULL)
            {
                SwitchDesktop(ghDeskDefault);
            }
            return 0;
    }

    return DefWindowProcA(hWnd, Msg, wParam, lParam);
}

LRESULT
CALLBACK
LgnWndProc(
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
            CreateWindowExA(
                0, "STATIC", "Selamat datang di VeaOS",
                WS_CHILD | WS_VISIBLE,
                20, 16, 340, 22,
                hWnd, (HMENU)ID_LBL_BANNER, NULL, NULL
            );

            CreateWindowExA(
                0, "STATIC", "Username:",
                WS_CHILD | WS_VISIBLE,
                20, 52, 100, 20,
                hWnd, (HMENU)ID_LBL_USER, NULL, NULL
            );
            ghEditUser = CreateWindowExA(
                0, "EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                125, 49, 230, 26,
                hWnd, (HMENU)ID_EDIT_USER, NULL, NULL
            );

            CreateWindowExA(
                0, "STATIC", "Password:",
                WS_CHILD | WS_VISIBLE,
                20, 88, 100, 20,
                hWnd, (HMENU)ID_LBL_PASS, NULL, NULL
            );
            ghEditPass = CreateWindowExA(
                0, "EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                125, 85, 230, 26,
                hWnd, (HMENU)ID_EDIT_PASS, NULL, NULL
            );

            ghBtnLogin = CreateWindowExA(
                0, "BUTTON", "Masuk",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                125, 126, 105, 30,
                hWnd, (HMENU)ID_BTN_LOGIN, NULL, NULL
            );
            CreateWindowExA(
                0, "BUTTON", "Matikan...",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                250, 126, 105, 30,
                hWnd, (HMENU)ID_BTN_SHUTDOWN, NULL, NULL
            );

            ghLblStatus = CreateWindowExA(
                0, "STATIC", "Status: Silakan masukkan akun Anda.",
                WS_CHILD | WS_VISIBLE,
                20, 172, 340, 20,
                hWnd, (HMENU)ID_LBL_STATUS, NULL, NULL
            );

            SetFocus(ghEditUser);
            return 0;
        }

        case WM_COMMAND:
        {
            WORD CtrlId = LOWORD(wParam);
            if (CtrlId == ID_BTN_LOGIN)
            {
                LogonAttemptSignIn(hWnd);
                return 0;
            }
            else if (CtrlId == ID_BTN_SHUTDOWN)
            {
                LogonPerformShutdown();
                return 0;
            }
            break;
        }

        case WM_LOGON_SHELL_EXITED:
        {
            ghShellProcess = NULL;

            /* Jika bukan logoff eksplisit, anggap shell crash -> auto-restart */
            if (!gbIntentionalLogoff)
            {
                if (LogonLaunchUserShell(DEFAULT_SHELL_PATH))
                {
                    if (!gbSessionLocked && ghDeskDefault != NULL)
                    {
                        SwitchDesktop(ghDeskDefault);
                    }
                    return 0;
                }

                /* Jika gagal restart shell, jatuhkan ke layar login */
                gbSessionLocked = FALSE;
                RtlZeroMemory(gszActiveUser, sizeof(gszActiveUser));
                LogonShowLoginScreen(
                    "Status: Shell berhenti dan gagal dimuat ulang.",
                    FALSE
                );
                return 0;
            }

            /* Sesi berakhir secara sengaja (Logoff) -> bersihkan sesi & kembali ke login */
            gbIntentionalLogoff = FALSE;
            gbSessionLocked = FALSE;
            RtlZeroMemory(gszActiveUser, sizeof(gszActiveUser));

            LogonShowLoginScreen(
                "Status: Sesi berakhir. Silakan masuk kembali.",
                FALSE
            );
            return 0;
        }

        case WM_SAS:
        {
            if (ghShellProcess == NULL)
            {
                /* Kondisi 1: Belum login -> pastikan fokus balik ke kotak Username */
                SetWindowTextA(ghLblStatus, "Status: SAS diterima. Silakan masukkan akun.");
                SetFocus(ghEditUser);
            }
            else if (gbSessionLocked)
            {
                /* Kondisi 2: Sesi sedang dikunci -> fokus ke kotak Password */
                SetFocus(ghEditPass);
            }
            else
            {
                /* Kondisi 3: User aktif di Desktop 'Default' -> buka menu VeaOS Security */
                LogonShowSasMenu();
            }
            return 0;
        }

        case WM_CLOSE:
            return 0;
    }

    return DefWindowProcA(hWnd, Msg, wParam, lParam);
}

INT
WINAPI
WinMain(
    IN HINSTANCE hInstance,
    IN HINSTANCE hPrevInstance,
    IN PSTR      lpCmdLine,
    IN INT       nCmdShow
)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    /* 1. Siapkan isolasi Desktop 'Default' vs 'Winlogon' sebelum bikin window apa pun */
    ghDeskDefault = OpenDesktopA(DEFAULT_USER_DESKTOP, 0, FALSE, VEA_GENERIC_ALL);
    if (ghDeskDefault == NULL)
    {
        ghDeskDefault = CreateDesktopA(
            DEFAULT_USER_DESKTOP,
            NULL,
            NULL,
            0,
            VEA_GENERIC_ALL,
            NULL
        );
    }

    ghDeskWinlogon = CreateDesktopA("Winlogon", NULL, NULL, 0, VEA_GENERIC_ALL, NULL);
    if (ghDeskWinlogon != NULL)
    {
        SetThreadDesktop(ghDeskWinlogon);
        SwitchDesktop(ghDeskWinlogon);
    }

    /* 2. Daftarkan window class untuk Login Screen dan VeaOS Security (SAS Menu) */
    WNDCLASSA wc;
    RtlZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc   = LgnWndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursorA(NULL, IDC_ARROW);
    wc.lpszClassName = "VeaLogonWindowClass";
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);

    if (!RegisterClassA(&wc))
    {
        return 1;
    }

    WNDCLASSA sasWc;
    RtlZeroMemory(&sasWc, sizeof(sasWc));
    sasWc.lpfnWndProc   = SasWndProc;
    sasWc.hInstance     = hInstance;
    sasWc.hCursor       = LoadCursorA(NULL, IDC_ARROW);
    sasWc.lpszClassName = "VeaSasSecurityClass";
    sasWc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);

    if (!RegisterClassA(&sasWc))
    {
        return 1;
    }

    INT DlgW = 380;
    INT DlgH = 235;
    INT PosX = (800 - DlgW) / 2;
    INT PosY = (600 - DlgH) / 2;

    ghLogonWnd = CreateWindowExA(
        0,
        "VeaLogonWindowClass",
        "VeaOS Security - Log On",
        WS_OVERLAPPED | WS_CAPTION | WS_VISIBLE,
        PosX, PosY, DlgW, DlgH,
        NULL, NULL, hInstance, NULL
    );
    if (!ghLogonWnd)
    {
        return 1;
    }

    INT SasW = 360;
    INT SasH = 275;
    INT SasX = (800 - SasW) / 2;
    INT SasY = (600 - SasH) / 2;

    ghSasWnd = CreateWindowExA(
        0,
        "VeaSasSecurityClass",
        "VeaOS SAS",
        WS_OVERLAPPED | WS_CAPTION,
        SasX, SasY, SasW, SasH,
        NULL, NULL, hInstance, NULL
    );
    if (!ghSasWnd)
    {
        return 1;
    }

    SetLogonNotifyWindow(ghLogonWnd);
    ShowWindow(ghLogonWnd, nCmdShow);

    /* 3. Message Loop + navigasi Tab & Enter untuk form dialog */
    MSG Msg;
    while (GetMessageA(&Msg, NULL, 0, 0) > 0)
    {
        if (Msg.message == WM_KEYDOWN)
        {
            if (Msg.wParam == VK_TAB)
            {
                if (Msg.hwnd == ghEditUser)
                {
                    SetFocus(ghEditPass);
                    continue;
                }
                else if (Msg.hwnd == ghEditPass)
                {
                    SetFocus(ghBtnLogin);
                    continue;
                }
                else if (Msg.hwnd == ghBtnLogin)
                {
                    SetFocus(ghEditUser);
                    continue;
                }
            }
            else if (Msg.wParam == VK_RETURN)
            {
                if (Msg.hwnd == ghEditUser || Msg.hwnd == ghEditPass || Msg.hwnd == ghBtnLogin)
                {
                    LogonAttemptSignIn(ghLogonWnd);
                    continue;
                }
            }
        }

        TranslateMessage(&Msg);
        DispatchMessageA(&Msg);
    }

    return (INT)Msg.wParam;
}

/* EOF */