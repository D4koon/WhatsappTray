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
	static std::wstring Helper::ToWString(const std::string& inputString);
	static std::string Helper::ToString(const std::wstring& inputString);
	static std::wstring Utf8ToWide(const std::string& inputString);
	static std::string WideToUtf8(const std::wstring& inputString);
	static bool Replace(std::string& str, const std::string& oldValue, const std::string& newValue);
	static HICON GetWindowIcon(HWND hwnd);
	static std::string GetProductAndVersion();
	static std::string GetStartMenuProgramsDirectory();
	static std::string GetWindowsAppDataDirectory();
	static std::string GetCurrentUserDirectory();
	static std::string GetCurrentUserAppData();
	static std::string GetFilenameFromPath(std::string path);
	static std::wstring GetFilenameFromPath(std::wstring path);
	static std::string ResolveLnk(HWND hwnd, LPCSTR lpszLinkFile);
	static std::string GetFilepathFromProcessID(DWORD processId);
	static std::string GetWindowTitle(const HWND hwnd);
};

