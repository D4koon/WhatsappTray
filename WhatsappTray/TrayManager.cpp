/*
 * 
 * WhatsappTray
 * Copyright (C) 1998-2018 Sebastian Amann
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

#include "stdafx.h"

#include "TrayManager.h"
#include "Helper.h"
#include "Logger.h"
#include "WhatsappTray.h"

#undef MODULE_NAME
#define MODULE_NAME "TrayManager::"

TrayManager::TrayManager(HWND _hwndWhatsappTray)
	: _hwndWhatsappTray(_hwndWhatsappTray)
{
}

void TrayManager::MinimizeWindowToTray(HWND hwnd)
{
	Logger::Info(MODULE_NAME "MinimizeWindowToTray(0x%llX)", reinterpret_cast<uintptr_t>(hwnd));

	// Don't minimize MDI child windows
	if ((UINT)GetWindowLongPtr(hwnd, GWL_EXSTYLE) & WS_EX_MDICHILD) return;

	// If hwnd is a child window, find parent window (e.g. minimize button in
	// Office 2007 (ribbon interface) is in a child window)
	if ((UINT)GetWindowLongPtr(hwnd, GWL_STYLE) & WS_CHILD) {
		hwnd = GetAncestor(hwnd, GA_ROOT);
	}

	AddWindowToTray(hwnd);

	// Hide window
	ShowWindow(hwnd, SW_HIDE);
}

/**
 * If a window is already in the tray nothing will be done.
*/
void TrayManager::AddWindowToTray(HWND hwnd)
{
	// Add icon to tray if it's not already there
	if (GetIndexFromWindowHandle(hwnd) != -1) {
		Logger::Warning(MODULE_NAME "AddWindowToTray() - Trying to send a window to tray that should already be minimized. This should not happen.");
		return;
	}

	// Search first empty spot.
	int32_t newIndex = -1;
	for (int32_t index = 0; index < MAXTRAYITEMS; index++) {
		if (_hwndItems[index] == NULL) {
			newIndex = index;
		}
	}

	if (newIndex == -1) {
		Logger::Error(MODULE_NAME "AddWindowToTray() - Tray is full!");
	}

	_hwndItems[newIndex] = hwnd;
	CreateTrayIcon(newIndex, hwnd);
}

void TrayManager::CreateTrayIcon(int32_t index, HWND hwnd)
{
	Logger::Info(MODULE_NAME "CreateTrayIcon(%d)", index);

	NOTIFYICONDATA nid;
	ZeroMemory(&nid, sizeof(nid));
	nid.cbSize = NOTIFYICONDATA_V2_SIZE;
	nid.hWnd = _hwndWhatsappTray;
	nid.uID = static_cast<UINT>(index);
	nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	nid.uCallbackMessage = WM_TRAYCMD;
	nid.hIcon = Helper::GetWindowIcon(hwnd);
	GetWindowText(hwnd, nid.szTip, sizeof(nid.szTip) / sizeof(nid.szTip[0]));
	nid.uVersion = NOTIFYICON_VERSION;
	Shell_NotifyIcon(NIM_ADD, &nid);
	Shell_NotifyIcon(NIM_SETVERSION, &nid);
}

void TrayManager::CloseWindowFromTray(HWND hwnd)
{
	Logger::Info(MODULE_NAME "CloseWindowFromTray()");

	// Use PostMessage to avoid blocking if the program brings up a dialog on exit.
	// Also, Explorer windows ignore WM_CLOSE messages from SendMessage.
	PostMessage(hwnd, WM_CLOSE, 0, 0);

	Sleep(50);
	if (IsWindow(hwnd)) Sleep(50);

	if (!IsWindow(hwnd)) {
		// Closed successfully
		RemoveTrayIcon(hwnd);
	}
}

void TrayManager::RemoveTrayIcon(HWND hwnd)
{
	int32_t index = GetIndexFromWindowHandle(hwnd);
	if (index == -1) { return; }
	RemoveFromTray(index);
}

void TrayManager::RemoveFromTray(int32_t index)
{
	Logger::Info(MODULE_NAME "RemoveFromTray(%d)", index);

	NOTIFYICONDATA nid;
	ZeroMemory(&nid, sizeof(nid));
	nid.cbSize = NOTIFYICONDATA_V2_SIZE;
	nid.hWnd = _hwndWhatsappTray;
	nid.uID = (UINT)index;
	Shell_NotifyIcon(NIM_DELETE, &nid);
	_hwndItems[index] = NULL;
}

void TrayManager::RestoreAllWindowsFromTray()
{
	for (int i = 0; i < MAXTRAYITEMS; i++) {
		if (_hwndItems[i]) {
			RestoreFromTray(i);
		}
	}
}

void TrayManager::RestoreFromTray(uintptr_t index)
{
	HWND hwnd = GetHwndFromIndex(index);
	RestoreWindowFromTray(hwnd);
}

void TrayManager::RestoreWindowFromTray(HWND hwnd)
{
	ShowWindow(hwnd, SW_RESTORE);
	SetForegroundWindow(hwnd);
	RemoveTrayIcon(hwnd);
}

void TrayManager::RefreshWindowInTray(HWND hwnd)
{
	int32_t index = GetIndexFromWindowHandle(hwnd);
	if (index == -1) {
		return;
	}
	if (!IsWindow(hwnd) || IsWindowVisible(hwnd)) {
		RemoveFromTray(index);
	} else {
		NOTIFYICONDATA nid;
		ZeroMemory(&nid, sizeof(nid));
		nid.cbSize = NOTIFYICONDATA_V2_SIZE;
		nid.hWnd = _hwndWhatsappTray;
		nid.uID = (UINT)index;
		nid.uFlags = NIF_TIP;
		GetWindowText(hwnd, nid.szTip, sizeof(nid.szTip) / sizeof(nid.szTip[0]));
		Shell_NotifyIcon(NIM_MODIFY, &nid);
	}
}

HWND TrayManager::GetHwndFromIndex(uintptr_t index)
{
	return _hwndItems[index];
}

int32_t TrayManager::GetIndexFromWindowHandle(HWND hwnd)
{
	if (hwnd == NULL) {
		return -1;
	}
	for (int i = 0; i < MAXTRAYITEMS; i++) {
		if (_hwndItems[i] == hwnd) {
			return i;
		}
	}
	return -1;
}
