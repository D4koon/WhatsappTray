/*
*
* WhatsappTray
* Copyright (C) 1998-2018 Sebastian Amann
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
#include <fstream>
#include <string>

enum class Loglevel
{
	LOG_NONE,
	LOG_APP,
	LOG_FATAL,
	LOG_ERROR,
	LOG_WARNING,
	LOG_INFO,
	LOG_DEBUG,
};

class Logger
{
private:
	Logger();
	~Logger();

	static std::ofstream logFile;

	bool Log(Loglevel loglevel, std::string text, ...);
	bool LogLine(Loglevel loglevel, std::string text, ...);
	static bool LogVariadic(Loglevel loglevel, std::string text, va_list vadriaicList);
	static void ProcessLog(const Loglevel loglevel, const char* logTextBuffer);

public:
	static Loglevel loglevelToLog;
	static bool isSetupDone;
	static void Setup();
	static void ReleaseInstance();
	static bool App(std::string text, ...);
	static bool Fatal(std::string text, ...);
	static bool Error(std::string text, ...);
	static bool Warning(std::string text, ...);
	static bool Info(std::string text, ...);
	static bool Debug(std::string text, ...);
};
