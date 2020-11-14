/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright(C) 1998 - 2018 WhatsappTray Sebastian Amann */

#include "stdafx.h"

#include "Registry.h"
#include "Helper.h"
#include "Logger.h"

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
		// e.g. ("-name \"Mark Voidale\"");
		_tcscat_s(szValue, count, args);
	}

	lResult = RegCreateKeyEx(HKEY_CURRENT_USER, TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), 0, NULL, 0, (KEY_WRITE | KEY_READ), NULL, &hKey, NULL);

	if (lResult != 0) {
		Logger::Error("Registry::RegisterMyProgramForStartup() - Regestry-key could not been created.");
		return false;
	}

	if ((_tcslen(szValue) + 1) * 2 > ULONG_MAX) {
		Logger::Error("Registry::RegisterMyProgramForStartup() - String is too long.");
		throw std::exception("Registry::RegisterMyProgramForStartup() - String is too long.");
	}

	dwSize = static_cast<DWORD>((_tcslen(szValue) + 1) * 2);
	lResult = RegSetValueEx(hKey, pszAppName, 0, REG_SZ, (BYTE*)szValue, dwSize);

	if (hKey != NULL) {
		RegCloseKey(hKey);
	}

	if (lResult != 0) {
		Logger::Error("Registry::RegisterMyProgramForStartup() - Could not set value of regestry-key.");
		return false;
	}

	return (lResult == 0);
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
	TCHAR szPathToExe[MAX_PATH] = { 0 };
	DWORD dwSize = sizeof(szPathToExe);

	lResult = RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), 0, KEY_READ, &hKey);

	fSuccess = (lResult == 0);

	if (fSuccess) {
		lResult = RegGetValue(hKey, NULL, pszAppName, RRF_RT_REG_SZ, &dwRegType, szPathToExe, &dwSize);
		fSuccess = (lResult == 0);
	}

	if (fSuccess) {
		fSuccess = (lstrlen(szPathToExe) > 0) ? TRUE : FALSE;
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
