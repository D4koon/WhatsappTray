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
#include "Helper.h"

#include "Logger.h"

#include <vector>
#include <sstream>

/*
* @brief Get the Path to the exe-file of the application.
*/
std::string Helper::GetApplicationFilePath()
{
	char szPathToExe[MAX_PATH];
	GetModuleFileNameA(NULL, szPathToExe, MAX_PATH);

	return szPathToExe;
}

/*
* @brief Get the Path to the directory where the exe-file of the application is.
*/
std::string Helper::GetApplicationDirectory()
{
	char szPathToExe[MAX_PATH];
	GetModuleFileNameA(NULL, szPathToExe, MAX_PATH);

	std::string applicationFilePath = GetApplicationFilePath();
	std::size_t found = applicationFilePath.find_last_of('\\');
	if (found == std::string::npos) {
		OutputDebugString(TEXT("ERROR: GetApplicationDirectory() - Could not find \\ in the application-path.\n"));
		return "";
	}

	return applicationFilePath.substr(0, found) + "\\";
}

/**
* Untested!
*/
std::wstring Helper::ToWString(const std::string& inputString)
{
	const char* inputC_String = inputString.c_str();
	size_t size = inputString.length() + 1;
	wchar_t* outputStringBuffer = new wchar_t[size];

	size_t outSize;
	errno_t convertResult = mbstowcs_s(&outSize, outputStringBuffer, size, inputC_String, size - 1);

	if (convertResult != NULL) {
		Logger::Error("Error in mbsrtowcs(): %d", errno);
		return L"";
	}

	std::wstring outputString = outputStringBuffer;
	delete[] outputStringBuffer;

	return outputString;
}

/**
* Convert a std::wstring to a std::string.
*/
std::string Helper::ToString(const std::wstring& inputString)
{
	const wchar_t* inputC_String = inputString.c_str();
	size_t size = inputString.length() + 1;
	char* outputStringBuffer = new char[size];

	size_t outSize;
	errno_t convertResult = wcstombs_s(&outSize, outputStringBuffer, size, inputC_String, size - 1);

	if (convertResult != NULL) {
		Logger::Error("Error in mbsrtowcs(): %d", errno);
		return "";
	}

	std::string outputString = outputStringBuffer;
	delete[] outputStringBuffer;

	return outputString;
}

HICON Helper::GetWindowIcon(HWND hwnd)
{
	HICON icon;
	if (icon = (HICON)SendMessage(hwnd, WM_GETICON, ICON_SMALL, 0)) return icon;
	if (icon = (HICON)SendMessage(hwnd, WM_GETICON, ICON_BIG, 0)) return icon;
	if (icon = (HICON)GetClassLongPtr(hwnd, GCLP_HICONSM)) return icon;
	if (icon = (HICON)GetClassLongPtr(hwnd, GCLP_HICON)) return icon;
	return LoadIcon(NULL, IDI_WINLOGO);
}

// From: https://stackoverflow.com/questions/7028304/error-lnk2019-when-using-getfileversioninfosize
std::string Helper::GetProductAndVersion()
{
	// get the filename of the executable containing the version resource
	TCHAR szFilename[MAX_PATH + 1] = { 0 };
	if (GetModuleFileName(NULL, szFilename, MAX_PATH) == 0) {
		Logger::Error("GetModuleFileName failed with error %d", GetLastError());
		return "";
	}

	// allocate a block of memory for the version info
	DWORD dummy;
	DWORD dwSize = GetFileVersionInfoSize(szFilename, &dummy);
	if (dwSize == 0) {
		Logger::Error("GetFileVersionInfoSize failed with error %d", GetLastError());
		return "";
	}
	std::vector<BYTE> data(dwSize);

	// load the version info
	if (!GetFileVersionInfo(szFilename, NULL, dwSize, &data[0])) {
		Logger::Error("GetFileVersionInfo failed with error %d", GetLastError());
		return "";
	}

	// get the version strings
	LPVOID pvProductVersion = NULL;
	unsigned int iProductVersionLen = 0;

	UINT                uiVerLen = 0;
	VS_FIXEDFILEINFO*   pFixedInfo = 0;     // pointer to fixed file info structure
	// get the fixed file info (language-independent) 
	if (VerQueryValue(&data[0], TEXT("\\"), (void**)&pFixedInfo, (UINT *)&uiVerLen) == 0) {
		Logger::Error("Can't obtain ProductVersion from resources");
		return "";
	}

	std::stringstream stringStream;
	stringStream << HIWORD(pFixedInfo->dwProductVersionMS) << "." << LOWORD(pFixedInfo->dwProductVersionMS) << "." << HIWORD(pFixedInfo->dwProductVersionLS) << "." << LOWORD(pFixedInfo->dwProductVersionLS);

	return stringStream.str();
}