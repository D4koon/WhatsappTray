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
#include <fstream>
#include <sstream>
#include <psapi.h> // OpenProcess()

// Problems with OutputDebugString:
// Had problems that the messages from OutputDebugString in the hook-functions did not work anymore after some playing arround with old versions, suddenly it worked again.
// Maybe its best to restart pc and hope that it will be fixed, when this happens again...

// NOTES:
// It looks like as if the contrlos like _ and X for minimize or close are custom controls and do not trigger all normal messgages
// For example SC_CLOSE seems to be not sent when clicking the X

// Use DebugView (https://docs.microsoft.com/en-us/sysinternals/downloads/debugview) to see OutputDebugString() messages.

#undef MODULE_NAME
#define MODULE_NAME "WhatsappTrayHook::"

#define DLLIMPORT __declspec(dllexport)

static DWORD _processID = NULL;
static HWND _whatsAppWindowHandle = NULL;
static WNDPROC _originalWndProc = NULL;

static HWND _tempWindowHandle = NULL; /* For GetTopLevelWindowhandleWithName() */
static std::string _searchedWindowTitle; /* For GetTopLevelWindowhandleWithName() */

static LRESULT APIENTRY RedirectedWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
// NOTE: extern "C" is important to disable name-mangling, so that the functions can be found with GetProcAddress(), which is used in WhatsappTray.cpp
extern "C" DLLIMPORT LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam);
static HWND GetTopLevelWindowhandleWithName(std::string searchedWindowTitle);
static BOOL CALLBACK FindTopLevelWindowhandleWithNameCallback(HWND hwnd, LPARAM lParam);
static bool WindowhandleIsToplevelWithTitle(HWND hwnd, std::string searchedWindowTitle);
static bool IsTopLevelWhatsApp(HWND hwnd);
static std::string GetWindowTitle(HWND hwnd);
static std::string GetFilepathFromProcessID(DWORD processId);
static std::string WideToUtf8(const std::wstring& inputString);
static std::string GetEnviromentVariable(const std::string& inputString);
static bool SendMessageToWhatsappTray(UINT message, WPARAM wParam = 0, LPARAM lParam = 0);
static void TraceString(std::string traceString);
static void TraceStream(std::ostringstream& traceBuffer);

#define LogString(logString, ...) TraceString(string_format(logString, __VA_ARGS__))

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH: {
		SocketInit();

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
			break;
		}

		_whatsAppWindowHandle = GetTopLevelWindowhandleWithName(WHATSAPP_CLIENT_NAME);
		auto windowTitle = GetWindowTitle(_whatsAppWindowHandle);

		LogString("Attached in window '%s' _whatsAppWindowHandle: 0x%08X", windowTitle.c_str(), _whatsAppWindowHandle);

		if (_whatsAppWindowHandle == NULL) {
			LogString("Error, window-handle for '" WHATSAPP_CLIENT_NAME "' was not found");
		}

		// Replace the original window-proc with our own. This is called subclassing.
		// Our window-proc will call after the processing the original window-proc.
		_originalWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(_whatsAppWindowHandle, GWLP_WNDPROC, (LONG_PTR)RedirectedWndProc));

		break;
	}
	case DLL_PROCESS_DETACH: {
		LogString("Detach hook.dll from ProcessID: 0x%08X",_processID);

		// Remove our window-proc from the chain by setting the original window-proc.
		if (_originalWndProc != NULL) {
			SetWindowLongPtr(_whatsAppWindowHandle, GWLP_WNDPROC, (LONG_PTR)_originalWndProc);
		}

		SocketCleanup();
	} break;
	}

	return true;
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

		traceBuffer << MODULE_NAME << __func__ << ": " << WindowsMessage::GetString(uMsg) << "(0x" << std::uppercase << std::hex << uMsg << ") ";
		traceBuffer << "windowTitle='" << GetWindowTitle(hwnd) << "' ";
		traceBuffer << "hwnd='" << std::uppercase << std::hex << hwnd << "' ";
		traceBuffer << "wParam='" << std::uppercase << std::hex << wParam << "' ";
		TraceStream(traceBuffer);
	}
