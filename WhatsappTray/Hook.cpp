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

#include <windows.h>
#include <iostream>
#include <fstream>
#include <sstream>

// Problems with OutputDebugString:
// Had problems that the messages from OutputDebugString in the hook-functions did not work anymore after some playing arround with old versions, suddenly it worked again.
// Maybe its best to restart pc and hope that it will be fixed, when this happens again...

// NOTES:
// It looks like as if the contrlos like _ and X for minimize or close are custom controls and do not trigger all normal messgages
// For example SC_CLOSE seems to be not sent when clicking the X

// Use DebugView (https://docs.microsoft.com/en-us/sysinternals/downloads/debugview) to see OutputDebugString() messages.

#undef MODULE_NAME
#define MODULE_NAME "WhatsappTrayHook::"

static HHOOK _hWndProc = NULL;
static HHOOK _mouseProc = NULL;
static HHOOK _cbtProc = NULL;

static DWORD _processIdWhatsapp;
static HWND _hwndWhatsapp;
static WNDPROC _originalWndProc;

LRESULT APIENTRY EditSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam);
LRESULT CALLBACK CallWndRetProc(int nCode, WPARAM wParam, LPARAM lParam);
bool IsTopLevelWhatsApp(HWND hwnd);
std::string GetWindowTitle(HWND hwnd);
bool SendMessageToWhatsappTray(HWND hwnd, UINT message);
void TraceString(std::string traceString);
void TraceStream(std::ostringstream& traceBuffer);

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	std::ostringstream traceBuffer;

	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:

		OutputDebugString("Hook.dll ATTACHED!");

		// Find the WhatsApp window-handle that we need to replace the window-proc
		_processIdWhatsapp = GetCurrentProcessId();

		traceBuffer << "_processIdWhatsapp: 0x" << std::uppercase << std::hex << _processIdWhatsapp;
		TraceStream(traceBuffer);

		EnumWindows(EnumWindowsCallback, NULL);

		traceBuffer << "Attached in window '" << GetWindowTitle(_hwndWhatsapp) << "'" << "_hwndWhatsapp: 0x" << std::uppercase << std::hex << _hwndWhatsapp;
		TraceStream(traceBuffer);

		// Replace the original window-proc with our own. This is called subclassing.
		// Our window-proc will call after the processing the original window-proc.
		_originalWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(_hwndWhatsapp, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc));
		
		break;
	case DLL_PROCESS_DETACH:
		OutputDebugString("Hook.dll DETACHED!");

		// Remove our window-proc from the chain by setting the original window-proc.
		SetWindowLongPtr(_hwndWhatsapp, GWLP_WNDPROC, (LONG_PTR)_originalWndProc);
		break;
	}
	return TRUE;
}

/**
 * @brief This is the new main window-proc. After we are done we call the original window-proc.
 * 
 * This method has the advantage over SetWindowsHookEx(WH_CALLWNDPROC... that here we can skip or modifie messages.
 */
LRESULT APIENTRY EditSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	std::ostringstream traceBuffer;

#ifdef _DEBUG
	if (uMsg != WM_GETTEXT) {
		// Dont print WM_GETTEXT so there is not so much "spam"

		traceBuffer << MODULE_NAME << __func__ << ": " << WindowsMessage::GetString(uMsg) << "(0x" << std::uppercase << std::hex << uMsg << ") ";
		traceBuffer << "windowName='" << GetWindowTitle(hwnd) << "' ";
		traceBuffer << "hwnd='" << std::uppercase << std::hex << hwnd << "' ";
		traceBuffer << "wParam='" << std::uppercase << std::hex << wParam << "' ";
		TraceStream(traceBuffer);
	}
