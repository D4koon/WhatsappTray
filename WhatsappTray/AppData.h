/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright(C) 2019 - 2020 WhatsappTray Sebastian Amann */

#pragma once
#include "Enum.h"

#include <string>
#include <map>
#include <thread>

BETTER_ENUM(Data, uint8_t, 
	CLOSE_TO_TRAY,
	LAUNCH_ON_WINDOWS_STARTUP,
	START_MINIMIZED,
	SHOW_UNREAD_MESSAGES,
	CLOSE_TO_TRAY_WITH_ESCAPE,
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
	void SetAsString(std::string valueAsString)
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
	void SetAsString(std::string valueAsString)
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

	// If true, the close-button of WhatsApp sends it to tray instead of closing.
	static DataEntryS<SBool> CloseToTray;
	static DataEntryS<SBool> LaunchOnWindowsStartup;
	static DataEntryS<SBool> StartMinimized;
	static DataEntryS<SBool> ShowUnreadMessages;
	static DataEntryS<SBool> CloseToTrayWithEscape;

	static std::string WhatsappStartpathGet();
	static std::string WhatsappRoamingDirectoryGet();
private:
	AppData() {}

	// NOTE: UTF8-string.
	static DataEntryS<SString> WhatsappStartpath;
	// NOTE: UTF8-string.
	static DataEntryS<SString> WhatsappRoamingDirectory;

	static std::string GetAppDataFilePath();
	
	/// Dummy-value used for initialization. Like a static constructor.
	static bool initDone;
};
