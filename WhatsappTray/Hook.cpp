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
#include "WindowsMessage.h"

#include <iostream>
#include <fstream>
#include <sstream>

#undef MODULE_NAME
#define MODULE_NAME "[WhatsappTrayHook] "

static HHOOK _hWndProc = NULL;
static HHOOK _mouseProc = NULL;
static HHOOK _cbtProc = NULL;

// Using this variable i am able to enable/disable tracing. I could do that by using a self defined windowmessage and send it to the application. (not implemented yet)
#ifdef _DEBUG
static bool traceActivated = true;
#else
static bool traceActivated = false;
#endif

void TraceString(std::string traceString)
{
	if (traceActivated) {
		OutputDebugStringA(traceString.c_str());
	}
}

/**
 * To verify that we found the correct window:
 * 1. Check if its a toplevel-window (below desktop)
 * 2. Match the window-name with whatsapp.
 */
bool IsTopLevelWhatsApp(HWND hwnd)
{
	char windowNameBuffer[2000];
	GetWindowTextA(hwnd, windowNameBuffer, sizeof(windowNameBuffer));

	if (hwnd != GetAncestor(hwnd, GA_ROOT)) {
		TraceString(MODULE_NAME " - Window is not a toplevel-window.");
		return false;
	}
	if (strstr(windowNameBuffer, WHATSAPP_CLIENT_NAME) == false) {
		TraceString(std::string(MODULE_NAME " - Window-name '") + windowNameBuffer + "' does not match '" WHATSAPP_CLIENT_NAME "'." + windowNameBuffer);
		return false;
	}
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

	CWPSTRUCT* msg = reinterpret_cast<CWPSTRUCT*>(lParam);

	char buffer[2000];
	//char windowNameBuffer[2000];
	//GetWindowTextA(msg->hwnd, windowNameBuffer, sizeof(windowNameBuffer));
	//sprintf_s(buffer, sizeof(buffer), MODULE_NAME "CallWndRetProc windowNameBuffer='%s' msg->hwnd='0x%lX' message=%s(0x%lX) wParam=0x%llX", windowNameBuffer, GetAncestor(msg->hwnd, GA_ROOT), WindowsMessage::GetString(msg->message).c_str(), msg->message, msg->wParam);
	//TraceString(buffer);

	if (msg->message == WM_SYSCOMMAND) {
		// Description for WM_SYSCOMMAND: https://msdn.microsoft.com/de-de/library/windows/desktop/ms646360(v=vs.85).aspx
		if (msg->wParam == SC_MINIMIZE) {
			TraceString(MODULE_NAME "SC_MINIMIZE");

			// Here i check if the windowtitle matches. Vorher hatte ich das Problem das sich Chrome auch minimiert hat.
			if (IsTopLevelWhatsApp(msg->hwnd)) {
				PostMessage(FindWindow(NAME, NAME), WM_ADDTRAY, 0, reinterpret_cast<LPARAM>(msg->hwnd));
			}
		}
	} else if (msg->message == WM_NCDESTROY) {
		uintptr_t handle1 = reinterpret_cast<uintptr_t>(msg->hwnd);
		uintptr_t whatsappTrayWindowHandle = reinterpret_cast<uintptr_t>(FindWindow(NAME, NAME));
		sprintf_s(buffer, sizeof(buffer), MODULE_NAME "WM_NCDESTROY hwnd=0x%llX whatsappTrayWindowHandle=0x%llX", handle1, whatsappTrayWindowHandle);
		TraceString(buffer);

		if (IsTopLevelWhatsApp(msg->hwnd)) {
			bool successfulSent = PostMessage(FindWindow(NAME, NAME), WM_WHAHTSAPP_CLOSING, 0, reinterpret_cast<LPARAM>(msg->hwnd));
			if (successfulSent) {
				TraceString(MODULE_NAME "WM_WHAHTSAPP_CLOSING successful sent.");
			}
		}
	}

	return CallNextHookEx(NULL, nCode, wParam, lParam);
}

LRESULT CALLBACK CallWndRetProcDebug(int nCode, WPARAM wParam, LPARAM lParam)
{
	//if (nCode >= 0)
	{
		CWPSTRUCT *msg = (CWPSTRUCT*)lParam;

		//if (msg->hwnd == (HWND)0x00120A42)
		if (msg->hwnd == FindWindow(NULL, WHATSAPP_CLIENT_NAME)) {
			//
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

