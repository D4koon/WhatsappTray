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
#include "SharedDefines.h"
#include "resource.h"

#include "AppData.h"
#include "Registry.h"
#include "WhatsAppApi.h"
#include "TrayManager.h"
#include "AboutDialog.h"
#include "WinSockServer.h"
#include "Helper.h"
#include "Logger.h"

#include <windows.h>
#include <Strsafe.h>
#include <filesystem>

namespace fs = std::experimental::filesystem;

#ifdef _DEBUG
constexpr auto CompileConfiguration = "Debug";
#else
constexpr auto CompileConfiguration = "Release";
#endif

#undef MODULE_NAME
#define MODULE_NAME "WhatsappTray"

static HINSTANCE _hInstance = NULL;
static HWND _hwndWhatsappTray = NULL;
static HWND _hwndForMenu = NULL;
static HWND _hwndWhatsapp = NULL;

static HHOOK _hWndProc = NULL;
static HMODULE _hLib = NULL;

static int _messagesSinceMinimize = 0;

static char _loggerPort[] = LOGGER_PORT;
static std::thread _winsockThread;

static std::unique_ptr<TrayManager> _trayManager;

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static HWND FindWhatsapp();
static HWND StartWhatsapp();
static void TryClosePreviousWhatsappTrayInstance();
static bool CreateWhatsappTrayWindow();
static bool SetHook();
static void UnRegisterHook();
static void SetLaunchOnWindowsStartupSetting(bool value);

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
	_hInstance = hInstance;

	Logger::Setup();

	// For reading data from *.lnk files (shortcut files). See Helper::ResolveLnk()
	CoInitialize(nullptr);

	Logger::Info(MODULE_NAME "::WinMain(): Starting WhatsappTray %s in %s CompileConfiguration.", Helper::GetProductAndVersion().c_str(), CompileConfiguration);

	// Initialize WinSock-server, which is used to send log-messages from WhatsApp-hook to WhatsappTray
	SocketNotifyOnNewMessage([](std::string message) {
		Logger::Info("Hook> %s", message.c_str());
	});

	_winsockThread = std::thread(SocketStart, _loggerPort);

	WhatsAppApi::Init();

	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;

	// Initialize GDI+.
	Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	// Setup the settings for launch on windows startup.
	SetLaunchOnWindowsStartupSetting(AppData::LaunchOnWindowsStartup.Get());

	// Check if closeToTray was set per commandline. (this overrides the persistent storage-value.)
	if (strstr(lpCmdLine, "--closeToTray")) {
		AppData::CloseToTray.Set(true);
	}

	/* If WhatsappTray window is already open close it */
	TryClosePreviousWhatsappTrayInstance();

	if (CreateWhatsappTrayWindow() == false) {
		return 0;
	}

	if (!_hwndWhatsappTray) {
		MessageBox(NULL, "Error creating window", "WhatsappTray", MB_OK | MB_ICONERROR);
		return 0;
	}

	if (AppData::StartMinimized.Get()) {
		Logger::Info(MODULE_NAME "::WinMain() - Prepare for starting minimized.");

		WhatsAppApi::NotifyOnFullInit([]() {
			Logger::Info(MODULE_NAME "::WinMain() - NotifyOnFullInit");
			Sleep(2000);
			_trayManager->MinimizeWindowToTray(_hwndWhatsapp);
			// Remove event after the first execution
			WhatsAppApi::NotifyOnFullInit(NULL);
		});
	}

	/* LoadLibrary()triggers DllMain with DLL_PROCESS_ATTACH. This is used in WhatsappTray.cpp
	 * to prevent tirggering the wndProc redirect for WhatsappTray we need to detect if this happend.
	 * the easiest/best way to detect that is by setting a enviroment variable before LoadLibrary() */
	_putenv_s(WHATSAPPTRAY_LOAD_LIBRARY_TEST_ENV_VAR, WHATSAPPTRAY_LOAD_LIBRARY_TEST_ENV_VAR_VALUE);
	if (!(_hLib = LoadLibrary("Hook.dll"))) {
		Logger::Error(MODULE_NAME "::WinMain() - Error loading Hook.dll.");
		MessageBox(NULL, "Error loading Hook.dll.", "WhatsappTray", MB_OK | MB_ICONERROR);
		return 0;
	}
	
	if (StartWhatsapp() == nullptr) {
		MessageBoxA(NULL, (std::string("Error launching WhatsApp. Examine the logs for details")).c_str(), "WhatsappTray", MB_OK);
		return 0;
	}

	if (SetHook() == false) {
		Logger::Error(MODULE_NAME "::WinMain() - Error setting hook.");
		return 0;
	}

	_trayManager = std::make_unique<TrayManager>(_hwndWhatsappTray);
	_trayManager->AddWindowToTray(_hwndWhatsapp);

	// Send a WM_WHATSAPP_API_NEW_MESSAGE-message when a new WhatsApp-message has arrived.
	WhatsAppApi::NotifyOnNewMessage([]() { PostMessageA(_hwndWhatsappTray, WM_WHATSAPP_API_NEW_MESSAGE, 0, 0); });

	MSG msg;
	while (IsWindow(_hwndWhatsappTray) && GetMessage(&msg, _hwndWhatsappTray, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	Gdiplus::GdiplusShutdown(gdiplusToken);
	CoUninitialize();

	return 0;
}

/**
 * Start WhatsApp.
 */
HWND StartWhatsapp()
{
	fs::path waStartPath = Helper::Utf8ToWide(AppData::WhatsappStartpathGet());
	std::string waStartPathString;
	if (waStartPath.is_relative()) {
		fs::path appPath = Helper::GetApplicationFilePath();
		auto combinedPath = appPath / waStartPath;

		Logger::Info(MODULE_NAME "StartWhatsapp() - Starting WhatsApp from combinedPath:%s", combinedPath.string().c_str());

		// Shorten the path by converting to absoltue path.
		auto combinedPathCanonical = fs::canonical(combinedPath);
		waStartPathString = combinedPathCanonical.string();
	} else {
		waStartPathString = waStartPath.string();
	}

	Logger::Info(MODULE_NAME "::StartWhatsapp() - Starting WhatsApp from canonical-path:'" + waStartPathString + "'");

	auto waStartPathStringExtension = waStartPathString.substr(waStartPathString.size() - 3);
	if (waStartPathStringExtension.compare("lnk") == 0)
	{
		waStartPathString = Helper::ResolveLnk(_hwndWhatsappTray, waStartPathString.c_str());
		Logger::Info(MODULE_NAME "::StartWhatsapp() - Resolved .lnk (Shortcut) to:'" + waStartPathString + "'");
	}

	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	// Add quotes so a path with spaces can be used
	auto cmdLine = ("\"" + waStartPathString + "\"");

	// Start the process. 
	if (!CreateProcess(NULL,    // No module name (use command line)
		(LPSTR)cmdLine.c_str(), // Command line
		NULL,                   // Process handle not inheritable
		NULL,                   // Thread handle not inheritable
		FALSE,                  // Set handle inheritance to FALSE
		0,                      // No creation flags
		NULL,                   // Use parent's environment block
		NULL,                   // Use parent's starting directory 
		&si,                    // Pointer to STARTUPINFO structure
		&pi)                    // Pointer to PROCESS_INFORMATION structure
		)
	{
		Logger::Info(MODULE_NAME "::StartWhatsapp() - CreateProcess failed(%d).", GetLastError());
		return nullptr;
	}

	// Wait a maximum of 2000ms for the process to go into idle.
	auto result = WaitForInputIdle(pi.hProcess, 2000);
	if (result != 0)
	{
		Logger::Info(MODULE_NAME "::StartWhatsapp() - WaitForInputIdle failed.");
		return nullptr;
	}

	// We want to find the window-handle of WhatsApp
	// - The hard thing here is to find the window-handle even if WhatsApp was already running when CreateProcess() was called.
	//   This means we can not really rely on the data (STARTUPINFO and PROCESS_INFORMATION) from CreateProcess()
	// - Normally the exe referenced by the shortcut spawns the real program(exe).
	//   So it is necessary to find the child-process of the original process.

	 // Wait for WhatsApp to be started.
	Sleep(100);
	for (int attemptN = 0; (_hwndWhatsapp = FindWhatsapp()) == NULL; ++attemptN) {
		if (attemptN > 60) {
			MessageBoxA(NULL, "WhatsApp-Window not found.", "WhatsappTray", MB_OK | MB_ICONERROR);
			return nullptr;
		}
		Logger::Info(MODULE_NAME "StartWhatsapp() - WhatsApp-Window not found. Wait 500ms and retry.");
		Sleep(500);
	}

	Logger::Info(MODULE_NAME "StartWhatsapp() - WhatsApp-Window found.");

	return _hwndWhatsapp;
}

/**
 * Search for the WhatsApp window.
 * Checks if it is the correct window:
 * 1. Get the path to the binary(exe) for the window with "WhatsApp" in the title
 * 2. Match it with the appData-setting.
 */
HWND FindWhatsapp()
{
	HWND currentWindow = NULL;
	while (true) {
		currentWindow = FindWindowExA(NULL, currentWindow, NULL, WHATSAPP_CLIENT_NAME);
		if (currentWindow == NULL) {
			return NULL;
		}

		DWORD processId;
		DWORD threadId = GetWindowThreadProcessId(currentWindow, &processId);

		auto filepath = Helper::GetFilepathFromProcessID(processId);

		Logger::Info(MODULE_NAME "::FindWhatsapp() - Filepath is: '%s'", filepath.c_str());

		std::wstring filenameFromWindow = Helper::GetFilenameFromPath(Helper::Utf8ToWide(filepath));
		std::wstring whatsappStartpathWide = Helper::Utf8ToWide(AppData::WhatsappStartpathGet());
		std::wstring filenameFromSettings = Helper::GetFilenameFromPath(whatsappStartpathWide);

		// NOTE: I do not compare the extension because when i start from an link, the name is WhatsApp.lnk whicht does not match the WhatsApp.exe
		// This could be improved by reading the real value from the .lnk but i think this should be fine for now.
		if (filenameFromWindow.compare(filenameFromSettings) != 0) {
			Logger::Error(MODULE_NAME "::FindWhatsapp() - Filenames from window and from settings do not match.");
			continue;
		}

		Logger::Info(MODULE_NAME "::FindWhatsapp() - Found match.");
		break;
	}

	return currentWindow;
}

/**
 * Try to close old WhatsappTray instance
 */
void TryClosePreviousWhatsappTrayInstance()
{
	// Test if WhatsappTray is already running.
	// NOTE: This also matches the class-name of the window so we can be sure its our window and not for example an explorer-window with this name.
	_hwndWhatsappTray = FindWindow(NAME, NAME);
	if (_hwndWhatsappTray) {
		Logger::Error(MODULE_NAME "::WinMain() - Found an already open instance of WhatsappTray. Trying to close the other instance.");
		Logger::Error(MODULE_NAME "::WinMain() - If this error persists, try to close the other instance by hand using for example the taskmanager.");
		//if (strstr(lpCmdLine, "--exit")) {
		SendMessage(_hwndWhatsappTray, WM_CLOSE, 0, 0);
		//} else {
		//	//MessageBox(NULL, "WhatsappTray is already running. Reapplying hook", "WhatsappTray", MB_OK | MB_ICONINFORMATION);
		//	SendMessage(_hwndWhatsappTray, WM_REAPPLY_HOOK, 0, 0);
		//}
		//return 0;

#pragma WARNING("It would be best to wait her a bit and check if it is still active. And if it is still active shoot it down.")
	}
}

/**
 * Create a window.
 * This window will be mainly used to receive messages.
 */
bool CreateWhatsappTrayWindow()
{
	WNDCLASS wc;
	wc.style = 0;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = _hInstance;
	wc.hIcon = NULL;
	wc.hCursor = NULL;
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = NAME;

	if (!RegisterClass(&wc)) {
		MessageBox(NULL, "Error creating window class", "WhatsappTray", MB_OK | MB_ICONERROR);
		return false;
	}

	_hwndWhatsappTray = CreateWindow(NAME, NAME, WS_OVERLAPPED, 0, 0, 0, 0, (HWND)NULL, (HMENU)NULL, _hInstance, (LPVOID)NULL);
	return true;
}

/*
 * Create the rightclick-menue.
 */
void ExecuteMenu()
{
	HMENU hMenu = CreatePopupMenu();
	if (!hMenu) {
		Logger::Error(MODULE_NAME "::ExecuteMenu() - Error creating menu.");
		MessageBox(NULL, "Error creating menu.", "WhatsappTray", MB_OK | MB_ICONERROR);
		return;
	}

	AppendMenu(hMenu, MF_STRING, IDM_ABOUT, "About WhatsappTray");
	// - Display options.

	// -- Close to Tray
	if (AppData::CloseToTray.Get()) {
		AppendMenu(hMenu, MF_CHECKED, IDM_SETTING_CLOSE_TO_TRAY, "Close to tray");
	} else {
		AppendMenu(hMenu, MF_UNCHECKED, IDM_SETTING_CLOSE_TO_TRAY, "Close to tray");
	}

	// -- Launch on Windows startup.
	if (AppData::LaunchOnWindowsStartup.Get()) {
		AppendMenu(hMenu, MF_CHECKED, IDM_SETTING_LAUNCH_ON_WINDOWS_STARTUP, "Launch on Windows startup");
	} else {
		AppendMenu(hMenu, MF_UNCHECKED, IDM_SETTING_LAUNCH_ON_WINDOWS_STARTUP, "Launch on Windows startup");
	}

	// -- Start minimized.
	if (AppData::StartMinimized.Get()) {
		AppendMenu(hMenu, MF_CHECKED, IDM_SETTING_START_MINIMIZED, "Start minimized");
	} else {
		AppendMenu(hMenu, MF_UNCHECKED, IDM_SETTING_START_MINIMIZED, "Start minimized");
	}

	// -- Start minimized.
	if (AppData::ShowUnreadMessages.Get()) {
		AppendMenu(hMenu, MF_CHECKED, IDM_SETTING_SHOW_UNREAD_MESSAGES, "Show Unread Messages (experimental)");
	} else {
		AppendMenu(hMenu, MF_UNCHECKED, IDM_SETTING_SHOW_UNREAD_MESSAGES, "Show Unread Messages (experimental)");
	}

	AppendMenu(hMenu, MF_SEPARATOR, 0, NULL); //--------------

	AppendMenu(hMenu, MF_STRING, IDM_RESTORE, "Restore Window");
	AppendMenu(hMenu, MF_STRING, IDM_CLOSE, "Close Whatsapp");

	POINT point;
	GetCursorPos(&point);
	SetForegroundWindow(_hwndWhatsappTray);

	TrackPopupMenu(hMenu, TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_RIGHTALIGN | TPM_BOTTOMALIGN, point.x, point.y, 0, _hwndWhatsappTray, NULL);

	PostMessage(_hwndWhatsappTray, WM_USER, 0, 0);
	DestroyMenu(hMenu);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	//Logger::Info(MODULE_NAME "::WndProc() - Message Received msg='0x%X'", msg);

	switch (msg) {
	case WM_COMMAND: {
		switch (LOWORD(wParam)) {
		case IDM_ABOUT: {
			AboutDialog::Create(_hInstance, _hwndWhatsappTray);
		} break;
		case IDM_SETTING_CLOSE_TO_TRAY: {
			// Toggle the 'close to tray'-feature.
			// We have to first change the value and then unregister and register to set the ne value in the hook.
			AppData::CloseToTray.Set(!AppData::CloseToTray.Get());

			SendMessage(_hwndWhatsappTray, WM_REAPPLY_HOOK, 0, 0);
		} break;
		case IDM_SETTING_LAUNCH_ON_WINDOWS_STARTUP: {
			// Toggle the 'launch on windows startup'-feature.
			SetLaunchOnWindowsStartupSetting(!AppData::LaunchOnWindowsStartup.Get());
		} break;
		case IDM_SETTING_START_MINIMIZED: {
			// Toggle the 'start minimized'-feature.
			AppData::StartMinimized.Set(!AppData::StartMinimized.Get());
		} break;
		case IDM_SETTING_SHOW_UNREAD_MESSAGES: {
			// Toggle the 'ShowUnreadMessages'-feature.
			AppData::ShowUnreadMessages.Set(!AppData::ShowUnreadMessages.Get());
		} break;
		case IDM_RESTORE: {
			Logger::Info(MODULE_NAME "::WndProc() - IDM_RESTORE");
			_trayManager->RestoreWindowFromTray(_hwndForMenu);
		} break;
		case IDM_CLOSE: {
			_trayManager->CloseWindowFromTray(_hwndForMenu);

			// Running WhatsappTray without Whatsapp makes no sence because if a new instance of Whatsapp is started, WhatsappTray would not hook it. Atleast not in the current implementation...
			DestroyWindow(_hwndWhatsappTray);
		} break;
		}
	} break;
	case WM_REAPPLY_HOOK: {
		UnRegisterHook();
		_hwndWhatsapp = FindWhatsapp();
		SetHook();
	}break;
	case WM_WA_MINIMIZE_BUTTON_PRESSED: {
		Logger::Info(MODULE_NAME "::WndProc() - WM_WA_MINIMIZE_BUTTON_PRESSED");

		if (AppData::CloseToTray.Get() == true) {
			Logger::Info(MODULE_NAME "::WndProc() - Close to tray is true => Minimize WhatsApp to taskbar.");
		} else {
			Logger::Info(MODULE_NAME "::WndProc() - Close to tray is false => Minimize WhatsApp to tray.");
			_trayManager->MinimizeWindowToTray(_hwndWhatsapp);
		}
	} break;
	case WM_WA_CLOSE_BUTTON_PRESSED: {
		Logger::Info(MODULE_NAME "::WndProc() - WM_WA_CLOSE_BUTTON_PRESSED");

		if (AppData::CloseToTray.Get() == true) {
			Logger::Info(MODULE_NAME "::WndProc() - Close to tray is true => Minimize WhatsApp to tray.");
			_trayManager->MinimizeWindowToTray(_hwndWhatsapp);
		} else {
			Logger::Info(MODULE_NAME "::WndProc() - Close to tray is false => Close WhatsApp and WhatsappTray.");
			_trayManager->CloseWindowFromTray(_hwndWhatsapp);
		}
	} break;
	case WM_TRAYCMD: {
#pragma WARNING(Move into TrayManager. Problem is executeMenue...)
		switch (static_cast<UINT>(lParam)) {
		case NIN_SELECT: {
			_trayManager->RestoreFromTray(wParam);
		} break;
		case WM_CONTEXTMENU: {
			_hwndForMenu = _trayManager->GetHwndFromIndex(wParam);
			ExecuteMenu();
		} break;
		}
	}break;
	case WM_WHAHTSAPP_CLOSING: {
		// If Whatsapp is closing we want to close WhatsappTray as well.
		Logger::Info(MODULE_NAME "::WndProc() - WM_WHAHTSAPP_CLOSING");
		DestroyWindow(_hwndWhatsappTray);
	} break;
	case WM_WHATSAPP_TO_WHATSAPPTRAY_RECEIVED_WM_CLOSE: {
		Logger::Info(MODULE_NAME "::" + std::string(__func__) + "() - WM_WHATSAPP_TO_WHATSAPPTRAY_RECEIVED_WM_CLOSE received");
		// This message means that WhatsApp received a WM_CLOSE-message.
		// This happens when alt + f4 is pressed.

		if (AppData::CloseToTray.Get()) {
			// Close to tray is activated => Minimize Whatsapp to tray.
			_trayManager->MinimizeWindowToTray(_hwndWhatsapp);
		}
		else {
			// Close to tray is not activated => Close Whatsapp.

			// NOTE: WM_WHATSAPPTRAY_TO_WHATSAPP_SEND_WM_CLOSE is a special message i made because WM_CLOSE is always blocked by the hook.
			PostMessage(_hwndWhatsapp, WM_WHATSAPPTRAY_TO_WHATSAPP_SEND_WM_CLOSE, 0, 0);
			Logger::Info(MODULE_NAME "::WndProc() - WM_WHATSAPPTRAY_TO_WHATSAPP_SEND_WM_CLOSE sent");
		}

	} break;
	case WM_DESTROY: {
		Logger::Info(MODULE_NAME "::WndProc() - WM_DESTROY");

		_trayManager->RestoreAllWindowsFromTray();
		_trayManager->RemoveTrayIcon(_hwndWhatsapp);

		UnRegisterHook();
		FreeLibrary(_hLib);

		// Stop winsock-server and wait for it to cleanup and finish
		SocketStopServer();
		_winsockThread.join();

		PostQuitMessage(0);
		Logger::Info(MODULE_NAME "::WndProc() - QuitMessage posted.");
	} break;
	case WM_WHATSAPP_API_NEW_MESSAGE: {

		Logger::Info(MODULE_NAME "::WndProc() - WM_WHATSAPP_API_NEW_MESSAGE");
		_messagesSinceMinimize++;

		if (AppData::ShowUnreadMessages.Get()) {
			char messagesSinceMinimizeBuffer[20] = { 0 };
			snprintf(messagesSinceMinimizeBuffer, sizeof(messagesSinceMinimizeBuffer), "%d", _messagesSinceMinimize);
			_trayManager->SetIcon(_hwndWhatsapp, messagesSinceMinimizeBuffer);
		}

	} break;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool SetHook()
{
	Logger::Info(MODULE_NAME "::SetHook()");

	// Damit nicht alle Prozesse gehookt werde, verwende ich jetzt die ThreadID des WhatsApp-Clients.
	DWORD processId;
	DWORD threadId = GetWindowThreadProcessId(_hwndWhatsapp, &processId);
	if (threadId == NULL) {
		MessageBox(NULL, "ThreadID of WhatsApp-Window not found.", "WhatsappTray", MB_OK | MB_ICONERROR);
		return false;
	}

	// NOTE: To see if the functions are visible use 'dumpbin /EXPORTS <pathToDll>\Hook.dll' in the debugger-console
	auto CallWndRetProc = (HOOKPROC)GetProcAddress(_hLib, "CallWndRetProc");
	if (CallWndRetProc == NULL) {
		Logger::Error("The function 'CallWndRetProc' was not found.\n");
		return false;
	}

	_hWndProc = SetWindowsHookEx(WH_CALLWNDPROC, (HOOKPROC)CallWndRetProc, _hLib, threadId);
	if (_hWndProc == NULL) {
		Logger::Error(MODULE_NAME "RegisterHook() - Error Creation Hook _hWndProc\n");
		UnRegisterHook();
		return false;
	}

	return true;
}

void UnRegisterHook()
{
	if (_hWndProc) {
		UnhookWindowsHookEx(_hWndProc);
		_hWndProc = NULL;
	}
}

/*
 * @brief Sets the 'launch on windows startup'-setting.
 */
void SetLaunchOnWindowsStartupSetting(bool value)
{
	AppData::LaunchOnWindowsStartup.Set(value);

	if (value) {
		Registry::RegisterProgram();
	} else {
		Registry::UnregisterProgram();
	}
}
