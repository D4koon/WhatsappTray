/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright(C) 1998 - 2018 WhatsappTray Sebastian Amann */

#pragma once

#include <Windows.h>

class Registry
{
public:
	static void RegisterProgram();
	static bool RegisterMyProgramForStartup(const TCHAR* pszAppName, TCHAR* pathToExe, TCHAR* args);
	static bool IsMyProgramRegisteredForStartup(TCHAR* pszAppName);
	static void UnregisterProgram();
private:
	Registry() { }
	~Registry() { }
	static const TCHAR* applicatinName;
};

