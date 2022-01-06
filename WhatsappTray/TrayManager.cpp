/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright(C) 1998 - 2018 WhatsappTray Sebastian Amann */

#include "stdafx.h"

#include "TrayManager.h"
#include "Helper.h"
#include "Logger.h"
#include "WhatsappTray.h"
#include "SharedDefines.h"

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
	CreateTrayIcon(newIndex, hwnd);
}

void TrayManager::CreateTrayIcon(const int32_t index, const HWND hwnd)
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

void TrayManager::SetIcon(const HWND hwnd, LPCSTR text)
{
	Logger::Info(MODULE_NAME "SetIcon use bitmap with id(%s)", text);

	int32_t index = GetIndexFromWindowHandle(hwnd);
	if (index == -1) {
		return;
	}

	HICON waIcon = Helper::GetWindowIcon(hwnd);
	auto trayIcon = waIcon;

	if (lstrlen(text) > 0) {
		trayIcon = AddImageOverlayToIcon(waIcon, (std::string("C:\\project\\WhatsappTray\\bin\\Debug\\unread_messages_") + text + ".bmp").c_str());
	}

	NOTIFYICONDATA nid;
	ZeroMemory(&nid, sizeof(nid));
	nid.cbSize = NOTIFYICONDATA_V2_SIZE;
	nid.hWnd = _hwndWhatsappTray;
	nid.uID = static_cast<UINT>(index);
	nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	nid.uCallbackMessage = WM_TRAYCMD;
	nid.hIcon = trayIcon;
	GetWindowText(hwnd, nid.szTip, sizeof(nid.szTip) / sizeof(nid.szTip[0]));
	nid.uVersion = NOTIFYICON_VERSION;
	Shell_NotifyIcon(NIM_MODIFY, &nid);

	if (lstrlen(text) > 0) {
		// Only destroy the icon when we created it (Through AddImageOverlayToIcon)
		::DestroyIcon(trayIcon);
	}
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

/**
 * Move into Helper-class?
 * NOTE:
 * I first tried this by only using gdi but since gdi has very limited support for transparency it would be very compliacted to do that.
 * When doing a TextOutA the alpha value gets overridden and the text is then transparent.
 * The solution is probably descibed here https://theartofdev.com/2013/10/24/transparent-text-rendering-with-gdi/ but is complicated.
 * I dont want to spent more time on it so im simply switch to gdi+ which can handle transparency
 * https://stackoverflow.com/questions/43393857/ <- has the problem described above.
 * Warning:
 * GDI+ has to be initialize to work. See WhatsappTray.cpp
 * There might be some GDI-memory leaks did not check to much.
*/
HICON TrayManager::AddTextToIcon(HICON hBackgroundIcon, LPCSTR text)
{
	// Load up background icon
	ICONINFO ii = { 0 };
	//GetIconInfo creates bitmaps for the hbmMask and hbmColor members of ICONINFO.
	//WARNING: The calling application must manage these bitmaps and delete them when they are no longer necessary.!!!!
	::GetIconInfo(hBackgroundIcon, &ii);

	//BITMAP bm;
	//GetObject(ii.hbmColor, sizeof(BITMAP), &bm);

	HDC hDc = ::GetDC(NULL);
	HDC hMemDC = ::CreateCompatibleDC(hDc);

	HGDIOBJ hOldBmp = ::SelectObject(hMemDC, ii.hbmColor);
	//for (size_t i = 0; i < 32; i++) {
	//	for (size_t y = 0; y < 32; y++) {
	//		COLORREF pixelColor = GetPixel(hMemDC, i, y);
	//		Logger::Info(MODULE_NAME "Pixelcolor: %X", pixelColor);
	//	}
	//}

	Graphics graphics(hMemDC);
	SolidBrush brush(Color(255, 255, 0, 0));
	FontFamily fontFamily(L"Arial Black");
	Font font(&fontFamily, 24, FontStyleBold, UnitPixel);
	PointF pointF(3.0f, -1.5f);
	if (strlen(text) >= 2) {
		pointF = PointF(-6.0f, -1.5f);
	}

	graphics.DrawString(Helper::Utf8ToWide(text).c_str(), -1, &font, pointF, &brush);

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
