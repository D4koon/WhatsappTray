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

#include "stdafx.h"
#include "WhatsappTray.h"
#include "resource.h"

#include "ApplicationData.h"
#include "Registry.h"
#include "WhatsAppApi.h"
#include "Helper.h"
#include "Logger.h"

#include <Shlobj.h>
#include <Strsafe.h>

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

int FindInTray(HWND hwnd)
{
	for (int i = 0; i < MAXTRAYITEMS; i++) {
		if (_hwndItems[i] == hwnd) return i;
	}
	return -1;
}

static void AddToTray(int i)
{
	NOTIFYICONDATA nid;
	ZeroMemory(&nid, sizeof(nid));
	nid.cbSize           = NOTIFYICONDATA_V2_SIZE;
	nid.hWnd             = _hwndWhatsappTray;
	nid.uID              = (UINT)i;
	nid.uFlags           = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	nid.uCallbackMessage = WM_TRAYCMD;
	nid.hIcon            = Helper::GetWindowIcon(_hwndItems[i]);
	GetWindowText(_hwndItems[i], nid.szTip, sizeof(nid.szTip) / sizeof(nid.szTip[0]));
	nid.uVersion         = NOTIFYICON_VERSION;
	Shell_NotifyIcon(NIM_ADD, &nid);
	Shell_NotifyIcon(NIM_SETVERSION, &nid);
}

static void AddWindowToTray(HWND hwnd)
{
	int i = FindInTray(NULL);
	if (i == -1) { return; }
	_hwndItems[i] = hwnd;
	AddToTray(i);
}

