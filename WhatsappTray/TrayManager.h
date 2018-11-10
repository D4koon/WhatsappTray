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

#include <stdint.h>

class TrayManager
{
public:
	TrayManager(HWND _hwndWhatsappTray);
	void MinimizeWindowToTray(HWND hwnd);
	void CloseWindowFromTray(HWND hwnd);
	void RemoveTrayIcon(HWND hwnd);
	void RemoveFromTray(int32_t index);
	void RestoreAllWindowsFromTray();
	void RestoreFromTray(uintptr_t index);
	void RestoreWindowFromTray(HWND hwnd);
	void RefreshWindowInTray(HWND hwnd);
	HWND GetHwndFromIndex(uintptr_t index);
	~TrayManager() { }
private:
	static const int MAXTRAYITEMS = 64;

	/// The windows that are currently minimized to tray.
	//std::vector<HWND> trayItems;
	HWND _hwndItems[MAXTRAYITEMS];
	HWND _hwndWhatsappTray;

	void AddWindowToTray(HWND hwnd);
	void CreateTrayIcon(int32_t index, HWND hwnd);
	int32_t GetIndexFromWindowHandle(HWND hwnd);
};