#endif

	if (uMsg == WM_SYSCOMMAND) {
		// Description for WM_SYSCOMMAND: https://msdn.microsoft.com/de-de/library/windows/desktop/ms646360(v=vs.85).aspx
		if (wParam == SC_MINIMIZE) {
			LogString(MODULE_NAME "::%s: SC_MINIMIZE received", __func__);

			// Here i check if the windowtitle matches. Vorher hatte ich das Problem das sich Chrome auch minimiert hat.
			if (IsTopLevelWhatsApp(hwnd)) {
				SendMessageToWhatsappTray(WM_WA_MINIMIZE_BUTTON_PRESSED);
			}
		}
	} else if (uMsg == WM_NCDESTROY) {
		LogString(MODULE_NAME "::%s: WM_NCDESTROY received", __func__);

		if (IsTopLevelWhatsApp(hwnd)) {
			auto successfulSent = SendMessageToWhatsappTray(WM_WHAHTSAPP_CLOSING);
			if (successfulSent) {
				TraceString(MODULE_NAME "WM_WHAHTSAPP_CLOSING successful sent.");
			}
		}
	} else if (uMsg == WM_CLOSE) {
		// This happens when alt + f4 is pressed.
		LogString(MODULE_NAME "::%s: WM_CLOSE received", __func__);

		// Notify WhatsappTray and if it wants to close it can do so...
		SendMessageToWhatsappTray(WM_WHATSAPP_TO_WHATSAPPTRAY_RECEIVED_WM_CLOSE);

		LogString(MODULE_NAME "::%s: WM_CLOSE blocked.", __func__);
		// Block WM_CLOSE
		return 0;
	} else if (uMsg == WM_WHATSAPPTRAY_TO_WHATSAPP_SEND_WM_CLOSE) {
		// This message is defined by me and should only come from WhatsappTray.
		// It more or less replaces WM_CLOSE which is now always blocked...
		// To have a way to still send WM_CLOSE this message was made.
		LogString(MODULE_NAME "::%s: WM_WHATSAPPTRAY_TO_WHATSAPP_SEND_WM_CLOSE received", __func__);

		LogString(MODULE_NAME "::%s: Send WM_CLOSE to WhatsApp.", __func__);
		// NOTE: lParam/wParam are not used in WM_CLOSE.
		return CallWindowProc(_originalWndProc, hwnd, WM_CLOSE, 0, 0);
	}

	// Call the original window-proc.
	return CallWindowProc(_originalWndProc, hwnd, uMsg, wParam, lParam);
}

/**
 * @brief Mouse-hook
 * NOTE: This hook is also used to inject the dll so this can not be removed currently
*/
LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode >= 0) {
		if (wParam == WM_LBUTTONDOWN) {
			LogString(MODULE_NAME "::%s: WM_LBUTTONDOWN received", __func__);

			MOUSEHOOKSTRUCT* mhs = (MOUSEHOOKSTRUCT*)lParam;
			RECT rect;
			GetWindowRect(mhs->hwnd, &rect);

			// Modify rect to cover the X(close) button.
			rect.left = rect.right - 46;
			rect.bottom = rect.top + 35;

			bool mouseOnClosebutton = PtInRect(&rect, mhs->pt);

			if (mouseOnClosebutton) {
				SendMessageToWhatsappTray(WM_WA_CLOSE_BUTTON_PRESSED);

				// Returning nozero blocks the message frome being sent to the application.
				return 1;
			}
		}
	}

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
	_tempWindowHandle = NULL;

	_searchedWindowTitle = searchedWindowTitle;
	EnumWindows(FindTopLevelWindowhandleWithNameCallback, NULL);

	return _tempWindowHandle;
}

/**
 * @brief Search the window-handle for the current process
 *
 * Be carefull, one process can have multiple windows.
 * Thats why the window-title also gets checked.
 */
static BOOL CALLBACK FindTopLevelWindowhandleWithNameCallback(HWND hwnd, LPARAM lParam)
{
	DWORD processId;
	auto result = GetWindowThreadProcessId(hwnd, &processId);

	// Check processId and Title
	// Already observerd an instance where the processId alone lead to an false match!
	if (_processID == processId) {
		// Found the matching processId

		if (WindowhandleIsToplevelWithTitle(hwnd, _searchedWindowTitle)) {
			// The window is the root-window

			_tempWindowHandle = hwnd;

			// Stop enumeration
			return false;
		}
	}

	// Continue enumeration
	return true;
}

/**
 * @brief Returns true if the window-handle and the title match
 */
static bool WindowhandleIsToplevelWithTitle(HWND hwnd, std::string searchedWindowTitle)
{
	if (hwnd != GetAncestor(hwnd, GA_ROOT)) {
		TraceString(MODULE_NAME "WindowhandleIsToplevelWithTitle: Window is not a toplevel-window.");
		return false;
	}

	auto windowTitle = GetWindowTitle(hwnd);
	if (windowTitle.compare(searchedWindowTitle) != 0) {
		TraceString(std::string(MODULE_NAME "WindowhandleIsToplevelWithTitle: windowTitle='") + windowTitle + "' does not match '" WHATSAPP_CLIENT_NAME "'." + windowTitle);
		return false;
	}

	return true;
}

/**
 * To verify that we found the correct window:
 * 1. Check if its a toplevel-window (below desktop)
 * 2. Match the window-name with whatsapp.
 */
static bool IsTopLevelWhatsApp(HWND hwnd)
{
	return WindowhandleIsToplevelWithTitle(hwnd, WHATSAPP_CLIENT_NAME);
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

/**
 * @brief Send message to WhatsappTray.
 */
static bool SendMessageToWhatsappTray(UINT message, WPARAM wParam, LPARAM lParam)
{
	return PostMessage(FindWindow(NAME, NAME), message, wParam, lParam);
}

static void TraceString(std::string traceString)
{
	SocketSendMessage(LOGGER_IP, LOGGER_PORT, traceString.c_str());

//#ifdef _DEBUG
//	OutputDebugStringA(traceString.c_str());
//#endif
}

static void TraceStream(std::ostringstream& traceBuffer)
{
	SocketSendMessage(LOGGER_IP, LOGGER_PORT, traceBuffer.str().c_str());
	traceBuffer.clear();
	traceBuffer.str(std::string());

//#ifdef _DEBUG
//	OutputDebugStringA(traceBuffer.str().c_str());
//	traceBuffer.clear();
//	traceBuffer.str(std::string());
//#endif
}
