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

#include "SharedDefines.h"
#include "WindowsMessage.h"
#include "WinSockClient.h"

#include <windows.h>
#include <iostream>
#include <sstream>
#include <psapi.h> // OpenProcess()
#include <shellscalingapi.h> // For dpi-scaling stuff

#pragma comment(lib, "SHCore") // For dpi-scaling stuff

// Problems with OutputDebugString:
// Had problems that the messages from OutputDebugString in the hook-functions did not work anymore after some playing arround with old versions, suddenly it worked again.
// Maybe its best to restart pc and hope that it will be fixed, when this happens again...

// NOTES:
// It looks like as if the contrlos like _ and X for minimize or close are custom controls and do not trigger all normal messgages
// For example SC_CLOSE seems to be not sent when clicking the X

// Use DebugView (https://docs.microsoft.com/en-us/sysinternals/downloads/debugview) to see OutputDebugString() messages.

constexpr char* MODULE_NAME = "WhatsappTrayHook";

#define DLLIMPORT __declspec(dllexport)

static DWORD _processID = NULL;
static HWND _whatsAppWindowHandle = NULL;
static WNDPROC _originalWndProc = NULL;

static UINT _dpiX; /* The horizontal dpi-size. Is set in Windows settings. Default 100% = 96 */
static UINT _dpiY; /* The vertical dpi-size. Is set in Windows settings. Default 100% = 96 */

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved);
static LRESULT APIENTRY RedirectedWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static void UpdateDpi(HWND hwnd);
// NOTE: extern "C" is important to disable name-mangling, so that the functions can be found with GetProcAddress(), which is used in WhatsappTray.cpp
extern "C" DLLIMPORT LRESULT CALLBACK CallWndRetProc(int nCode, WPARAM wParam, LPARAM lParam); 
static HWND GetTopLevelWindowhandleWithName(std::string searchedWindowTitle);
static std::string GetWindowTitle(HWND hwnd);
static std::string GetFilepathFromProcessID(DWORD processId);
static std::string WideToUtf8(const std::wstring& inputString);
static std::string GetEnviromentVariable(const std::string& inputString);
static POINT LParamToPoint(LPARAM lParam);
static bool SendMessageToWhatsappTray(UINT message, WPARAM wParam = 0, LPARAM lParam = 0);
static void TraceString(std::string traceString);
static void TraceStream(std::ostringstream& traceBuffer);

static void StartInitThread();
static DWORD WINAPI Init(LPVOID lpParam);

#define LogString(logString, ...) TraceString(MODULE_NAME + std::string("::") + std::string(__func__) + ": " + string_format(logString, __VA_ARGS__))

/**
 * @brief The entry point for the dll
 * 
 * This is called when the hook is attached to the thread
*/
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH: {
		
		StartInitThread();

		break;
	}
	case DLL_PROCESS_DETACH: {
		LogString("Detach hook.dll from ProcessID: 0x%08X", _processID);

		// Remove our window-proc from the chain by setting the original window-proc.
		if (_originalWndProc != NULL) {
			SetWindowLongPtr(_whatsAppWindowHandle, GWLP_WNDPROC, (LONG_PTR)_originalWndProc);
		}

		// NOTE: For some reason this works here. 
		// Because according to this:https://docs.microsoft.com/en-ca/windows/win32/dlls/dynamic-link-library-best-practices?redirectedfrom=MSDN
		// All threads should be terminated already?
		// Anyway without stopping a messagebox with an error will appear.
		SocketStop();

	} break;
	}

	return true;
}

