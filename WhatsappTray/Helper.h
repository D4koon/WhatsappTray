/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright(C) 1998 - 2018 WhatsappTray Sebastian Amann */

#pragma once

// Include lib for GetFileVersionInfoSize()
#pragma comment(lib,"Version.lib")

#include <windows.h>
#include <string>

class Helper
{
public:
	Helper() { }
	~Helper() { }

	static std::string Helper::GetApplicationFilePath();
	static std::string GetApplicationDirectory();
	static std::wstring Utf8ToWide(const std::string& inputString);
	static std::string WideToUtf8(const std::wstring& inputString);
	static bool Replace(std::string& str, const std::string& oldValue, const std::string& newValue);
	static HICON GetWindowIcon(HWND hwnd);
	static std::string GetProductAndVersion();
	static std::string GetStartMenuProgramsDirectory();
	static std::string GetWindowsAppDataRoamingDirectory();
	static std::string GetCurrentUserDirectory();
	static std::string GetFilenameFromPath(std::string path);
	static std::wstring GetFilenameFromPath(std::wstring path);
	static std::string ResolveLnk(HWND hwnd, LPCSTR lpszLinkFile);
	static std::string GetFilepathFromProcessID(DWORD processId);
	static std::string GetWindowTitle(const HWND hwnd);
};

