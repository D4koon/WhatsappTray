#pragma once
#include <string>

class ApplicationData
{
public:
	ApplicationData();
	~ApplicationData();
	bool SetData(std::wstring key, bool value);
	bool GetDataOrSetDefault(std::wstring key, bool defaultValue);
private:
	std::wstring GetSettingsFile();
};

