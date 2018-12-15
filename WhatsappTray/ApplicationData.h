/*
*
* WhatsappTray
* Copyright (C) 1998-2017  Sebastian Amann
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
//#define BETTER_ENUMS_NO_CONSTEXPR
#include "Enum.h"

#include <string>
#include <map>

BETTER_ENUM(Data, uint8_t, 
	CLOSE_TO_TRAY,
	LAUNCH_ON_WINDOWS_STARTUP
)

class DataSetBool
{
public:
	DataSetBool(const bool defaultValue, const Data info)
		: Value(defaultValue)
		, DefaultValue(defaultValue)
		, Info(info)
	{
	}
	bool Value;
	const bool DefaultValue;
	const Data Info;
};

class ApplicationData
{
public:
	ApplicationData();
	~ApplicationData() {}
	bool SetData(std::string key, bool value);
	bool GetCloseToTray();
	void SetCloseToTray(bool value);
	bool GetLaunchOnWindowsStartup();
	void SetLaunchOnWindowsStartup(bool value);
	bool GetDataOrSetDefault(std::string key, bool defaultValue);
private:
	std::string GetSettingsFile();
	
	/// If true, the close-button of WhatsApp sends it to tray instead of closing.
	DataSetBool closeToTray;
	DataSetBool launchOnWindowsStartup;
};

