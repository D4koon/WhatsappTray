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
#include "Enum.h"

#include <string>
#include <map>
#include <thread>

BETTER_ENUM(Data, uint8_t, 
	CLOSE_TO_TRAY,
	LAUNCH_ON_WINDOWS_STARTUP,
	START_MINIMIZED,
	WHATSAPP_STARTPATH,
	WHATSAPP_ROAMING_DIRECTORY
)

class Serializeable
{
public:
	Serializeable()
	{ }
	virtual std::string ToString() = 0;
	std::function<void()> valueChanged;
};

class SBool : public Serializeable
{
	bool Value;
public:
	SBool() : Value(false) { }
	SBool(bool value) : Value(value) { }
	void operator=(const SBool& newValue)
	{
		Value = newValue;
		valueChanged();
	}
	operator bool() const { return Value; }
	std::string ToString() {
		return this->Value ? "1" : "0";
	}
	void FromString(std::string valueAsString)
	{
		Value = valueAsString.compare("1") == 0;
		valueChanged();
	}
};

class SString : public Serializeable
{
	std::string Value;
public:
	SString() : Value("") { }
	SString(std::string value) : Value(value) { }
	void operator=(const SString& newValue)
	{
		Value = newValue;
		valueChanged();
	}
	operator std::string() const { return Value; }
	std::string ToString() {
		return this->Value;
	}
	void FromString(std::string valueAsString)
	{
		Value = valueAsString;
		valueChanged();
	}
};

class DataEntry
{
public:
	DataEntry(const Data info)
		: Info(info)
	{ }
	const Data Info;
	Serializeable* Value;
	Serializeable* DefaultValue;
};

template<typename T>
class DataEntryS : public DataEntry
{
public:
	DataEntryS(const Data info, T defaultValue, const std::function<bool(DataEntry&)>& valueChangedHandler) : DataEntry(info)
	{
		//this->Value = new T(defaultValue);
		this->Value = new T();
		*(this->Value) = defaultValue;
		this->Value->valueChanged = std::bind(&DataEntryS::OnValueChanged, this);
		this->valueChangedHandler = valueChangedHandler;
		this->DefaultValue = new T(defaultValue);
	}
	~DataEntryS()
	{
		delete this->Value;
		delete this->DefaultValue;
	}
	T& Get()
	{
		return *(reinterpret_cast<T*>(this->Value));
	}
	void Set(const T& value)
	{
		*(reinterpret_cast<T*>(this->Value)) = value;
	}
	std::function<bool(DataEntry&)> valueChangedHandler;
	void OnValueChanged()
	{
		if (valueChangedHandler) {
			valueChangedHandler(*this);
		}
	}
};

class AppData
{
public:
	static bool SetData(DataEntry& value);
	static std::string GetDataOrSetDefault(DataEntry& value);

	/// If true, the close-button of WhatsApp sends it to tray instead of closing.
	static DataEntryS<SBool> CloseToTray;
	static DataEntryS<SBool> LaunchOnWindowsStartup;
	static DataEntryS<SBool> StartMinimized;
	static DataEntryS<SString> WhatsappStartpath;
	static DataEntryS<SString> WhatsappRoamingDirectory;
private:
	AppData() {}

	static std::string GetAppDataFilePath();
	
	/// Dummy-value used for initialization. Like a static constructor.
	static bool initDone;
};
