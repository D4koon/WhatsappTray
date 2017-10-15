/*
*
* WhatsappTray
* Copyright (C) 1998-2017  Sebastian Amann, Nikolay Redko, J.D. Purcell
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
*/

#include <windows.h>
#include "WhatsappTray.h"
#include "resource.h"
#include <Shlobj.h>
#include <Strsafe.h>

#include "ApplicationData.h"
#include "Registry.h"

#undef MODULE_NAME
#define MODULE_NAME "[WhatsappTray] "

#define MAXTRAYITEMS 64

static HINSTANCE _hInstance;
static HMODULE _hLib;
static HWND _hwndWhatsappTray;
static HWND _hwndItems[MAXTRAYITEMS];
static HWND _hwndForMenu;
static HWND hwndWhatsapp;
static bool closeToTray;

/*
* @brief If true the 'launch on windows startup'-feature is on.
*/
static bool launchOnWindowsStartup;
static ApplicationData applicationData;
static Registry registry;

HWND startWhatsapp();
bool setHook();
void setLaunchOnWindowsStartupSetting(bool value);

int FindInTray(HWND hwnd) {
	for (int i = 0; i < MAXTRAYITEMS; i++) {
		if (_hwndItems[i] == hwnd) return i;
	}
	return -1;
}

HICON GetWindowIcon(HWND hwnd) {
	HICON icon;
	if (icon = (HICON)SendMessage(hwnd, WM_GETICON, ICON_SMALL, 0)) return icon;
	if (icon = (HICON)SendMessage(hwnd, WM_GETICON, ICON_BIG, 0)) return icon;
	if (icon = (HICON)GetClassLongPtr(hwnd, GCLP_HICONSM)) return icon;
	if (icon = (HICON)GetClassLongPtr(hwnd, GCLP_HICON)) return icon;
	return LoadIcon(NULL, IDI_WINLOGO);
}

static void AddToTray(int i) {
	NOTIFYICONDATA nid;
	ZeroMemory(&nid, sizeof(nid));
	nid.cbSize           = NOTIFYICONDATA_V2_SIZE;
	nid.hWnd             = _hwndWhatsappTray;
	nid.uID              = (UINT)i;
	nid.uFlags           = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	nid.uCallbackMessage = WM_TRAYCMD;
	nid.hIcon            = GetWindowIcon(_hwndItems[i]);
	GetWindowText(_hwndItems[i], nid.szTip, sizeof(nid.szTip) / sizeof(nid.szTip[0]));
	nid.uVersion         = NOTIFYICON_VERSION;
	Shell_NotifyIcon(NIM_ADD, &nid);
	Shell_NotifyIcon(NIM_SETVERSION, &nid);
}

static void AddWindowToTray(HWND hwnd) {
	int i = FindInTray(NULL);
	if (i == -1) return;
	_hwndItems[i] = hwnd;
	AddToTray(i);
}

static void MinimizeWindowToTray(HWND hwnd) {
	// Don't minimize MDI child windows
	if ((UINT)GetWindowLongPtr(hwnd, GWL_EXSTYLE) & WS_EX_MDICHILD) return;

	// If hwnd is a child window, find parent window (e.g. minimize button in
	// Office 2007 (ribbon interface) is in a child window)
	if ((UINT)GetWindowLongPtr(hwnd, GWL_STYLE) & WS_CHILD) {
		hwnd = GetAncestor(hwnd, GA_ROOT);
	}

	// Add icon to tray if it's not already there
	if (FindInTray(hwnd) == -1) {
		AddWindowToTray(hwnd);
	}

	// Hide window
	ShowWindow(hwnd, SW_HIDE);
}

static void RemoveFromTray(int i) {
	NOTIFYICONDATA nid;
	ZeroMemory(&nid, sizeof(nid));
	nid.cbSize = NOTIFYICONDATA_V2_SIZE;
	nid.hWnd   = _hwndWhatsappTray;
	nid.uID    = (UINT)i;
	Shell_NotifyIcon(NIM_DELETE, &nid);
}

static void RemoveWindowFromTray(HWND hwnd) {
	int i = FindInTray(hwnd);
	if (i == -1) return;
	RemoveFromTray(i);
	_hwndItems[i] = 0;
}

static void RestoreWindowFromTray(HWND hwnd) {
	ShowWindow(hwnd, SW_RESTORE);
	SetForegroundWindow(hwnd);
	RemoveWindowFromTray(hwnd);

}

