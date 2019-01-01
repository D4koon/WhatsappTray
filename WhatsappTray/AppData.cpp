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
#include <string>

#undef MODULE_NAME
#define MODULE_NAME "AppData::"

/**
 * The data-storage for the application.
 */
AppData::AppData()
	: CloseToTray(Data::CLOSE_TO_TRAY, false, &AppData::SetData)
	, LaunchOnWindowsStartup(Data::LAUNCH_ON_WINDOWS_STARTUP, false, &AppData::SetData)
	, StartMinimized(Data::START_MINIMIZED, false, &AppData::SetData)
	, WhatsappStartpath(Data::WHATSAPP_STARTPATH, Helper::GetStartMenuProgramsDirectory() + "\\WhatsApp\\WhatsApp.lnk", &AppData::SetData)
{
	CloseToTray.Get().FromString(GetDataOrSetDefault(CloseToTray));
	LaunchOnWindowsStartup.Get().FromString(GetDataOrSetDefault(LaunchOnWindowsStartup));
	StartMinimized.Get().FromString(GetDataOrSetDefault(StartMinimized));
	WhatsappStartpath.Get().FromString(GetDataOrSetDefault(WhatsappStartpath));
}

/*
* Set the data in the persistant storage.
* Write data to the AppData-file.
*/
bool AppData::SetData(DataEntry& value)
{
	std::string appDataFilePath = GetAppDataFilePath();
	if (WritePrivateProfileStringA("config", value.Info.toString(), value.Value->ToString().c_str(), appDataFilePath.c_str()) == NULL) {
		// We get here also when the folder does not exist.
		std::stringstream message;
		message << MODULE_NAME "SetData() - Saving app-data failed because the data could not be written in the AppData-file '" << appDataFilePath.c_str() << "'.";
		Logger::Error(message.str().c_str());
		MessageBox(NULL, message.str().c_str(), "WhatsappTray", MB_OK | MB_ICONINFORMATION);
		return false;
	}
	return true;
}

/**
 * Returns the default value if no value is found.
 */
std::string AppData::GetDataOrSetDefault(DataEntry& value)
{
	std::string appDataFilePath = GetAppDataFilePath();

	char valueBuffer[200] = { 0 };
	DWORD copiedCharacters = GetPrivateProfileStringA("config", value.Info.toString(), value.DefaultValue->ToString().c_str(), valueBuffer, sizeof(valueBuffer), appDataFilePath.c_str());
	// If the default value is "" i dont want to trigger an error if the value cannot be read. So i added "value.DefaultValue->ToString().length() > 0"
	if (copiedCharacters == 0 && value.DefaultValue->ToString().length() > 0) {
		// If we end up here that means the config was found but was empty.
		// If the config would have not existed the default value would have been used.
		std::stringstream message;
		message << MODULE_NAME "GetDataOrSetDefault() - Could not find value '" << value.Info.toString() << "' in AppData-file '" << appDataFilePath.c_str() << "'.\n Setting the value to default='" << value.DefaultValue->ToString().c_str() << "'.";
		Logger::Error(message.str().c_str());
		MessageBox(NULL, message.str().c_str(), "WhatsappTray", MB_OK | MB_ICONINFORMATION);
		return value.DefaultValue->ToString();
	}

	return valueBuffer;
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
