// ****************************************************************************
// 
// WhatsappTray
// Copyright (C) 1998-2016  Sebastian Amann, Nikolay Redko, J.D. Purcell
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
// 
// ****************************************************************************

#include <windows.h>
#include "WhatsappTray.h"
#include <iostream>
#include <fstream>
#include <sstream>

static HHOOK _hMouse = NULL;
static HHOOK _hWndProc = NULL;
static HWND _hLastHit = NULL;

// Only works for 32-bit apps or 64-bit apps depending on whether this is complied as 32-bit or 64-bit (Whatsapp is a 64Bit-App)
LRESULT CALLBACK CallWndRetProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode >= 0)
	{
		CWPRETSTRUCT *msg = (CWPRETSTRUCT*)lParam;

		if (msg->message == WM_SYSCOMMAND)
		{
			if (msg->wParam == 61472)
			{
				// Ich prüfe hier noch ob der Fenstertitel übereinstimmt. Vorher hatte ich das Problem das sich Chrome auch minimiert hat.
				// Ich könnte hier auch noch die klasse checken, das hat dann den vorteil, das es noch genauer ist.
				// Sollte die Klasse von Whatsapp aus aber umbenannt werden muss ich hier wieder nachbesser.
				// -> Daher lass ichs erstmal so...
				if (msg->hwnd == FindWindow(NULL, WHATSAPP_CLIENT_NAME))
				{
					PostMessage(FindWindow(NAME, NAME), WM_ADDTRAY, 0, (LPARAM)msg->hwnd);
				}
			}
		}

	}
	
	return CallNextHookEx(_hWndProc, nCode, wParam, lParam);
}

LRESULT CALLBACK CallWndRetProcDebug(int nCode, WPARAM wParam, LPARAM lParam) {
	if (nCode >= 0) {
		CWPRETSTRUCT *msg = (CWPRETSTRUCT*)lParam;

		if (msg->hwnd == (HWND)0x10404)
		{
			if (msg->message == WM_SYSCOMMAND)
			{
				std::ostringstream filename2;
				filename2 << "C:/hooktest/HWND_" << msg->hwnd << ".txt";

				std::ofstream myfile;
				myfile.open(filename2.str().c_str(), std::ios::app);

				myfile << "\nWM_SYSCOMMAND (" << msg->message << ")" << msg->wParam;

				if (msg->wParam == 61472)
				{
					myfile << "\nMinimize";
					myfile << "\nHWND to Hookwindow:" << FindWindow(NAME, NAME);

					PostMessage(FindWindow(NAME, NAME), WM_ADDTRAY, 0, (LPARAM)msg->hwnd);
					//PostMessage(FindWindow(NAME, NAME), WM_REFRTRAY, 0, (LPARAM)msg->hwnd);
				}
				myfile.close();
			}

		}

	}

	return CallNextHookEx(_hWndProc, nCode, wParam, lParam);
}

BOOL DLLIMPORT RegisterHook(HMODULE hLib)
{
	_hWndProc = SetWindowsHookEx(WH_CALLWNDPROCRET, (HOOKPROC)CallWndRetProc, hLib, 0);
	if (_hWndProc == NULL)
	{
		OutputDebugString(L"RegisterHook() - Error Creation Hook _hWndProc\n");
		UnRegisterHook();
		return FALSE;
	}
	return TRUE;
}

void DLLIMPORT UnRegisterHook()
{
	if (_hWndProc)
	{
		UnhookWindowsHookEx(_hWndProc);
		_hWndProc = NULL;
	}
}