#endif

	if (uMsg == WM_SYSCOMMAND) {
		// Description for WM_SYSCOMMAND: https://msdn.microsoft.com/de-de/library/windows/desktop/ms646360(v=vs.85).aspx
		if (wParam == SC_MINIMIZE) {
			TraceString(MODULE_NAME "SC_MINIMIZE");

			// Here i check if the windowtitle matches. Vorher hatte ich das Problem das sich Chrome auch minimiert hat.
			if (IsTopLevelWhatsApp(hwnd)) {
				SendMessageToWhatsappTray(hwnd, WM_ADDTRAY);
			}
		}
	} else if (uMsg == WM_NCDESTROY) {
		uintptr_t handle1 = reinterpret_cast<uintptr_t>(hwnd);
		uintptr_t whatsappTrayWindowHandle = reinterpret_cast<uintptr_t>(FindWindow(NAME, NAME));
		traceBuffer << MODULE_NAME << __func__ << ": " << "WM_NCDESTROY whatsappTrayWindowHandle='" << whatsappTrayWindowHandle << "'";
		TraceStream(traceBuffer);

		if (IsTopLevelWhatsApp(hwnd)) {
			auto successfulSent = SendMessageToWhatsappTray(hwnd, WM_WHAHTSAPP_CLOSING);
			if (successfulSent) {
				TraceString(MODULE_NAME "WM_WHAHTSAPP_CLOSING successful sent.");
			}
		}
	} else if (uMsg == WM_CLOSE) {
		// This happens when alt + f4 is pressed.
		traceBuffer << MODULE_NAME << __func__ << ": WM_CLOSE received.";
		TraceStream(traceBuffer);

		// Notify WhatsappTray and if it wants to close it can do so...
		SendMessageToWhatsappTray(hwnd, WM_WHATSAPP_TO_WHATSAPPTRAY_RECEIVED_WM_CLOSE);

		traceBuffer << MODULE_NAME << __func__ << ": WM_CLOSE blocked.";
		// Block WM_CLOSE
		return 0;
	} else if (uMsg == WM_WHATSAPPTRAY_TO_WHATSAPP_SEND_WM_CLOSE) {
		// This message is defined by me and should only come from WhatsappTray.
		// It more or less replaces WM_CLOSE which is now always blocked...
		// To have a way to still send WM_CLOSE this message was made.
		traceBuffer << MODULE_NAME << __func__ << ": WM_WHATSAPPTRAY_TO_WHATSAPP_SEND_WM_CLOSE received.";
		TraceStream(traceBuffer);

		traceBuffer << MODULE_NAME << __func__ << ": Send WM_CLOSE to WhatsApp.";
		// NOTE: lParam/wParam are not used in WM_CLOSE.
		return CallWindowProc(_originalWndProc, hwnd, WM_CLOSE, 0, 0);
	}

	// Call the original window-proc.
	return CallWindowProc(_originalWndProc, hwnd, uMsg, wParam, lParam);
}

/**
 * Search for the main whatsapp window
 * 
 * Be carefull, one process can have multiple windows.
 * Thats why the window-title also gets checked.
 */
BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam)
{
	DWORD processId;
	auto result = GetWindowThreadProcessId(hwnd, &processId);

	// Check processId and Title
	// Already observerd an instance where the processId alone lead to an false match!
	if (_processIdWhatsapp == processId && GetWindowTitle(hwnd).compare(WHATSAPP_CLIENT_NAME) == 0)
	{
		// Found the matching processId
		_hwndWhatsapp = hwnd;

		// Stop enumeration
		return false;
	}

	// Continue enumeration
	return true;
}

/**
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
	if (nCode < HC_ACTION) {
		return CallNextHookEx(NULL, nCode, wParam, lParam);
	}
	//CWPSTRUCT* msg = reinterpret_cast<CWPSTRUCT*>(lParam);

	return CallNextHookEx(NULL, nCode, wParam, lParam);
}

// This function is should not be active in normal distribution-version
// Only for debugging. The call to SetWindowsHookEx(WH_CALLWNDPROC, ... to use this function instead of the "normal" one.
LRESULT CALLBACK CallWndRetProcDebug(int nCode, WPARAM wParam, LPARAM lParam)
{
	//if (nCode >= 0)
	{
		CWPSTRUCT *msg = (CWPSTRUCT*)lParam;

		//if (msg->hwnd == (HWND)0x00120A42)
		if (msg->hwnd == FindWindow(NULL, WHATSAPP_CLIENT_NAME)) {

			if (msg->message == 0x2 || msg->message == 528 || msg->message == 70 || msg->message == 71 || msg->message == 28 || msg->message == 134 || msg->message == 6 || msg->message == 641 || msg->message == 642 || msg->message == 7 || msg->message == 533 || msg->message == 144 || msg->message == 70 || msg->message == 71 || msg->message == 134 || msg->message == 6 || msg->message == 28 || msg->message == 8 || msg->message == 641 || msg->message == 642 || msg->message == 626 || msg->message == 2) {
				
				// WM_DESTROY

				//std::ostringstream filename2;
				//filename2 << "C:/hooktest/HWND_" << msg->hwnd << ".txt";

				//std::ofstream myfile;
				//myfile.open(filename2.str().c_str(), std::ios::app);

				////LONG wndproc = GetWindowLong(msg->hwnd, -4);

				//myfile << "\nblocked (" << msg->message << ")" << msg->wParam;

				//MSG msgr;
				//PeekMessage(&msgr, msg->hwnd, 0, 0, PM_REMOVE);

				return 0;
			}

			//if (msg->message == WM_SYSCOMMAND)
			{
				std::ostringstream filename;
				filename << "C:/hooktest/HWND_" << msg->hwnd << ".txt";

				std::ofstream myfile;
				myfile.open(filename.str().c_str(), std::ios::app);

				myfile << "\nWM_SYSCOMMAND (" << msg->message << ")" << msg->wParam;

				if (msg->wParam == 61472) {
					myfile << "\nMinimize";
					myfile << "\nHWND to Hookwindow:" << FindWindow(NAME, NAME);

					PostMessage(FindWindow(NAME, NAME), WM_ADDTRAY, 0, reinterpret_cast<LPARAM>(msg->hwnd));
				}
				myfile.close();
			}

		}

	}

	return CallNextHookEx(NULL, nCode, wParam, lParam);
}

LRESULT CALLBACK CBTProc(_In_ int nCode, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
	if (nCode == HCBT_DESTROYWND) {
		std::ostringstream filename;
		filename << "C:/hooktest/HWND_" << (HWND)wParam << ".txt";

		std::ofstream myfile;
		myfile.open(filename.str().c_str(), std::ios::app);

		myfile << "\nblocked (";


		return 1;
	}
	return CallNextHookEx(_cbtProc, nCode, wParam, lParam);
}

LRESULT CALLBACK MouseProc(_In_ int nCode, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
	if (nCode >= 0) {
		if (wParam == WM_LBUTTONDOWN) {
			//std::ostringstream filename2;
			//filename2 << "C:/hooktest/HWND_" << (HWND)wParam << ".txt";

			//std::ofstream myfile;
			//myfile.open(filename2.str().c_str(), std::ios::app);

			MOUSEHOOKSTRUCT *mhs = (MOUSEHOOKSTRUCT*)lParam;

			RECT rect;
			GetWindowRect(mhs->hwnd, &rect);

			// Modify rect to cover the X(close) button.
			rect.left = rect.right - 46;
			rect.bottom = rect.top + 35;

			bool mouseOnClosebutton = PtInRect(&rect, mhs->pt);

			if (mouseOnClosebutton) {
				//OutputDebugStringA(MODULE_NAME "Closebutton mousedown");
				//myfile << "\nMinimize";
				//myfile << "\nHWND to Hookwindow:" << FindWindow(NAME, NAME);

				PostMessage(FindWindow(NAME, NAME), WM_ADDTRAY, 0, (LPARAM)mhs->hwnd);

				// Returning nozero blocks the message frome being sent to the application.
				return 1;
			}
		}
	}

	return CallNextHookEx(_cbtProc, nCode, wParam, lParam);
}

/**
 * To verify that we found the correct window:
 * 1. Check if its a toplevel-window (below desktop)
 * 2. Match the window-name with whatsapp.
 */
