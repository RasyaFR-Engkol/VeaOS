#pragma once

#include <procbind.h>
#include <veastatus.h>
#include <psdk/msg.h>
#include <psdk/virtkey.h>
#include <psdk/winuser.h>
#include <userapi.h>

/* Deklarasi WndProc kontrol bawaan (diimplementasikan di controls.c) */
LRESULT CALLBACK StaticWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK EditWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK ButtonWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

/* Dipanggil saat startup userapi.so sebelum WinMain berjalan */
VOID UserapiRegisterSystemControls(VOID);

