/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright(C) 1998 - 2018 WhatsappTray Sebastian Amann */

#pragma once

#include <Windows.h>

class Registry
{
public:
	static void RegisterProgram();
	static bool RegisterMyProgramForStartup(LPTSTR pszAppName, LPTSTR pathToExe, LPTSTR args);
	static bool IsMyProgramRegisteredForStartup(LPTSTR pszAppName);
	static void UnregisterProgram();
private:
	Registry() { }
	~Registry() { }
	static const LPTSTR applicatinName;
};

