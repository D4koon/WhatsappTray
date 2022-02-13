/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright(C) 1998 - 2018 WhatsappTray Sebastian Amann */

#pragma once

// Include lib for GetFileVersionInfoSize()
#pragma comment(lib,"Version.lib")

#include <windows.h>
#include <string>

/**
 * @brief
 *
 * https://stackoverflow.com/a/26221725/4870255
 */
template<typename ... Args>
std::string string_format(const std::string& format, Args ... args)
{
	size_t size = snprintf(nullptr, 0, format.c_str(), args ...) + 1ll; // '1 + ' because extra space for '\0'
	if (size <= 0) { throw std::runtime_error("Error during formatting."); }
	std::unique_ptr<char[]> buf(new char[size]);
	snprintf(buf.get(), size, format.c_str(), args ...);
	return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
}


enum class WindowsOS {
	NotFind,
	Win2000,
	WinXP,
	WinVista,
	Win7,
	Win8,
	Win10
};

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
	static PROCESS_INFORMATION Helper::StartProcess(std::string exePath);
	static WindowsOS GetOsVersionQuick();
};