DWORD WINAPI Init(LPVOID lpParam)
{
	//OutputDebugStringA("Hook-init-thread is started");

	SocketStart(LOGGER_IP, LOGGER_PORT);

	// Find the WhatsApp window-handle that we need to replace the window-proc
	_processID = GetCurrentProcessId();

	LogString("Attached hook.dll to ProcessID: 0x%08X", _processID);

	auto filepath = GetFilepathFromProcessID(_processID);

	/* LoadLibrary()triggers DllMain with DLL_PROCESS_ATTACH. This is used in WhatsappTray.cpp
	 * to prevent tirggering the wndProc redirect for WhatsappTray we need to detect if this happend.
	 * the easiest/best way to detect that is by setting a enviroment variable before LoadLibrary() */
	auto envValue = GetEnviromentVariable(WHATSAPPTRAY_LOAD_LIBRARY_TEST_ENV_VAR);

	LogString("Filepath: '%s' " WHATSAPPTRAY_LOAD_LIBRARY_TEST_ENV_VAR ": '%s'", filepath.c_str(), envValue.c_str());

	if (envValue.compare(WHATSAPPTRAY_LOAD_LIBRARY_TEST_ENV_VAR_VALUE) == 0) {
		LogString("Detected that this Attache was triggered by LoadLibrary() => Cancel further processing");

		// It is best to remove the variable here so we can be sure it is not removed before it was detected.
		// Delete enviroment-variable by setting it to "".
		_putenv_s(WHATSAPPTRAY_LOAD_LIBRARY_TEST_ENV_VAR, "");
		return 1;
	}

	_whatsAppWindowHandle = GetTopLevelWindowhandleWithName(WHATSAPP_CLIENT_NAME);
	auto windowTitle = GetWindowTitle(_whatsAppWindowHandle);

	LogString("Attached in window '%s' _whatsAppWindowHandle: 0x%08X", windowTitle.c_str(), _whatsAppWindowHandle);

	if (_whatsAppWindowHandle == NULL) {
		LogString("Error, window-handle for '" WHATSAPP_CLIENT_NAME "' was not found");
		return 2;
	}

	// Update the windows scaling for the monitor that whatsapp is currently on
	UpdateDpi(_whatsAppWindowHandle);

	// Replace the original window-proc with our own. This is called subclassing.
	// Our window-proc will call after the processing the original window-proc.
	_originalWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(_whatsAppWindowHandle, GWLP_WNDPROC, (LONG_PTR)RedirectedWndProc));

	return 0;
}

/**
 * @brief This is the new main window-proc. After we are done we call the original window-proc.
 * 
 * This method has the advantage over SetWindowsHookEx(WH_CALLWNDPROC... that here we can skip or modifie messages.
 */
static LRESULT APIENTRY RedirectedWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	std::ostringstream traceBuffer;

#ifdef _DEBUG
	if (uMsg != WM_GETTEXT) {
		// Dont print WM_GETTEXT so there is not so much "spam"

		traceBuffer << MODULE_NAME << "::" << __func__ << ": " << WindowsMessage::GetString(uMsg) << "(0x" << std::uppercase << std::hex << uMsg << ") ";
		traceBuffer << "windowTitle='" << GetWindowTitle(hwnd) << "' ";
		traceBuffer << "hwnd='" << std::uppercase << std::hex << hwnd << "' ";
		traceBuffer << "wParam='" << std::uppercase << std::hex << wParam << "' ";
		TraceStream(traceBuffer);
	}
#endif

	if (uMsg == WM_SYSCOMMAND) {
		// Description for WM_SYSCOMMAND: https://msdn.microsoft.com/de-de/library/windows/desktop/ms646360(v=vs.85).aspx
		if (wParam == SC_MINIMIZE) {
			LogString("SC_MINIMIZE received");

			// Here i check if the windowtitle matches. Vorher hatte ich das Problem das sich Chrome auch minimiert hat.
			if (hwnd == _whatsAppWindowHandle) {
				SendMessageToWhatsappTray(WM_WA_MINIMIZE_BUTTON_PRESSED);
			}
		}
	} else if (uMsg == WM_NCDESTROY) {
		LogString("WM_NCDESTROY received");

		if (hwnd == _whatsAppWindowHandle) {
			auto successfulSent = SendMessageToWhatsappTray(WM_WHAHTSAPP_CLOSING);
			if (successfulSent) {
				LogString("WM_WHAHTSAPP_CLOSING successful sent.");
			}
		}
	} else if (uMsg == WM_CLOSE) {
		// This happens when alt + f4 is pressed.
		LogString("WM_CLOSE received. Probably Alt + F4");

		// Notify WhatsappTray and if it wants to close it can do so...
		SendMessageToWhatsappTray(WM_WHATSAPP_TO_WHATSAPPTRAY_RECEIVED_WM_CLOSE);

		LogString("WM_CLOSE blocked.");

		// Block WM_CLOSE
		return 0;
	} else if (uMsg == WM_WHATSAPPTRAY_TO_WHATSAPP_SEND_WM_CLOSE) {
		// This message is defined by me and should only come from WhatsappTray.
		// It more or less replaces WM_CLOSE which is now always blocked...
		// To have a way to still send WM_CLOSE this message was made.
		LogString("WM_WHATSAPPTRAY_TO_WHATSAPP_SEND_WM_CLOSE received");

		LogString("Send WM_CLOSE to WhatsApp.");
		// NOTE: lParam/wParam are not used in WM_CLOSE.
		return CallWindowProc(_originalWndProc, hwnd, WM_CLOSE, 0, 0);
	} else if (uMsg == WM_DPICHANGED) {
		LogString("WM_DPICHANGED received");

		LogString("Updating the Dpi");
		UpdateDpi(_whatsAppWindowHandle);
	} else if (uMsg == WM_LBUTTONUP) {
		// Note x and y are clientare-coordiantes
		auto clickPoint = LParamToPoint(lParam);
		LogString("WM_LBUTTONUP received x=%d y=%d", clickPoint.x, clickPoint.y);

		RECT rect;
		GetClientRect(hwnd, &rect);

		constexpr int defaultDpi = 96;
		int widthOfButton = 46; /* dpi 96 (100%) window not maximized */
		int heightOfButton = 34; /* dpi 96 (100%) window not maximized */

		// I use percent because it is a fraction like 1,25. 100 to get value in percent
		int dpiRatioPercentX = (100 * _dpiX) / defaultDpi;
		// NOTE: The width is little to big but it is better to wrongly send to tray instead of maximize then close instead of send to tray
		widthOfButton = (widthOfButton * dpiRatioPercentX) / 100;
		int dpiRatioPercentY = (100 * _dpiY) / defaultDpi;
		heightOfButton = (heightOfButton * dpiRatioPercentY) / 100;

		// calculate x-distance fom right window border
		int windowWidth = rect.right - rect.left;
		int xDistanceFromRight = windowWidth - clickPoint.x;
		LogString("WM_LBUTTONUP => windowWidth=%d xDistanceFromRight=%d widthOfButton=%d", windowWidth, xDistanceFromRight, widthOfButton);

		if (xDistanceFromRight <= widthOfButton && clickPoint.y <= heightOfButton) {
			SendMessageToWhatsappTray(WM_WA_CLOSE_BUTTON_PRESSED);

			LogString("Block WM_LBUTTONUP");
			return 0;
		}
	}

	// Call the original window-proc.
	return CallWindowProc(_originalWndProc, hwnd, uMsg, wParam, lParam);
}


