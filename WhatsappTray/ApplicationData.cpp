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
#include <tchar.h>

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
bool ApplicationData::SetData(std::string key, bool value)
{
	// Write data to the settingsfile.
	std::string applicationDataFilePath = GetSettingsFile();
	if (WritePrivateProfileString("config", key.c_str(), value ? "1" : "0", applicationDataFilePath.c_str()) == NULL)
	{
		// We get here also when the folder does not exist.
		std::stringstream message;
		message << MODULE_NAME L"Saving application-data failed because the data could not be written in the settings file '" << applicationDataFilePath.c_str() << "'.";
		MessageBox(NULL, message.str().c_str(), "WhatsappTray", MB_OK | MB_ICONINFORMATION);
		return false;
	}
	return true;
}

bool ApplicationData::GetDataOrSetDefault(std::string key, bool defaultValue)
{
	std::string applicationDataFilePath = GetSettingsFile();

	TCHAR valueBuffer[200] = { 0 };
	if (GetPrivateProfileString("config", key.c_str(), defaultValue ? "1" : "0", valueBuffer, sizeof(valueBuffer), applicationDataFilePath.c_str()) == NULL)
	{
		std::stringstream message;
		message << MODULE_NAME "Error while trying to read the settings file '" << applicationDataFilePath.c_str() << "'.";
		MessageBox(NULL, message.str().c_str(), "WhatsappTray", MB_OK | MB_ICONINFORMATION);
		return defaultValue;
	}

	// Convert the value from a c-string to bool. "1" -> true otherwise false.
	bool value = _tcsncmp(TEXT("1"), valueBuffer, 1) == 0;
	return value;
}

std::string ApplicationData::GetSettingsFile()
{
	TCHAR applicationDataDirectoryPath[MAX_PATH] = { 0 };
	if (SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, applicationDataDirectoryPath) != S_OK)
	{
		MessageBox(NULL, MODULE_NAME "Saving application-data failed because the path could not be received.", "WhatsappTray", MB_OK | MB_ICONINFORMATION);
		return "";
	}

	// Create settings-filder if not exists.
	_tcscat_s(applicationDataDirectoryPath, MAX_PATH, TEXT("\\WhatsappTray"));
	if (CreateDirectory(applicationDataDirectoryPath, NULL) == NULL)
	{
		// We can get here, if the folder already exists -> We dont want an error for that case ...
		if (GetLastError() != ERROR_ALREADY_EXISTS)
		{
			MessageBox(NULL, MODULE_NAME "Saving application-data failed because the folder could not be created.", "WhatsappTray", MB_OK | MB_ICONINFORMATION);
			return "";
		}
	}
	std::string applicationDataFilePath(applicationDataDirectoryPath);
	applicationDataFilePath.append("\\config.ini");

	return applicationDataFilePath;
}
