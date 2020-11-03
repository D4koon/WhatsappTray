/*
*
* WhatsappTray
* Copyright (C) 1998-2018  Sebastian Amann
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

#pragma once

#include <objidl.h>
#include <gdiplus.h>
#pragma comment (lib,"Gdiplus.lib")

#include <stdint.h>

class TrayManager
{
public:
	TrayManager(const HWND hwndWhatsappTray);
	~TrayManager() { }
	void MinimizeWindowToTray(const HWND hwnd);
	void CloseWindowFromTray(const HWND hwnd);
	void RemoveTrayIcon(const HWND hwnd);
	void RemoveFromTray(const int32_t index);
	void RestoreAllWindowsFromTray();
	void RestoreWindowFromTray(const HWND hwnd);
	void SetIcon(const HWND hwnd, LPCSTR text);
	void RegisterWindow(const HWND hwnd);
private:
	static const int MAXTRAYITEMS = 64;

	/// The windows that are currently minimized to tray.
	HWND _hwndWhatsappTray;
	HWND _hwndItems[MAXTRAYITEMS];

	void CreateTrayIcon(const int32_t index, const HWND hwnd);
	int32_t GetIndexFromWindowHandle(const HWND hwnd);
	HICON AddTextToIcon(HICON hBackgroundIcon, LPCSTR text);
};

