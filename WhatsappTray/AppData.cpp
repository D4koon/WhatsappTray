/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright(C) 2018 - 2018 WhatsappTray Sebastian Amann */

#include "stdafx.h"

#include "AppData.h"
#include "Logger.h"
#include "Helper.h"

//#include <Shlobj.h>
#include <sstream>
#include <string>

#undef MODULE_NAME
#define MODULE_NAME "AppData::"

/**
 * Initialize the data-storage for the application.
 */
DataEntryS<SBool> AppData::CloseToTray(Data::CLOSE_TO_TRAY, false, &AppData::SetData);
DataEntryS<SBool> AppData::LaunchOnWindowsStartup(Data::LAUNCH_ON_WINDOWS_STARTUP, false, &AppData::SetData);
DataEntryS<SBool> AppData::StartMinimized(Data::START_MINIMIZED, false, &AppData::SetData);
DataEntryS<SBool> AppData::ShowUnreadMessages(Data::SHOW_UNREAD_MESSAGES, false, &AppData::SetData);
DataEntryS<SBool> AppData::CloseToTrayWithEscape(Data::CLOSE_TO_TRAY_WITH_ESCAPE, false, &AppData::SetData);
DataEntryS<SString> AppData::WhatsappStartpath(Data::WHATSAPP_STARTPATH, std::string("%userStartmenuePrograms%\\WhatsApp\\WhatsApp.lnk"), &AppData::SetData);
DataEntryS<SString> AppData::WhatsappRoamingDirectory(Data::WHATSAPP_ROAMING_DIRECTORY, Helper::GetWindowsAppDataRoamingDirectory(), &AppData::SetData);

/// Initialize the dummy-value initDone with a lambda to get a static-constructor like behavior. NOTE: The disadvantage is though that we can not control the order. For example if we want to make sure that the logger inits first.
bool AppData::initDone([]() 
{
	// Currently the DataEntries hold the default value.
	// Now we look vor values in the config-file and write those if they exist.
	CloseToTray.Get().SetAsString(GetDataOrSetDefault(CloseToTray));
	LaunchOnWindowsStartup.Get().SetAsString(GetDataOrSetDefault(LaunchOnWindowsStartup));
	StartMinimized.Get().SetAsString(GetDataOrSetDefault(StartMinimized));
	ShowUnreadMessages.Get().SetAsString(GetDataOrSetDefault(ShowUnreadMessages));
	CloseToTrayWithEscape.Get().SetAsString(GetDataOrSetDefault(CloseToTrayWithEscape));
	WhatsappStartpath.Get().SetAsString(GetDataOrSetDefault(WhatsappStartpath));
	WhatsappRoamingDirectory.Get().SetAsString(GetDataOrSetDefault(WhatsappRoamingDirectory));

	return true; 
}());

/**
 * @brief Set the data in the persistant storage.
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
 * @brief Returns the default value if no value is found.
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
 * @brief Get path to the data-file.
 */
std::string AppData::GetAppDataFilePath()
{
	std::string appDirectory = Helper::GetApplicationDirectory();

	std::string appDataFilePath(appDirectory);
	appDataFilePath.append("appData.ini");

	return appDataFilePath;
}

/**
 * @brief Gets the path to the shortcut(*.lnk) or *.exe file of WhatsApp
 */
std::string AppData::WhatsappStartpathGet()
{
	auto path = AppData::WhatsappStartpath.Get().ToString();

	Helper::Replace(path, "%userStartmenuePrograms%", Helper::GetStartMenuProgramsDirectory());
	Helper::Replace(path, "%UserProfile%", Helper::GetCurrentUserDirectory());
	Helper::Replace(path, "%userprofile%", Helper::GetCurrentUserDirectory());
	Helper::Replace(path, "%AppData%", Helper::GetWindowsAppDataRoamingDirectory());
	Helper::Replace(path, "%appdata%", Helper::GetWindowsAppDataRoamingDirectory());

	return path;
}

/**
 * @brief Gets the path to C:\Users\<JohnDoe>\AppData\Roaming
 */
std::string AppData::WhatsappRoamingDirectoryGet()
{
	auto path = AppData::WhatsappRoamingDirectory.Get().ToString();

	Helper::Replace(path, "%UserProfile%", Helper::GetCurrentUserDirectory());
	Helper::Replace(path, "%userprofile%", Helper::GetCurrentUserDirectory());
	Helper::Replace(path, "%AppData%", Helper::GetWindowsAppDataRoamingDirectory());
	Helper::Replace(path, "%appdata%", Helper::GetWindowsAppDataRoamingDirectory());

	return path;
}
