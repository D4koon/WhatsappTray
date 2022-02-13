/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright(C) 2020 - 2022 WhatsappTray Sebastian Amann */

#include "WinSockLogger.h"

#include <windows.h>
#include <psapi.h> // GetModuleFileNameExW()

HWND GetTopLevelWindowhandleWithName(std::string searchedWindowTitle, DWORD _processID);
std::string GetWindowTitle(HWND hwnd);
POINT LParamToPoint(LPARAM lParam);
std::string GetFilepathFromProcessID(DWORD processId);
std::string GetEnviromentVariable(const std::string& inputString);
std::string WideToUtf8(const std::wstring& inputString);
bool SaveHBITMAPToFile(HBITMAP hBitmap, LPCTSTR fileName);
bool SaveHIconToFile(HICON hIcon, std::string fileName);