static void CloseWindowFromTray(HWND hwnd) {
	// Use PostMessage to avoid blocking if the program brings up a dialog on exit.
	// Also, Explorer windows ignore WM_CLOSE messages from SendMessage.
	PostMessage(hwnd, WM_CLOSE, 0, 0);

	Sleep(50);
	if (IsWindow(hwnd)) Sleep(50);

	if (!IsWindow(hwnd)) {
		// Closed successfully
		RemoveWindowFromTray(hwnd);
	}
}

void RefreshWindowInTray(HWND hwnd) {
	int i = FindInTray(hwnd);
	if (i == -1) return;
	if (!IsWindow(hwnd) || IsWindowVisible(hwnd)) {
		RemoveWindowFromTray(hwnd);
	}
	else {
		NOTIFYICONDATA nid;
		ZeroMemory(&nid, sizeof(nid));
		nid.cbSize = NOTIFYICONDATA_V2_SIZE;
		nid.hWnd   = _hwndWhatsappTray;
		nid.uID    = (UINT)i;
		nid.uFlags = NIF_TIP;
		GetWindowText(hwnd, nid.szTip, sizeof(nid.szTip) / sizeof(nid.szTip[0]));
		Shell_NotifyIcon(NIM_MODIFY, &nid);
	}
}

/*
 * Create the rightclick-menue.
 */
void ExecuteMenu() {
	HMENU hMenu;
	POINT point;

	hMenu = CreatePopupMenu();
	if (!hMenu) {
		MessageBox(NULL, L"Error creating menu.", L"WhatsappTray", MB_OK | MB_ICONERROR);
		return;
	}

	AppendMenu(hMenu, MF_STRING, IDM_ABOUT,   L"About WhatsappTray");
	// - Display options.

	// -- Close to Tray
	if (closeToTray)
	{
		AppendMenu(hMenu, MF_STRING, IDM_SETTING_CLOSE_TO_TRAY, L"Close to tray ☑");
	}
	else
	{
		AppendMenu(hMenu, MF_STRING, IDM_SETTING_CLOSE_TO_TRAY, L"Close to tray ☐");
	}

	// -- Launch on Windows startup.
	if (launchOnWindowsStartup)
	{
		AppendMenu(hMenu, MF_STRING, IDM_SETTING_LAUNCH_ON_WINDOWS_STARTUP, L"Launch on Windows startup ☑");
	}
	else
	{
		AppendMenu(hMenu, MF_STRING, IDM_SETTING_LAUNCH_ON_WINDOWS_STARTUP, L"Launch on Windows startup ☐");
	}
	
	AppendMenu(hMenu, MF_SEPARATOR, 0, NULL); //--------------
	AppendMenu(hMenu, MF_STRING, IDM_RESTORE, L"Restore Window");
	//AppendMenu(hMenu, MF_STRING, IDM_EXIT,    L"Exit WhatsappTray");
	// Make window closable by menueentry when closeToTray is active, because the closebutton doesn't work anymore...
	//if (closeToTray)
	{
		AppendMenu(hMenu, MF_STRING, IDM_CLOSE, L"Close Whatsapp");
	}
	
	GetCursorPos(&point);
	SetForegroundWindow(_hwndWhatsappTray);

	TrackPopupMenu(hMenu, TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_RIGHTALIGN | TPM_BOTTOMALIGN, point.x, point.y, 0, _hwndWhatsappTray, NULL);

	PostMessage(_hwndWhatsappTray, WM_USER, 0, 0);
	DestroyMenu(hMenu);
}

BOOL CALLBACK AboutDlgProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
	switch (Msg) {
		case WM_CLOSE:
			PostMessage(hWnd, WM_COMMAND, IDCANCEL, 0);
			break;
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDOK:
EndDialog(hWnd, TRUE);
break;
				case IDCANCEL:
					EndDialog(hWnd, FALSE);
					break;
			}
			break;
		default:
			return FALSE;
	}
	return TRUE;
}

