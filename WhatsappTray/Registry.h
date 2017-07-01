#pragma once

#include <Windows.h>

class Registry
{
public:
	Registry();
	~Registry();
	void RegisterProgram();
	bool RegisterMyProgramForStartup(PCWSTR pszAppName, PCWSTR pathToExe, PCWSTR args);
	bool IsMyProgramRegisteredForStartup(PCWSTR pszAppName);
};