bool IsTopLevelWhatsApp(HWND hwnd)
{
	if (hwnd != GetAncestor(hwnd, GA_ROOT)) {
		TraceString(MODULE_NAME "IsTopLevelWhatsApp: Window is not a toplevel-window.");
		return false;
	}

	auto windowName = GetWindowTitle(hwnd);
	if (windowName.compare(WHATSAPP_CLIENT_NAME) != 0) {
		TraceString(std::string(MODULE_NAME "IsTopLevelWhatsApp: windowName='") + windowName + "' does not match '" WHATSAPP_CLIENT_NAME "'." + windowName);
		return false;
	}

	return true;
}

/**
 * Gets the text of a window.
 */
std::string GetWindowTitle(HWND hwnd)
{
	char windowNameBuffer[2000];
	GetWindowTextA(hwnd, windowNameBuffer, sizeof(windowNameBuffer));

	return std::string(windowNameBuffer);
}

/**
 * Send message to WhatsappTray.
 */
bool SendMessageToWhatsappTray(HWND hwnd, UINT message)
{
	return PostMessage(FindWindow(NAME, NAME), message, 0, reinterpret_cast<LPARAM>(hwnd));
}

void TraceString(std::string traceString)
{
#ifdef _DEBUG
	OutputDebugStringA(traceString.c_str());
#endif
}

void TraceStream(std::ostringstream& traceBuffer)
{
#ifdef _DEBUG
	OutputDebugStringA(traceBuffer.str().c_str());
	traceBuffer.clear();
	traceBuffer.str(std::string());
#endif
}

/**
 * Registers the hook.
 *
 * @param hLib
 * @param threadId If threadId 0 is passed, it is a global Hook.
 * @param closeToTray
 * @return
 */
BOOL DLLIMPORT RegisterHook(HMODULE hLib, DWORD threadId, bool closeToTray)
{
	if (!closeToTray) {
		_hWndProc = SetWindowsHookEx(WH_CALLWNDPROC, (HOOKPROC)CallWndRetProc, hLib, threadId);
		if (_hWndProc == NULL) {
			OutputDebugStringA(MODULE_NAME "RegisterHook() - Error Creation Hook _hWndProc\n");
			UnRegisterHook();
			return FALSE;
		}
	}

	if (closeToTray) {
		_mouseProc = SetWindowsHookEx(WH_MOUSE, (HOOKPROC)MouseProc, hLib, threadId);
		if (_mouseProc == NULL) {
			OutputDebugStringA(MODULE_NAME "RegisterHook() - Error Creation Hook _hWndProc\n");
			UnRegisterHook();
			return FALSE;
		}
	}

	return TRUE;
}

void DLLIMPORT UnRegisterHook()
{
	if (_hWndProc) {
		UnhookWindowsHookEx(_hWndProc);
		_hWndProc = NULL;
	}
	if (_mouseProc) {
		UnhookWindowsHookEx(_mouseProc);
		_mouseProc = NULL;
	}
}

