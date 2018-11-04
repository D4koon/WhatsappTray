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

#include "Registry.h"
#include "Helper.h"

#include <tchar.h>
#include <exception>

const LPTSTR Registry::applicatinName = TEXT("WhatsappTray");

/*
* @brief Creates an entry in the registry to run WhatsappTray on startup.
*/
void Registry::RegisterProgram()
{
	// Get the path to WhatsappTray.
	TCHAR szPathToExe[MAX_PATH];
	GetModuleFileName(NULL, szPathToExe, MAX_PATH);

	// Set the autostart in registry.
	RegisterMyProgramForStartup(applicatinName, szPathToExe, TEXT(""));
}

/*
* @brief Creates an entry in the registry to run \p pszAppName on startup.
*/
bool Registry::RegisterMyProgramForStartup(LPTSTR pszAppName, LPTSTR pathToExe, LPTSTR args)
{
	HKEY hKey = NULL;
	LONG lResult = 0;
	bool fSuccess = TRUE;
	DWORD dwSize;

	const size_t count = MAX_PATH * 2;
	TCHAR szValue[count] = { 0 };

	_tcscpy_s(szValue, count, TEXT("\""));
	_tcscat_s(szValue, count, pathToExe);
	_tcscat_s(szValue, count, TEXT("\" "));

	if (args != NULL) {
		// caller should make sure "args" is quoted if any single argument has a space
		// e.g. (L"-name \"Mark Voidale\"");
		_tcscat_s(szValue, count, args);
	}

	lResult = RegCreateKeyEx(HKEY_CURRENT_USER, TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), 0, NULL, 0, (KEY_WRITE | KEY_READ), NULL, &hKey, NULL);

	fSuccess = (lResult == 0);

	if (fSuccess) {
		if ((_tcslen(szValue) + 1) * 2 > ULONG_MAX) {
			throw std::exception("Registry::RegisterMyProgramForStartup() - String is too long.");
		}
		dwSize = static_cast<DWORD>((_tcslen(szValue) + 1) * 2);
		lResult = RegSetValueEx(hKey, pszAppName, 0, REG_SZ, (BYTE*)szValue, dwSize);
		fSuccess = (lResult == 0);
	}

	if (hKey != NULL) {
		RegCloseKey(hKey);
		hKey = NULL;
	}

	return fSuccess;
}

/*
* @brief Returns true if the autorun entry for WhatsappTray exists in the registry.
*/
bool Registry::IsMyProgramRegisteredForStartup(LPTSTR pszAppName)
{
	HKEY hKey = NULL;
	LONG lResult = 0;
	bool fSuccess = TRUE;
	DWORD dwRegType = REG_SZ;
	wchar_t szPathToExe[MAX_PATH] = { 0 };
	DWORD dwSize = sizeof(szPathToExe);

	lResult = RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), 0, KEY_READ, &hKey);

	fSuccess = (lResult == 0);

	if (fSuccess) {
		lResult = RegGetValue(hKey, NULL, pszAppName, RRF_RT_REG_SZ, &dwRegType, szPathToExe, &dwSize);
		fSuccess = (lResult == 0);
	}

	if (fSuccess) {
		fSuccess = (wcslen(szPathToExe) > 0) ? TRUE : FALSE;
	}

	if (hKey != NULL) {
		RegCloseKey(hKey);
		hKey = NULL;
	}

	return fSuccess;
}

/*
* @brief Deletes the entry in the registry to run WhatsappTray on startup.
*/
void Registry::UnregisterProgram()
{
	RegDeleteKeyValue(HKEY_CURRENT_USER, TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), applicatinName);
}
