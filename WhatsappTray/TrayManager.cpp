/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright(C) 1998 - 2018 WhatsappTray Sebastian Amann */

#include "stdafx.h"

#include "TrayManager.h"
#include "Helper.h"
#include "Logger.h"
#include "WhatsappTray.h"
#include "SharedDefines.h"
#include <filesystem>

using namespace Gdiplus;

#undef MODULE_NAME
#define MODULE_NAME "TrayManager::"

TrayManager::TrayManager(const HWND hwndWhatsappTray)
	: _hwndWhatsappTray(hwndWhatsappTray)
	, _hwndItems { 0 }
{
	Logger::Info(MODULE_NAME "ctor() - Creating TrayManger.");
}

void TrayManager::MinimizeWindowToTray(const HWND hwnd)
{
	Logger::Info(MODULE_NAME "MinimizeWindowToTray(0x%08X)", reinterpret_cast<uintptr_t>(hwnd));

	// Hide window
	// NOTE: The SW_MINIMIZE is important for the case when close-to-tray-feature is used:
	// Without it, a maximized window is not restored as maximized.
	// This means Windows only remebers the size when it was minimized before. This is done implicied when the minimize-button is used for minimizing to tray
	// See also https://github.com/D4koon/WhatsappTray/issues/10
	ShowWindow(hwnd, SW_MINIMIZE);
	ShowWindow(hwnd, SW_HIDE);
}

/**
 * If a window is already in the tray nothing will be done.
 */
void TrayManager::RegisterWindow(const HWND hwnd)
{
	// Add icon to tray if it's not already there
	if (GetIndexFromWindowHandle(hwnd) != -1) {
		Logger::Warning(MODULE_NAME "RegisterWindow() - Trying to send a window to tray that should already be minimized. This should not happen.");
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
		Logger::Error(MODULE_NAME "RegisterWindow() - Tray is full!");
	}

	_hwndItems[newIndex] = hwnd;
	AddTrayIcon(newIndex, hwnd);
}

void TrayManager::AddTrayIcon(const int32_t index, const HWND hwnd)
{
	Logger::Info(MODULE_NAME "AddTrayIcon(%d)", index);

	auto nid = CreateTrayIconData(index, Helper::GetWindowIcon(hwnd));

	Shell_NotifyIcon(NIM_ADD, &nid);
	Shell_NotifyIcon(NIM_SETVERSION, &nid);
}

void TrayManager::CloseWindowFromTray(const HWND hwnd)
{
	Logger::Info(MODULE_NAME "CloseWindowFromTray() 0x%08X", hwnd);

	// Use PostMessage to avoid blocking if the program brings up a dialog on exit.
	// NOTE: WM_WHATSAPPTRAY_TO_WHATSAPP_SEND_WM_CLOSE is a special message i made because WM_CLOSE is always blocked by the hook
	PostMessage(hwnd, WM_WHATSAPPTRAY_TO_WHATSAPP_SEND_WM_CLOSE, 0, 0);

	Sleep(50);
	if (IsWindow(hwnd)) {
		Sleep(50);
	}

	if (!IsWindow(hwnd)) {
		// Closed successfully
		RemoveTrayIcon(hwnd);
	}
}

void TrayManager::RemoveTrayIcon(const HWND hwnd)
{
	int32_t index = GetIndexFromWindowHandle(hwnd);
	if (index == -1) { return; }
	RemoveFromTray(index);
}

void TrayManager::RemoveFromTray(const int32_t index)
{
	Logger::Info(MODULE_NAME "RemoveFromTray(%d)", index);

	NOTIFYICONDATA nid { 0 };
	nid.cbSize = NOTIFYICONDATA_V2_SIZE;
	nid.hWnd = _hwndWhatsappTray;
	nid.uID = (UINT)index;

	Shell_NotifyIcon(NIM_DELETE, &nid);

	_hwndItems[index] = NULL;
}

void TrayManager::RestoreAllWindowsFromTray()
{
	for (int i = 0; i < MAXTRAYITEMS; i++) {
		HWND itHwnd = _hwndItems[i];
		if (itHwnd) {
			RestoreWindowFromTray(itHwnd);
		}
	}
}

void TrayManager::RestoreWindowFromTray(const HWND hwnd)
{
	// Checking if the window is visible prevents the window from being reduced to windowed when the window is maximized and already showen.
	if (IsWindowVisible(hwnd) == false) {
		ShowWindow(hwnd, SW_RESTORE);
		SetForegroundWindow(hwnd);
	}
}

