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

#include "AppData.h"
#include "Logger.h"
#include "Helper.h"

#include <Shlobj.h>
#include <sstream>
#include <tchar.h>
#include <string>

#undef MODULE_NAME
#define MODULE_NAME "AppData::"

/**
 * The data-storage for the application.
 */
AppData::AppData()
	: closeToTray(false, Data::CLOSE_TO_TRAY)
	, launchOnWindowsStartup(false, Data::LAUNCH_ON_WINDOWS_STARTUP)
{
	closeToTray.Value = GetDataOrSetDefault(closeToTray.Info.toString(), closeToTray.DefaultValue);
	launchOnWindowsStartup.Value = GetDataOrSetDefault(launchOnWindowsStartup.Info.toString(), launchOnWindowsStartup.DefaultValue);
}

/*
* Set the data in the persistant storage.
*/
bool AppData::SetData(std::string key, bool value)
{
	// Write data to the AppData-file.
	std::string appDataFilePath = GetAppDataFilePath();
	if (WritePrivateProfileString("config", key.c_str(), value ? "1" : "0", appDataFilePath.c_str()) == NULL) {
		// We get here also when the folder does not exist.
		std::stringstream message;
		message << MODULE_NAME "SetData() - Saving app-data failed because the data could not be written in the AppData-file '" << appDataFilePath.c_str() << "'.";
		Logger::Error(message.str().c_str());
		MessageBox(NULL, message.str().c_str(), "WhatsappTray", MB_OK | MB_ICONINFORMATION);
		return false;
	}
	return true;
}

bool AppData::GetCloseToTray()
{
	return closeToTray.Value;
}

void AppData::SetCloseToTray(bool value)
{
	closeToTray.Value = value;
	// Write data to persistant storage.
	SetData("CLOSE_TO_TRAY", value);
}

bool AppData::GetLaunchOnWindowsStartup()
{
	return launchOnWindowsStartup.Value;
}

void AppData::SetLaunchOnWindowsStartup(bool value)
{
	launchOnWindowsStartup.Value = value;
	SetData("LAUNCH_ON_WINDOWS_STARTUP", value);
}

bool AppData::GetDataOrSetDefault(std::string key, bool defaultValue)
{
	std::string appDataFilePath = GetAppDataFilePath();

	TCHAR valueBuffer[200] = { 0 };
	if (GetPrivateProfileString("config", key.c_str(), defaultValue ? "1" : "0", valueBuffer, sizeof(valueBuffer), appDataFilePath.c_str()) == NULL) {
		std::stringstream message;
		message << MODULE_NAME "GetDataOrSetDefault() - Error while trying to read the AppData-file '" << appDataFilePath.c_str() << "'.";
		Logger::Error(message.str().c_str());
		MessageBox(NULL, message.str().c_str(), "WhatsappTray", MB_OK | MB_ICONINFORMATION);
		return defaultValue;
	}

	// Convert the value from a c-string to bool. "1" -> true otherwise false.
	bool value = _tcsncmp(TEXT("1"), valueBuffer, 1) == 0;
	return value;
}

/**
 * Get path to the data-file.
 */
std::string AppData::GetAppDataFilePath()
{
	//char appDataDirectory[MAX_PATH] = { 0 };
	//if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, appDataDirectory) != S_OK) {
	//	Logger::Error(MODULE_NAME "GetAppDataFilePath() - Saving app-data failed because the path could not be received.");
	//	MessageBoxA(NULL, MODULE_NAME "GetAppDataFilePath() - Saving app-data failed because the path could not be received.", "WhatsappTray", MB_OK | MB_ICONINFORMATION);
	//	return "";
	//}

	//// Create data-folder if not exists.
	//strcat_s(appDataDirectory, MAX_PATH, TEXT("\\WhatsappTray\\"));
	//if (CreateDirectory(appDataDirectory, NULL) == NULL) {
	//	// We can get here, if the folder already exists -> We don't want an error for that case ...
	//	if (GetLastError() != ERROR_ALREADY_EXISTS) {
	//		Logger::Error(MODULE_NAME "GetAppDataFilePath() - Saving app-data failed because the folder could not be created.");
	//		MessageBoxA(NULL, MODULE_NAME "GetAppDataFilePath() - Saving app-data failed because the folder could not be created.", "WhatsappTray", MB_OK | MB_ICONINFORMATION);
	//		return "";
	//	}
	//}
	std::string appDirectory = Helper::GetApplicationDirectory();

	std::string appDataFilePath(appDirectory);
	appDataFilePath.append("appData.ini");

	return appDataFilePath;
}