/**
 * @brief Update the windows scaling for the monitor that the window is currently on
*/
static void UpdateDpi(HWND hwnd)
{
	auto monitorHandle = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);

	auto result = GetDpiForMonitor(monitorHandle, MDT_DEFAULT, &_dpiX, &_dpiY);

	if (result != S_OK) {
		LogString("Error when getting the dpi for WhatsApp");
		// Continue even with the error...
	}
	else {
		LogString("The dpi for WhatsApp is dpiX: %d dpiY: %d", _dpiX, _dpiY);
	}

	return;
}

/**
 * THIS IS CURRENTLY ONLY A DUMMY. BUT THE SetWindowsHookEx() IS STILL USED TO INJECT THE DLL INTO THE TARGET (WhatsApp)
 * NOTE: This is better then the mousehook because it will inject the dll imediatly after SetWindowsHookEx()
 *       The Mousehook waits until it receives a mouse-message and then attaches the dll. This means the cursor has to be move on top of WhatsApp
 *
 * Only works for 32-bit apps or 64-bit apps depending on whether this is complied as 32-bit or 64-bit (Whatsapp is a 64Bit-App)
 * NOTE: This only caputers messages sent by SendMessage() and NOT PostMessage()!
 * NOTE: This function also receives messages from child-windows.
 * - This means we have to be carefull not accidently mix them.
 * - For example when watching for WM_NCDESTROY, it is possible that a childwindow does that but the mainwindow remains open.
 * @param nCode
 * @param wParam
 * @param lParam Contains a CWPSTRUCT*-struct in which the data can be found.
 */
LRESULT CALLBACK CallWndRetProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	return CallNextHookEx(NULL, nCode, wParam, lParam);
}

