/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright(C) 1998 - 2017 WhatsappTray Sebastian Amann */

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

namespace fs = std::filesystem;

#ifdef _DEBUG
constexpr auto CompileConfiguration = "Debug";
#else
constexpr auto CompileConfiguration = "Release";
#endif

#undef MODULE_NAME
#define MODULE_NAME "WhatsappTray"

static HINSTANCE _hInstance = NULL; /* Handle to the instance of WhatsappTray */
static HWND _hwndWhatsappTray = NULL; /* Handle to the window of WhatsappTray */
static HWND _hwndWhatsapp = NULL; /* Handle to the window of WhatsApp */

static HHOOK _hWndProc = NULL; /* Handle to the Hook from SetWindowsHookEx() */
static HMODULE _hLib = NULL; /* Handle to the Hook.dll */

static int _messagesSinceMinimize = 0;

static char _loggerPort[] = LOGGER_PORT;
static std::thread _winsockThread;

static std::unique_ptr<TrayManager> _trayManager;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd);
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static bool InitWhatsappTray();
static HWND StartWhatsapp();
static HWND FindWhatsapp();
static void TryClosePreviousWhatsappTrayInstance();
static bool CreateWhatsappTrayWindow();
static void ExecuteMenu();
static bool SetHook();
static void UnRegisterHook();
static void SetLaunchOnWindowsStartupSetting(const bool value);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	_hInstance = hInstance;

	Logger::Setup();

	// For reading data from *.lnk files (shortcut files). See Helper::ResolveLnk()
	CoInitialize(nullptr);

	LogInfo("Starting WhatsappTray %s in %s CompileConfiguration.", Helper::GetProductAndVersion().c_str(), CompileConfiguration);
	LogInfo("CloseToTray=%d.", static_cast<bool>(AppData::CloseToTray.Get()));
	
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

	// Check if closeToTray was set per commandline. (this overrides the persistent storage-value.)
	if (strstr(lpCmdLine, "--closeToTray")) {
		AppData::CloseToTray.Set(true);
	}

	/* If WhatsappTray window is already open close it */
	TryClosePreviousWhatsappTrayInstance();

	if (CreateWhatsappTrayWindow() == false) {
		return 1;
	}

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	Gdiplus::GdiplusShutdown(gdiplusToken);
	CoUninitialize();

	return 0;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static UINT s_uTaskbarRestart;
	//LogInfo("Message Received msg='0x%X'", msg);

	switch (msg) {
	case WM_CREATE: {
		// NOTE: CreateWindow() immediatly calls the WndProc. Before TranslateMessage/DispatchMessage !!!
		_hwndWhatsappTray = hwnd;

		// Handle crash or restart of explorer
		// If not handled, the tray-icon will be gone
		// https://docs.microsoft.com/en-us/windows/win32/shell/taskbar#taskbar-creation-notification
		// See default case for handling of this message
		s_uTaskbarRestart = RegisterWindowMessage(TEXT("TaskbarCreated"));

		if (InitWhatsappTray() == false) {
			DestroyWindow(_hwndWhatsappTray);
		}
	} break;
	case WM_CLOSE: {
		DestroyWindow(_hwndWhatsappTray);
	} break;
	case WM_TRAYCMD: {
#pragma WARNING(Move into TrayManager. Problem is executeMenue...)
		switch (static_cast<UINT>(lParam)) {
		case NIN_SELECT: {
			if (IsWindowVisible(_hwndWhatsapp) == false) {
				_trayManager->RestoreWindowFromTray(_hwndWhatsapp);
			} else {
				_trayManager->MinimizeWindowToTray(_hwndWhatsapp);
			}
		} break;
		case WM_CONTEXTMENU: {
			ExecuteMenu();
		} break;
		}
	} break;
	case WM_COMMAND: {
		switch (LOWORD(wParam)) {
		case IDM_ABOUT: {
			AboutDialog::Create(_hInstance, _hwndWhatsappTray);
		} break;
		case IDM_SETTING_CLOSE_TO_TRAY: {
			// Toggle the 'close to tray'-feature.
			AppData::CloseToTray.Set(!AppData::CloseToTray.Get());
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
		case IDM_SETTING_CLOSE_TO_TRAY_WITH_ESCAPE: {
			// Toggle the 'CloseToTrayWithEscape'-feature.
			AppData::CloseToTrayWithEscape.Set(!AppData::CloseToTrayWithEscape.Get());
		} break;
		case IDM_RESTORE: {
			LogInfo("IDM_RESTORE");
			_trayManager->RestoreWindowFromTray(_hwndWhatsapp);
		} break;
		case IDM_CLOSE: {
			LogInfo("IDM_CLOSE");
			_trayManager->CloseWindowFromTray(_hwndWhatsapp);

			// Close WhatsappTray
			SendMessage(_hwndWhatsappTray, WM_CLOSE, 0, 0);
		} break;
		}
	} break;
	case WM_WA_MINIMIZE_BUTTON_PRESSED: {
		LogInfo("WM_WA_MINIMIZE_BUTTON_PRESSED");

		if (AppData::CloseToTray.Get() == true) {
			LogInfo("Close to tray is true => Minimize WhatsApp to taskbar.");
		} else {
			LogInfo("Close to tray is false => Minimize WhatsApp to tray.");
			_trayManager->MinimizeWindowToTray(_hwndWhatsapp);
		}
	} break;
	case WM_WA_CLOSE_BUTTON_PRESSED: {
		LogInfo("WM_WA_CLOSE_BUTTON_PRESSED");

		if (AppData::CloseToTray.Get() == true) {
			LogInfo("Close to tray is true => Minimize WhatsApp to tray.");
			_trayManager->MinimizeWindowToTray(_hwndWhatsapp);
		} else {
			LogInfo("Close to tray is false => Close WhatsApp and WhatsappTray.");
			_trayManager->CloseWindowFromTray(_hwndWhatsapp);
		}
	} break;
	case WM_WA_KEY_PRESSED: {
		LogInfo("WM_WA_KEY_PRESSED wParam=%d", wParam);

		if (AppData::CloseToTrayWithEscape.Get() == true) {
			if (wParam == 27) {
				// Esc-key was pressed
				_trayManager->MinimizeWindowToTray(_hwndWhatsapp);
			}
		}
	} break;
	case WM_WHAHTSAPP_CLOSING: {
		LogInfo("WM_WHAHTSAPP_CLOSING");

		// Whatsapp is closing
		// Close WhatsappTray
		SendMessage(_hwndWhatsappTray, WM_CLOSE, 0, 0);
	} break;
	case WM_WHATSAPP_TO_WHATSAPPTRAY_RECEIVED_WM_CLOSE: {
		LogInfo("WM_WHATSAPP_TO_WHATSAPPTRAY_RECEIVED_WM_CLOSE received");
		// This message means that WhatsApp received a WM_CLOSE-message.
		// This happens when Alt + F4 is pressed.

		if (AppData::CloseToTray.Get()) {
			// Close to tray is activated => Minimize Whatsapp to tray.
			_trayManager->MinimizeWindowToTray(_hwndWhatsapp);
		}
		else {
			// Close to tray is not activated => Close Whatsapp.

			// NOTE: WM_WHATSAPPTRAY_TO_WHATSAPP_SEND_WM_CLOSE is a special message i made because WM_CLOSE is always blocked by the hook.
			PostMessage(_hwndWhatsapp, WM_WHATSAPPTRAY_TO_WHATSAPP_SEND_WM_CLOSE, 0, 0);
			LogInfo("WM_WHATSAPPTRAY_TO_WHATSAPP_SEND_WM_CLOSE sent");
		}

	} break;
	case WM_DESTROY: {
		LogInfo("WM_DESTROY");

		if (_trayManager != NULL) {
			_trayManager->RestoreAllWindowsFromTray();
			_trayManager->RemoveTrayIcon(_hwndWhatsapp);
		}

		UnRegisterHook();
		if (_hLib != NULL) {
			FreeLibrary(_hLib);
		}

		// Stop winsock-server and wait for it to cleanup and finish
		SocketStopServer();
		_winsockThread.join();

		PostQuitMessage(0);
		LogInfo("QuitMessage posted.");
	} break;
	case WM_WHATSAPP_API_NEW_MESSAGE: {

		LogInfo("WM_WHATSAPP_API_NEW_MESSAGE");
		_messagesSinceMinimize++;

		if (AppData::ShowUnreadMessages.Get()) {
			char messagesSinceMinimizeBuffer[20] = { 0 };
			snprintf(messagesSinceMinimizeBuffer, sizeof(messagesSinceMinimizeBuffer), "%d", _messagesSinceMinimize);
			_trayManager->SetIcon(_hwndWhatsapp, messagesSinceMinimizeBuffer);
		}

	} break;
	default: {
		if (msg == s_uTaskbarRestart) {
			_trayManager = std::make_unique<TrayManager>(_hwndWhatsappTray);
			_trayManager->RegisterWindow(_hwndWhatsapp);
		}
	} break;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

/**
 * @brief Initializes WhatsappTray
*/
static bool InitWhatsappTray()
{
	// Setup the settings for launch on windows startup.
	SetLaunchOnWindowsStartupSetting(AppData::LaunchOnWindowsStartup.Get());

	if (AppData::StartMinimized.Get()) {
		LogInfo("Prepare for starting minimized.");

		WhatsAppApi::NotifyOnFullInit([]() {
			LogInfo("NotifyOnFullInit");
			Sleep(2000);
			_trayManager->MinimizeWindowToTray(_hwndWhatsapp);
			// Remove event after the first execution
			WhatsAppApi::NotifyOnFullInit(NULL);
			});
	}

	// TrayManager needs to be ready before WhatsApp is started to handle 'start minimized'
	_trayManager = std::make_unique<TrayManager>(_hwndWhatsappTray);

	if (StartWhatsapp() == nullptr) {
		MessageBoxA(NULL, "Error launching WhatsApp. Examine the logs for details", "WhatsappTray", MB_OK);
		return false;
	}

	_trayManager->RegisterWindow(_hwndWhatsapp);

	if (SetHook() == false) {
		LogError("Error setting hook.");
		return false;
	}

	// Send a WM_WHATSAPP_API_NEW_MESSAGE-message when a new WhatsApp-message has arrived.
	WhatsAppApi::NotifyOnNewMessage([]() { PostMessageA(_hwndWhatsappTray, WM_WHATSAPP_API_NEW_MESSAGE, 0, 0); });

	return true;
}

/**
 * @brief Start WhatsApp
 */
static HWND StartWhatsapp()
{
	fs::path waStartPath = Helper::Utf8ToWide(AppData::WhatsappStartpathGet());
	std::string waStartPathString;
	if (waStartPath.is_relative()) {
		fs::path appPath = Helper::GetApplicationFilePath();
		auto combinedPath = appPath / waStartPath;

		LogInfo("Starting WhatsApp from combinedPath:%s", combinedPath.u8string().c_str());

		// Shorten the path by converting to absoltue path.
		auto combinedPathCanonical = fs::canonical(combinedPath);
		waStartPathString = combinedPathCanonical.u8string();
	} else {
		waStartPathString = waStartPath.u8string();
	}

	LogInfo("Starting WhatsApp from canonical-path:'" + waStartPathString + "'");

	auto waStartPathStringExtension = waStartPathString.substr(waStartPathString.size() - 3);
	if (waStartPathStringExtension.compare("lnk") == 0)
	{
		waStartPathString = Helper::ResolveLnk(_hwndWhatsappTray, waStartPathString.c_str());
		LogInfo("Resolved .lnk (Shortcut) to:'" + waStartPathString + "'");
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
		LogInfo("CreateProcess failed(%d).", GetLastError());
		return nullptr;
	}

	// Wait a maximum of 60 seconds for the process to go into idle.
	auto result = WaitForInputIdle(pi.hProcess, 60000);
	if (result != 0)
	{
		LogInfo("WaitForInputIdle failed.");
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
		LogInfo("WhatsApp-Window not found. Wait 500ms and retry.");
		Sleep(500);
	}

	LogInfo("WhatsApp-Window found.");

	return _hwndWhatsapp;
}

/**
 * Search for the WhatsApp window
 * WARNING: The WhatsApp window needs to be visible!
 * Checks if it is the correct window:
 * 1. Get the path to the binary(exe) for the window with "WhatsApp" in the title
 * 2. Match it with the appData-setting.
 */
static HWND FindWhatsapp()
{
	HWND iteratedHwnd = NULL;
	while (true) {
		iteratedHwnd = FindWindowExA(NULL, iteratedHwnd, NULL, WHATSAPP_CLIENT_NAME);
		if (iteratedHwnd == NULL) {
			return NULL;
		}

		auto windowTitle = Helper::GetWindowTitle(iteratedHwnd);
		LogInfo("Found window with title: '%s' hwnd=0x%08X", windowTitle.c_str(), reinterpret_cast<uintptr_t>(iteratedHwnd));

		// It looks like as if the 'Whatsapp Voip'-window is first named 'Whatsapp' when Whatsapp is started and then changed shortly after to 'Whatsapp Voip'.
		// Because of that it can happen that the wrong window is set.
		// To prevent that it is checked if the window is visible because 'Whatsapp Voip'-window seems to be always hidden. NOTE: I Could also always check the window later before i use it...
		if (IsWindowVisible(iteratedHwnd) == false) {
			LogInfo("Window is not visible");
			continue;
		}

		// Also check length because compare also matches strings that are longer like 'WhatsApp Voip'. Not sure if that is true but i leave it for now...
		if (windowTitle.compare(WHATSAPP_CLIENT_NAME) != 0 && windowTitle.length() == strlen(WHATSAPP_CLIENT_NAME)) {
			LogInfo("Window name does not match");
			continue;
		}

		DWORD processId;
		DWORD threadId = GetWindowThreadProcessId(iteratedHwnd, &processId);

		auto filepath = Helper::GetFilepathFromProcessID(processId);

		LogInfo("Filepath is: '%s'", filepath.c_str());

		std::wstring filenameFromWindow = Helper::GetFilenameFromPath(Helper::Utf8ToWide(filepath));
		std::wstring whatsappStartpathWide = Helper::Utf8ToWide(AppData::WhatsappStartpathGet());
		std::wstring filenameFromSettings = Helper::GetFilenameFromPath(whatsappStartpathWide);

		// NOTE: I do not compare the extension because when i start from an link, the name is WhatsApp.lnk whicht does not match the WhatsApp.exe
		// This could be improved by reading the real value from the .lnk but i think this should be fine for now.
		if (filenameFromWindow.compare(filenameFromSettings) != 0) {
			LogError("Filenames from window and from settings do not match.");
			continue;
		}

		LogInfo("Found match");
		break;
	}

	return iteratedHwnd;
}

/**
 * Try to close old WhatsappTray instance
 */
static void TryClosePreviousWhatsappTrayInstance()
{
	// Test if WhatsappTray is already running.
	// NOTE: This also matches the class-name of the window so we can be sure its our window and not for example an explorer-window with this name.
	_hwndWhatsappTray = FindWindow(NAME, NAME);
	if (_hwndWhatsappTray) {
		LogError("Found an already open instance of WhatsappTray. Trying to close the other instance.");
		LogError("If this error persists, try to close the other instance by hand using for example the taskmanager.");
		//if (strstr(lpCmdLine, "--exit")) {
		SendMessage(_hwndWhatsappTray, WM_CLOSE, 0, 0);

#pragma WARNING("It would be best to wait her a bit and check if it is still active. And if it is still active shoot it down.")
	}
}

/**
 * Create a window.
 * This window will be mainly used to receive messages.
 */
static bool CreateWhatsappTrayWindow()
{
	WNDCLASS wc{};
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

	// NOTE: This will immediatly call the WndProc with a few messages! One of them is WM_CREATE
	CreateWindow(NAME, NAME, WS_OVERLAPPED, 0, 0, 0, 0, (HWND)NULL, (HMENU)NULL, _hInstance, (LPVOID)NULL);

	if (_hwndWhatsappTray == NULL) {
		MessageBox(NULL, "Error creating window", "WhatsappTray", MB_OK | MB_ICONERROR);
		return false;
	}

	return true;
}

/*
 * Create the rightclick-menue.
 */
static void ExecuteMenu()
{
	HMENU hMenu = CreatePopupMenu();
	if (!hMenu) {
		LogError("Error creating menu.");
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

	// -- Show unread messages.
	if (AppData::ShowUnreadMessages.Get()) {
		AppendMenu(hMenu, MF_CHECKED, IDM_SETTING_SHOW_UNREAD_MESSAGES, "Show Unread Messages (experimental)");
	} else {
		AppendMenu(hMenu, MF_UNCHECKED, IDM_SETTING_SHOW_UNREAD_MESSAGES, "Show Unread Messages (experimental)");
	}

	// -- Close to tray with escape key.
	if (AppData::CloseToTrayWithEscape.Get()) {
		AppendMenu(hMenu, MF_CHECKED, IDM_SETTING_CLOSE_TO_TRAY_WITH_ESCAPE, "Close to tray with escape key");
	} else {
		AppendMenu(hMenu, MF_UNCHECKED, IDM_SETTING_CLOSE_TO_TRAY_WITH_ESCAPE, "Close to tray with escape key");
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

static bool SetHook()
{
	LogInfo("SetHook()");

	// Damit nicht alle Prozesse gehookt werde, verwende ich jetzt die ThreadID des WhatsApp-Clients.
	DWORD processId;
	DWORD threadId = GetWindowThreadProcessId(_hwndWhatsapp, &processId);
	if (threadId == NULL) {
		MessageBox(NULL, MODULE_NAME "::SetHook() ThreadID of WhatsApp-Window not found", "WhatsappTray", MB_OK | MB_ICONERROR);
		return false;
	}

	// For the case when this function is called multiple times, check if the dll is already loaded
	if (_hLib == NULL) {
		/* LoadLibrary()triggers DllMain with DLL_PROCESS_ATTACH. This is used in WhatsappTray.cpp
		 * to prevent tirggering the wndProc redirect for WhatsappTray we need to detect if this happend.
		 * the easiest/best way to detect that is by setting a enviroment variable before LoadLibrary() */
		_putenv_s(WHATSAPPTRAY_LOAD_LIBRARY_TEST_ENV_VAR, WHATSAPPTRAY_LOAD_LIBRARY_TEST_ENV_VAR_VALUE);
		_hLib = LoadLibrary("Hook.dll");
		if (_hLib == NULL) {
			LogError("Error loading Hook.dll");
			MessageBox(NULL, MODULE_NAME "::SetHook() Error loading Hook.dll", "WhatsappTray", MB_OK | MB_ICONERROR);
			return false;
		}
	}

	// NOTE: To see if the functions are visible use 'dumpbin /EXPORTS <pathToDll>\Hook.dll' in the debugger-console
	auto CallWndRetProc = (HOOKPROC)GetProcAddress(_hLib, "CallWndRetProc");
	if (CallWndRetProc == NULL) {
		LogError("The function 'CallWndRetProc' was not found");
		return false;
	}

	_hWndProc = SetWindowsHookEx(WH_CALLWNDPROC, (HOOKPROC)CallWndRetProc, _hLib, threadId);
	if (_hWndProc == NULL) {
		LogError("Error Creation Hook _hWndProc");
		UnRegisterHook();
		return false;
	}

	return true;
}

static void UnRegisterHook()
{
	if (_hWndProc) {
		UnhookWindowsHookEx(_hWndProc);
		_hWndProc = NULL;
	}
}

/*
 * @brief Sets the 'launch on windows startup'-setting.
 */
static void SetLaunchOnWindowsStartupSetting(const bool value)
{
	AppData::LaunchOnWindowsStartup.Set(value);

	if (value) {
		Registry::RegisterProgram();
	} else {
		Registry::UnregisterProgram();
	}
}
