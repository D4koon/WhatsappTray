/*
*
* WhatsappTray
* Copyright (C) 1998-2017  Sebastian Amann, Nikolay Redko, J.D. Purcell
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

#include "ApplicationData.h"
#include <windows.h>
#include <Shlobj.h>
#include <sstream>

#undef MODULE_NAME
#define MODULE_NAME "[ApplicationData] "

ApplicationData::ApplicationData()
{
}


ApplicationData::~ApplicationData()
{
}

/*
* Set the data in the persistant storage.
*/
bool ApplicationData::SetData(std::wstring key, bool value)
{
	// Write data to the settingsfile.
	std::wstring applicationDataFilePath = GetSettingsFile();
	if (WritePrivateProfileString(L"config", key.c_str(), value ? L"1" : L"0", applicationDataFilePath.c_str()) == NULL)
	{
		// We get here also when the folder does not exist.
		std::wstringstream message;
		message << MODULE_NAME L"Saving application-data failed because the data could not be written in the settings file '" << applicationDataFilePath.c_str() << "'.";
		MessageBox(NULL, message.str().c_str(), L"WhatsappTray", MB_OK | MB_ICONINFORMATION);
		return false;
	}
	return true;
}

bool ApplicationData::GetDataOrSetDefault(std::wstring key, bool defaultValue)
{
	std::wstring applicationDataFilePath = GetSettingsFile();

	wchar_t valueBuffer[200] = { 0 };
	if (GetPrivateProfileString(L"config", key.c_str(), defaultValue ? L"1" : L"0", valueBuffer, sizeof(valueBuffer), applicationDataFilePath.c_str()) == NULL)
	{
		std::wstringstream message;
		message << MODULE_NAME L"Error while trying to read the settings file '" << applicationDataFilePath.c_str() << "'.";
		MessageBox(NULL, message.str().c_str(), L"WhatsappTray", MB_OK | MB_ICONINFORMATION);
		return defaultValue;
	}

	// Convert the value from a c-string to bool. "1" -> true otherwise false.
	bool value = wcsncmp(L"1", valueBuffer, 1) == 0;
	return value;
}

std::wstring ApplicationData::GetSettingsFile()
{
	wchar_t applicationDataDirectoryPath[MAX_PATH] = { 0 };
	if (SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, applicationDataDirectoryPath) != S_OK)
	{
		MessageBox(NULL, MODULE_NAME L"Saving application-data failed because the path could not be received.", L"WhatsappTray", MB_OK | MB_ICONINFORMATION);
		return std::wstring();
	}

	// Create settings-filder if not exists.
	wcscat_s(applicationDataDirectoryPath, MAX_PATH, L"\\WhatsappTray");
	if (CreateDirectory(applicationDataDirectoryPath, NULL) == NULL)
	{
		// We can get here, if the folder already exists -> We dont want an error for that case ...
		if (GetLastError() != ERROR_ALREADY_EXISTS)
		{
			MessageBox(NULL, MODULE_NAME L"Saving application-data failed because the folder could not be created.", L"WhatsappTray", MB_OK | MB_ICONINFORMATION);
			return std::wstring();
		}
	}
	std::wstring applicationDataFilePath(applicationDataDirectoryPath);
	applicationDataFilePath.append(L"\\config.ini");

	return applicationDataFilePath;
}