/**
 * @brief Returns the window-handle for the window with the searched name in the current process
 * 
 * https://stackoverflow.com/questions/31384004/can-several-windows-be-bound-to-the-same-process
 * Windows are associated with threads.
 * Threads are associated with processes.
 * A thread can create as many top-level windows as it likes.
 * Therefore you can perfectly well have multiple top-level windows associated with the same thread.
 * @return 
*/
static HWND GetTopLevelWindowhandleWithName(std::string searchedWindowTitle)
{
	HWND iteratedHwnd = NULL;
	while (true) {
		iteratedHwnd = FindWindowExA(NULL, iteratedHwnd, NULL, WHATSAPP_CLIENT_NAME);
		if (iteratedHwnd == NULL) {
			return NULL;
		}

		DWORD processId;
		DWORD threadId = GetWindowThreadProcessId(iteratedHwnd, &processId);

		// Check processId and Title
		// Already observerd an instance where the processId alone lead to an false match!
		if (_processID != processId) {
			// processId does not match -> continue looking
			continue;
		}
		// Found matching processId

		if (iteratedHwnd != GetAncestor(iteratedHwnd, GA_ROOT)) {
			//LogString("Window is not a toplevel-window");
			continue;
		}

		auto windowTitle = GetWindowTitle(iteratedHwnd);
		// Also check length because compare also matches strings that are longer like 'WhatsApp Voip'
		if (windowTitle.compare(searchedWindowTitle) != 0 || windowTitle.length() != strlen(WHATSAPP_CLIENT_NAME)) {
			//LogString("windowTitle='" + windowTitle + "' does not match '" WHATSAPP_CLIENT_NAME "'");
			continue;
		}

		// Window handle found
		break;
	}

	return iteratedHwnd;
}

/**
 * @brief Gets the text of a window.
 */
static std::string GetWindowTitle(HWND hwnd)
{
	char windowNameBuffer[2000];
	GetWindowTextA(hwnd, windowNameBuffer, sizeof(windowNameBuffer));

	return std::string(windowNameBuffer);
}

/**
 * @brief Get the path to the executable for the ProcessID
 * 
 * @param processId The ProcessID from which the path to the executable should be fetched
 * @return The path to the executable from the ProcessID
*/
static std::string GetFilepathFromProcessID(DWORD processId)
{
	HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
	if (processHandle == NULL) {
		return "";
	}

	wchar_t filepath[MAX_PATH];
	if (GetModuleFileNameExW(processHandle, NULL, filepath, MAX_PATH) == 0) {
		CloseHandle(processHandle);
		return "";
	}
	CloseHandle(processHandle);

	return WideToUtf8(filepath);
}

static std::string WideToUtf8(const std::wstring& inputString)
{
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, inputString.c_str(), (int)inputString.size(), NULL, 0, NULL, NULL);
	std::string strTo(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, inputString.c_str(), (int)inputString.size(), &strTo[0], size_needed, NULL, NULL);
	return strTo;
}

static std::string GetEnviromentVariable(const std::string& inputString)
{
	size_t requiredSize;
	const size_t varbufferSize = 2000;
	char varbuffer[varbufferSize];

	getenv_s(&requiredSize, varbuffer, varbufferSize, inputString.c_str());

	return varbuffer;
}

static POINT LParamToPoint(LPARAM lParam)
{
	POINT p = { LOWORD(lParam), HIWORD(lParam) };

	return p;
}

/**
 * @brief Send message to WhatsappTray.
 */
static bool SendMessageToWhatsappTray(UINT message, WPARAM wParam, LPARAM lParam)
{
	return PostMessage(FindWindow(NAME, NAME), message, wParam, lParam);
}

static void TraceString(std::string traceString)
{
	SocketSendMessage(traceString.c_str());

//#ifdef _DEBUG
//	OutputDebugStringA(traceString.c_str());
//#endif
}

static void TraceStream(std::ostringstream& traceBuffer)
{
	SocketSendMessage(traceBuffer.str().c_str());
	traceBuffer.clear();
	traceBuffer.str(std::string());

//#ifdef _DEBUG
//	OutputDebugStringA(traceBuffer.str().c_str());
//	traceBuffer.clear();
//	traceBuffer.str(std::string());
//#endif
}

/**
 * @brief Starts the hook-init in an seperate thread
 * 
 * Because it is better to not use DllMain for more complex initialisaition, a seperate thread will be created in this function.
 * NOTE: I had problems with '_processMessagesThread = std::thread(ProcessMessageQueue);' in WinSockClient, which would not run because of some limitations of DllMain
 * IMPORTANT: Don't wait for the thread to finish! This should not be done in DllMain...
*/
void StartInitThread()
{
	DWORD   threadId;
	HANDLE  threadHandle;

	threadHandle = CreateThread(
		NULL,                   // default security attributes
		0,                      // use default stack size  
		Init,       // thread function name
		NULL,                   // argument to thread function 
		0,                      // use default creation flags 
		&threadId);   // returns the thread identifier 

	// Check the return value for success.
	if (threadHandle == NULL) {
		//OutputDebugStringA("Thread could not be created in Hook init-function-starter");
	}

	// Close thread handle. NOTE(SAM): For now i do not clean up the thread handle because it shouldn't be such a big deal...
	//CloseHandle(threadHandle);
}
