#include "ApplicationData.h"
#include <windows.h>
#include <Shlobj.h>
#include <sstream>

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
		message << L"[ApplicationData] Saving application-data failed because the data could not be written in the settings file '" << applicationDataFilePath.c_str() << "'.";
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
		message << L"[ApplicationData] Error while trying to read the settings file '" << applicationDataFilePath.c_str() << "'.";
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
		MessageBox(NULL, L"[ApplicationData] Saving application-data failed because the path could not be received.", L"WhatsappTray", MB_OK | MB_ICONINFORMATION);
		return std::wstring();
	}

	// Create settings-filder if not exists.
	wcscat_s(applicationDataDirectoryPath, sizeof(applicationDataDirectoryPath), L"\\WhatsappTray");
	if (CreateDirectory(applicationDataDirectoryPath, NULL) == NULL)
	{
		// We can get here, if the folder already exists -> We dont want an error for that case ...
		if (GetLastError() != ERROR_ALREADY_EXISTS)
		{
			MessageBox(NULL, L"[ApplicationData] Saving application-data failed because the folder could not be created.", L"WhatsappTray", MB_OK | MB_ICONINFORMATION);
			return std::wstring();
		}
	}
	std::wstring applicationDataFilePath(applicationDataDirectoryPath);
	applicationDataFilePath.append(L"\\config.ini");

	return applicationDataFilePath;
}