void TrayManager::UpdateIcon(uint64_t id)
{
	Logger::Info(MODULE_NAME "UpdateIcon() Use bitmap with id(%d)", id);

	HICON waIcon = Helper::GetWindowIcon(GetWhatsAppHwnd());
	auto trayIcon = waIcon;

	if (id > 0) {
		std::string appDirectory = Helper::GetApplicationDirectory();

		// Delete old unread_messsages-bitmap
		auto lastMessageCountBitmapPath = appDirectory + std::string("unread_messages_") + std::to_string(id - 1) + ".bmp";
		if (std::filesystem::exists(lastMessageCountBitmapPath)) {
			Logger::Info(MODULE_NAME "UpdateIcon() Deleting old unread_messages-bitmap '%s'", lastMessageCountBitmapPath.c_str());
			std::filesystem::remove(lastMessageCountBitmapPath);
		}

		auto messageCountBitmapPath = appDirectory + std::string("unread_messages_") + std::to_string(id) + ".bmp";
		if (std::filesystem::exists(messageCountBitmapPath) == false) {
			Logger::Info(MODULE_NAME "UpdateIcon() Could not find message-count-bitmap in '%s'", messageCountBitmapPath.c_str());
		} else {
			trayIcon = AddImageOverlayToIcon(waIcon, messageCountBitmapPath.c_str());
		}
	}

	auto index = GetIndexFromWindowHandle(GetWhatsAppHwnd());
	if (index == -1) {
		return;
	}

	auto nid = CreateTrayIconData(index, trayIcon);

	Shell_NotifyIcon(NIM_MODIFY, &nid);

	if (id > 0) {
		// Only destroy the icon when we created it (Through AddImageOverlayToIcon)
		::DestroyIcon(trayIcon);
	}
}

NOTIFYICONDATA TrayManager::CreateTrayIconData(const int32_t index, HICON trayIcon)
{
	Logger::Info(MODULE_NAME "CreateTrayIconData() With index=%d", index);

	NOTIFYICONDATA nid{ 0 };
	nid.cbSize = NOTIFYICONDATA_V2_SIZE;
	nid.hWnd = _hwndWhatsappTray;
	nid.uID = static_cast<UINT>(index);
	nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	nid.uCallbackMessage = WM_TRAYCMD;
	nid.hIcon = trayIcon;
	nid.uVersion = NOTIFYICON_VERSION;
	strncpy_s(nid.szTip, ARRAYSIZE(nid.szTip), "WhatsApp", _TRUNCATE);

	return nid;
}

int32_t TrayManager::GetIndexFromWindowHandle(const HWND hwnd)
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

HICON TrayManager::AddImageOverlayToIcon(HICON hBackgroundIcon, LPCSTR text)
{
	// Load up background icon
	ICONINFO ii = { 0 };
	//GetIconInfo creates bitmaps for the hbmMask and hbmColor members of ICONINFO.
	//WARNING: The calling application must manage these bitmaps and delete them when they are no longer necessary.!!!!
	::GetIconInfo(hBackgroundIcon, &ii);

	HDC hDc = ::GetDC(NULL);
	HDC hMemDC = ::CreateCompatibleDC(hDc);

	HGDIOBJ hOldBmp = ::SelectObject(hMemDC, ii.hbmColor);

	auto bitmap = Bitmap::FromFile(Helper::Utf8ToWide(text).c_str());

	Graphics graphics(hMemDC);

	// Set black as transparent (don't copy black pixels with DrawImage)
	Gdiplus::ImageAttributes attr;
	attr.SetColorKey(Gdiplus::Color::Black, Gdiplus::Color::Black, Gdiplus::ColorAdjustTypeBitmap);

	// Set x/y-postion and size where the image should be copied to.
	const Gdiplus::Rect rect(10, 10, 20, 20);
	graphics.DrawImage(bitmap, rect, 0, 0, bitmap->GetWidth(), bitmap->GetHeight(), Gdiplus::Unit::UnitPixel, &attr);

	delete bitmap;

	::SelectObject(hMemDC, hOldBmp);

	// Use new icon bitmap with text and new mask bitmap with text
	ICONINFO ii2 = { 0 };
	ii2.fIcon = TRUE;
	ii2.hbmMask = ii.hbmMask;
	ii2.hbmColor = ii.hbmColor;

	// Create updated icon
	HICON iconWithText = ::CreateIconIndirect(&ii2);

	// Delete background icon bitmap info
	::DeleteObject(ii.hbmColor);
	::DeleteObject(ii.hbmMask);

	::DeleteDC(hMemDC);
	::ReleaseDC(NULL, hDc);

	return iconWithText;
}
