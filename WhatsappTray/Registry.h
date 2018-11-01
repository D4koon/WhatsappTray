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

#include <Windows.h>

class Registry
{
public:
	Registry() { }
	~Registry() { }
	void RegisterProgram();
	bool RegisterMyProgramForStartup(PCWSTR pszAppName, PCWSTR pathToExe, PCWSTR args);
	bool IsMyProgramRegisteredForStartup(PCWSTR pszAppName);
	void UnregisterProgram();
private:
	static const wchar_t* applicatinName;
};