LRESULT CALLBACK HookWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	//OutputDebugString(MODULE_NAME L"HookWndProc() - Message Received\n");

	switch (msg)
	{
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDM_ABOUT:
			DialogBox(_hInstance, MAKEINTRESOURCE(IDD_ABOUT), _hwndWhatsappTray, (DLGPROC)AboutDlgProc);
			break;
		case IDM_SETTING_CLOSE_TO_TRAY: {
			// Toggle the 'close to tray'-feature.
			// We have to first change the value and then unregister and register to set the ne value in the hook.
			closeToTray = !closeToTray;
			// Write data to persistant storage.
			applicationData.SetData(L"CLOSE_TO_TRAY", closeToTray);

			SendMessage(_hwndWhatsappTray, WM_REAPPLY_HOOK, 0, 0);
			break;
		}
		case IDM_SETTING_LAUNCH_ON_WINDOWS_STARTUP:
			// Toggle the 'launch on windows startup'-feature.
			setLaunchOnWindowsStartupSetting(!launchOnWindowsStartup);
			break;
		case IDM_RESTORE:
			RestoreWindowFromTray(_hwndForMenu);
			break;
		case IDM_EXIT:
			// Running WhatsappTray without Whatsapp makes no sence because if a new instance of Whatsapp is started, WhatsappTray would not hook it. Atleast not in the current implementation...
			DestroyWindow(_hwndWhatsappTray);
			break;
		case IDM_CLOSE:
			CloseWindowFromTray(_hwndForMenu);

			// Running WhatsappTray without Whatsapp makes no sence because if a new instance of Whatsapp is started, WhatsappTray would not hook it. Atleast not in the current implementation...
			DestroyWindow(_hwndWhatsappTray);
			break;
		}
		break;
	case WM_REAPPLY_HOOK:
		UnRegisterHook();
		hwndWhatsapp = FindWindow(NULL, WHATSAPP_CLIENT_NAME);
		setHook();
		break;
	case WM_ADDTRAY:
		OutputDebugString(MODULE_NAME L"WM_ADDTRAY\n");
		MinimizeWindowToTray((HWND)lParam);
		break;
	case WM_REMTRAY:
		RestoreWindowFromTray((HWND)lParam);
		break;
	case WM_REFRTRAY:
		RefreshWindowInTray((HWND)lParam);
		break;
	case WM_TRAYCMD:
		switch ((UINT)lParam) {
		case NIN_SELECT:
			RestoreWindowFromTray(_hwndItems[wParam]);
			break;
		case WM_CONTEXTMENU:
			_hwndForMenu = _hwndItems[wParam];
			ExecuteMenu();
			break;
		case WM_MOUSEMOVE:
			RefreshWindowInTray(_hwndItems[wParam]);
			break;
		}
		break;

	case WM_WHAHTSAPP_CLOSING:
		// If Whatsapp is closing we want to close WhatsappTray as well.
		OutputDebugString(MODULE_NAME L"WM_WHAHTSAPP_CLOSING\n");
		DestroyWindow(_hwndWhatsappTray);
		break;
	case WM_DESTROY:
		//MessageBox(NULL, L"HookWndProc() WM_DESTROY", L"WhatsappTray", MB_OK | MB_ICONINFORMATION);
		for (int i = 0; i < MAXTRAYITEMS; i++) {
			if (_hwndItems[i]) {
				RestoreWindowFromTray(_hwndItems[i]);
			}
		}
		UnRegisterHook();
		FreeLibrary(_hLib);
		PostQuitMessage(0);
		//OutputDebugString(MODULE_NAME L"QuitMessage posted.\n");
		break;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR szCmdLine, int iCmdShow)
{
	_hInstance = hInstance;

	// Read the settings from the persistan storage.
	closeToTray = applicationData.GetDataOrSetDefault(L"CLOSE_TO_TRAY", false);
	// Setup the settings for launch on windows startup.
	setLaunchOnWindowsStartupSetting(applicationData.GetDataOrSetDefault(L"LAUNCH_ON_WINDOWS_STARTUP", false));

	// Check if closeToTray was set per commandline. (this overrides the persistent storage-value.)
	if (strstr(szCmdLine, "--closeToTray"))
	{
		closeToTray = true;
		// Write data to persistant storage.
		applicationData.SetData(L"CLOSE_TO_TRAY", closeToTray);
	}
	if (!(_hLib = LoadLibrary(L"Hook.dll"))) {
		MessageBox(NULL, L"Error loading Hook.dll.", L"WhatsappTray", MB_OK | MB_ICONERROR);
		return 0;
	}

	if (startWhatsapp() == NULL) {
		return 0;
	}
	
	_hwndWhatsappTray = FindWindow(NAME, NAME);
	if (_hwndWhatsappTray) {
		if (strstr(szCmdLine, "--exit")) {
			SendMessage(_hwndWhatsappTray, WM_CLOSE, 0, 0);
		}
		else {
			//MessageBox(NULL, L"WhatsappTray is already running. Reapplying hook", L"WhatsappTray", MB_OK | MB_ICONINFORMATION);
			SendMessage(_hwndWhatsappTray, WM_REAPPLY_HOOK, 0, 0);
		}
		return 0;
	}
	
	if (setHook() == false)
	{
		return 0;
	}

	WNDCLASS wc;
	wc.style = 0;
	wc.lpfnWndProc = HookWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = NULL;
	wc.hCursor = NULL;
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = NAME;
	if (!RegisterClass(&wc)) {
		MessageBox(NULL, L"Error creating window class", L"WhatsappTray", MB_OK | MB_ICONERROR);
		return 0;
	}
	if (!(_hwndWhatsappTray = CreateWindow(NAME, NAME, WS_OVERLAPPED, 0, 0, 0, 0, (HWND)NULL, (HMENU)NULL, (HINSTANCE)hInstance, (LPVOID)NULL))) {
		MessageBox(NULL, L"Error creating window", L"WhatsappTray", MB_OK | MB_ICONERROR);
		return 0;
	}
	for (int i = 0; i < MAXTRAYITEMS; i++) {
		_hwndItems[i] = NULL;
	}

	MSG msg;
	while (IsWindow(_hwndWhatsappTray) && GetMessage(&msg, _hwndWhatsappTray, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}

HWND startWhatsapp()
{
	hwndWhatsapp = FindWindow(NULL, WHATSAPP_CLIENT_NAME);

	// The reason why i disabled this check is, when an folder is open with the name "WhatsApp" this fails because it finds the explorer-window with that name.
	//if (hwndWhatsapp == NULL)
	{
		// Start WhatsApp.
		wchar_t* startMenuPrograms;
		if (SHGetKnownFolderPath(FOLDERID_Programs, 0, NULL, &startMenuPrograms) != S_OK) {
			MessageBox(NULL, L"'Start Menu\\Programs' folder not found", L"WhatsappTray", MB_OK);
			return NULL;
		}
		wchar_t lnk[MAX_PATH];
		StringCchCopy(lnk, MAX_PATH, startMenuPrograms);
		CoTaskMemFree(startMenuPrograms);
		StringCchCat(lnk, MAX_PATH, L"\\WhatsApp\\WhatsApp.lnk");
		HINSTANCE hInst = ShellExecute(0, NULL, lnk, NULL, NULL, 1);
		if (hInst <= (HINSTANCE)32) {
			MessageBox(NULL, L"Error launching WhatsApp", L"WhatsappTray", MB_OK);
			return NULL;
		}

		// Wait for WhatsApp to be started.
		Sleep(100);
		for (int attemptN = 0; (hwndWhatsapp = FindWindow(NULL, WHATSAPP_CLIENT_NAME)) == NULL; ++attemptN) {
			if (attemptN > 120) {
				MessageBox(NULL, L"WhatsApp-Window not found.", L"WhatsappTray", MB_OK | MB_ICONERROR);
				return NULL;
			}
			Sleep(500);
		}
	}

	return hwndWhatsapp;
}

bool setHook()
{
	// Damit nicht alle Prozesse gehookt werde, verwende ich jetzt die ThreadID des WhatsApp-Clients.
	DWORD threadId = GetWindowThreadProcessId(hwndWhatsapp, NULL);
	if (threadId == NULL)
	{
		MessageBox(NULL, L"ThreadID of WhatsApp-Window not found.", L"WhatsappTray", MB_OK | MB_ICONERROR);
		return false;
	}

	if (RegisterHook(_hLib, threadId, closeToTray) == false)
	{
		MessageBox(NULL, L"Error setting hook procedure.", L"WhatsappTray", MB_OK | MB_ICONERROR);
		return false;
	}
}

/*
 * @brief Sets the 'launch on windows startup'-setting.
 */
void setLaunchOnWindowsStartupSetting(bool value)
{
	launchOnWindowsStartup = value;
	// Write data to persistant storage.
	applicationData.SetData(L"LAUNCH_ON_WINDOWS_STARTUP", value);

	if (value) {
		registry.RegisterProgram();
	}
	else {
		registry.UnregisterProgram();
	}
}