static void MinimizeWindowToTray(HWND hwnd)
{
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

static void RemoveFromTray(int i)
{
	NOTIFYICONDATA nid;
	ZeroMemory(&nid, sizeof(nid));
	nid.cbSize = NOTIFYICONDATA_V2_SIZE;
	nid.hWnd   = _hwndWhatsappTray;
	nid.uID    = (UINT)i;
	Shell_NotifyIcon(NIM_DELETE, &nid);
}

static void RemoveWindowFromTray(HWND hwnd)
{
	int i = FindInTray(hwnd);
	if (i == -1) return;
	RemoveFromTray(i);
	_hwndItems[i] = 0;
}

static void RestoreWindowFromTray(HWND hwnd)
{
	ShowWindow(hwnd, SW_RESTORE);
	SetForegroundWindow(hwnd);
	RemoveWindowFromTray(hwnd);
}

static void CloseWindowFromTray(HWND hwnd)
{
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

void RefreshWindowInTray(HWND hwnd)
{
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
		MessageBox(NULL, "Error creating menu.", "WhatsappTray", MB_OK | MB_ICONERROR);
		return;
	}

	AppendMenu(hMenu, MF_STRING, IDM_ABOUT, "About WhatsappTray");
	// - Display options.

	// -- Close to Tray
	if (closeToTray) {
		AppendMenu(hMenu, MF_CHECKED, IDM_SETTING_CLOSE_TO_TRAY, "Close to tray");
	}
	else {
		AppendMenu(hMenu, MF_UNCHECKED, IDM_SETTING_CLOSE_TO_TRAY, "Close to tray");
	}

	// -- Launch on Windows startup.
	if (launchOnWindowsStartup) {
		AppendMenu(hMenu, MF_CHECKED, IDM_SETTING_LAUNCH_ON_WINDOWS_STARTUP, "Launch on Windows startup");
	}
	else {
		AppendMenu(hMenu, MF_UNCHECKED, IDM_SETTING_LAUNCH_ON_WINDOWS_STARTUP, "Launch on Windows startup");
	}
	
	AppendMenu(hMenu, MF_SEPARATOR, 0, NULL); //--------------
	AppendMenu(hMenu, MF_STRING, IDM_RESTORE, "Restore Window");
	//AppendMenu(hMenu, MF_STRING, IDM_EXIT,    "Exit WhatsappTray");
	// Make window closable by menueentry when closeToTray is active, because the closebutton doesn't work anymore...
	//if (closeToTray)
	{
		AppendMenu(hMenu, MF_STRING, IDM_CLOSE, "Close Whatsapp");
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
	Logger::Info(MODULE_NAME "HookWndProc() - Message Received msg='0x%X'", msg);

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
			applicationData.SetData("CLOSE_TO_TRAY", closeToTray);

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
		Logger::Info(MODULE_NAME "WM_ADDTRAY");
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
		Logger::Info(MODULE_NAME "WM_WHAHTSAPP_CLOSING");
		DestroyWindow(_hwndWhatsappTray);
		break;
	case WM_DESTROY:
		//MessageBox(NULL, "HookWndProc() WM_DESTROY", "WhatsappTray", MB_OK | MB_ICONINFORMATION);
		for (int i = 0; i < MAXTRAYITEMS; i++) {
			if (_hwndItems[i]) {
				RestoreWindowFromTray(_hwndItems[i]);
			}
		}
		UnRegisterHook();
		FreeLibrary(_hLib);
		PostQuitMessage(0);
		Logger::Info(MODULE_NAME "QuitMessage posted.");
		break;
	case WM_INDEXED_DB_CHANGED:
		PlaySound((LPCTSTR)SND_ALIAS_SYSTEMEXIT, NULL, SND_ALIAS_ID);
		Logger::Info(MODULE_NAME "WM_INDEXED_DB_CHANGED");
		break;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_  HINSTANCE hPrevInstance, _In_  LPSTR lpCmdLine, _In_  int nShowCmd)
{
	Logger::Setup();

	_hInstance = hInstance;

	// Read the settings from the persistan storage.
	closeToTray = applicationData.GetDataOrSetDefault("CLOSE_TO_TRAY", false);
	// Setup the settings for launch on windows startup.
	setLaunchOnWindowsStartupSetting(applicationData.GetDataOrSetDefault("LAUNCH_ON_WINDOWS_STARTUP", false));

	// Check if closeToTray was set per commandline. (this overrides the persistent storage-value.)
	if (strstr(lpCmdLine, "--closeToTray"))
	{
		closeToTray = true;
		// Write data to persistant storage.
		applicationData.SetData("CLOSE_TO_TRAY", closeToTray);
	}
	if (!(_hLib = LoadLibrary("Hook.dll"))) {
		MessageBox(NULL, "Error loading Hook.dll.", "WhatsappTray", MB_OK | MB_ICONERROR);
		return 0;
	}

	if (startWhatsapp() == NULL) {
		return 0;
	}
	
	// Test if WhatsappTray is already running.
	_hwndWhatsappTray = FindWindow(NAME, NAME);
	if (_hwndWhatsappTray) {
		if (strstr(lpCmdLine, "--exit")) {
			SendMessage(_hwndWhatsappTray, WM_CLOSE, 0, 0);
		}
		else {
			//MessageBox(NULL, "WhatsappTray is already running. Reapplying hook", "WhatsappTray", MB_OK | MB_ICONINFORMATION);
			SendMessage(_hwndWhatsappTray, WM_REAPPLY_HOOK, 0, 0);
		}
		return 0;
	}

	if (setHook() == false) {
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
		MessageBox(NULL, "Error creating window class", "WhatsappTray", MB_OK | MB_ICONERROR);
		return 0;
	}
	if (!(_hwndWhatsappTray = CreateWindow(NAME, NAME, WS_OVERLAPPED, 0, 0, 0, 0, (HWND)NULL, (HMENU)NULL, (HINSTANCE)hInstance, (LPVOID)NULL))) {
		MessageBox(NULL, "Error creating window", "WhatsappTray", MB_OK | MB_ICONERROR);
		return 0;
	}
	for (int i = 0; i < MAXTRAYITEMS; i++) {
		_hwndItems[i] = NULL;
	}

	// Send a WM_INDEXED_DB_CHANGED-message when a new WhatsApp-message has arrived.
	WhatsAppApi::NotifyOnNewMessage([]() { PostMessageA(_hwndWhatsappTray, WM_INDEXED_DB_CHANGED, 0, 0); });

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
		wchar_t* startMenuProgramsBuffer;
		if (SHGetKnownFolderPath(FOLDERID_Programs, 0, NULL, &startMenuProgramsBuffer) != S_OK) {
			MessageBox(NULL, "'Start Menu\\Programs' folder not found", "WhatsappTray", MB_OK);
			return NULL;
		}
		std::string startMenuPrograms = Helper::ToString(startMenuProgramsBuffer);
		CoTaskMemFree(startMenuProgramsBuffer);

		TCHAR lnk[MAX_PATH];
		StringCchCopy(lnk, MAX_PATH, startMenuPrograms.c_str());
		StringCchCat(lnk, MAX_PATH, "\\WhatsApp\\WhatsApp.lnk");
		HINSTANCE hInst = ShellExecute(0, NULL, lnk, NULL, NULL, 1);
		if (hInst <= (HINSTANCE)32) {
			MessageBox(NULL, "Error launching WhatsApp", "WhatsappTray", MB_OK);
			return NULL;
		}

		// Wait for WhatsApp to be started.
		Sleep(100);
		for (int attemptN = 0; (hwndWhatsapp = FindWindow(NULL, WHATSAPP_CLIENT_NAME)) == NULL; ++attemptN) {
			if (attemptN > 120) {
				MessageBox(NULL, "WhatsApp-Window not found.", "WhatsappTray", MB_OK | MB_ICONERROR);
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
	DWORD processId;
	DWORD threadId = GetWindowThreadProcessId(hwndWhatsapp, &processId);
	if (threadId == NULL)
	{
		MessageBox(NULL, "ThreadID of WhatsApp-Window not found.", "WhatsappTray", MB_OK | MB_ICONERROR);
		return false;
	}

	if (RegisterHook(_hLib, threadId, closeToTray) == false)
	{
		MessageBox(NULL, "Error setting hook procedure.", "WhatsappTray", MB_OK | MB_ICONERROR);
		return false;
	}
	return true;
}

/*
 * @brief Sets the 'launch on windows startup'-setting.
 */
void setLaunchOnWindowsStartupSetting(bool value)
{
	launchOnWindowsStartup = value;
	// Write data to persistant storage.
	applicationData.SetData("LAUNCH_ON_WINDOWS_STARTUP", value);

	if (value) {
		registry.RegisterProgram();
	}
	else {
		registry.UnregisterProgram();
	}
}
