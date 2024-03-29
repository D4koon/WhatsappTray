/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright(C) 1998 - 2018 WhatsappTray Sebastian Amann */

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
	void UpdateIcon(uint64_t id);
	void RegisterWindow(const HWND hwnd);
private:
	static const int MAXTRAYITEMS = 64;

	/// The windows that are currently minimized to tray.
	HWND _hwndWhatsappTray;
	HWND _hwndItems[MAXTRAYITEMS];

	void AddTrayIcon(const int32_t index, const HWND hwnd);
	NOTIFYICONDATA CreateTrayIconData(const int32_t index, HICON trayIcon);
	int32_t GetIndexFromWindowHandle(const HWND hwnd);
	HICON AddImageOverlayToIcon(HICON hBackgroundIcon, LPCSTR text);
};

