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
				PostMessage(FindWindow(NAME, NAME), WM_ADDTRAY, 0, (LPARAM)msg->hwnd);
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
