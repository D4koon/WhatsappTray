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

#include "stdafx.h"
#include "Logger.h"

#include "Helper.h"

#include <windows.h>
#include <iostream>
#include <time.h>

Logger *Logger::loggerInstance = NULL;
Loglevel Logger::loglevelToLog = Loglevel::LOG_ERROR;

Logger::Logger()
{
	if (Logger::loglevelToLog != Loglevel::LOG_NONE) {
		time_t now = time(0);
		struct tm localTimeStruct;
		int errorNr = localtime_s(&localTimeStruct, &now);

		const int LOGNAME_SIZE = 50;
		static char timeString[LOGNAME_SIZE];
		strftime(timeString, LOGNAME_SIZE, "%Y-%m-%d_%H#%M#%S", &localTimeStruct);

		std::string fileUrl = Helper::GetApplicationExePath();
		fileUrl.append("Log_");
		fileUrl.append(timeString);
		fileUrl.append(".txt");

		std::cout << "Log to " << fileUrl << "\n";

		logFile.open(fileUrl.c_str(), std::ofstream::out);
		if ((logFile.rdstate() & std::ofstream::failbit) != 0) {
			std::cout << "ERROR: Logfile could not be created!\n";
		}
	}
}

Logger::~Logger()
{
	if (logFile.is_open()) {
		logFile.close();
	}
}

Logger& Logger::GetInstance()
{
	if (loggerInstance == NULL) {
		loggerInstance = new Logger();
	}
	return *loggerInstance;
}

void Logger::ReleaseInstance()
{
	if (loggerInstance != NULL) {
		delete loggerInstance;
		loggerInstance = NULL;
	}
}

bool Logger::Log(Loglevel loglevel, std::string text, ...)
{
	bool returnvalue = false;

	va_list argptr;
	va_start(argptr, text);
	LogVariadic(loglevel, text, argptr);
	va_end(argptr);

	return returnvalue;
}

bool Logger::LogLine(Loglevel loglevel, std::string text, ...)
{
	bool returnvalue = false;

	va_list argptr;
	va_start(argptr, text);
	LogVariadic(loglevel, text + "\n", argptr);
	va_end(argptr);

	return returnvalue;
}

bool Logger::LogVariadic(Loglevel loglevel, std::string text, va_list argptr)
{
	const size_t buffersize = 5000;
	int count = 0;
	char charBuffer[buffersize];
	count = vsnprintf(charBuffer, buffersize, text.c_str(), argptr);

	if (count > buffersize) {
		OutputDebugStringA("Error: The buffer was to small for the logstring. It has to be adusted in the code if that happens.");
		std::cerr << "Error: The buffer was to small for the logstring. It has to be adusted in the code if that happens.";
	}

	// Log everything above warning so that we can use DebugView for debugging.
	if (loglevel <= Loglevel::LOG_WARNING) {
		OutputDebugStringA(charBuffer);
	}

	// If the loglevel is above the maximum, nothing shall be loged.
	if (loglevel > Logger::loglevelToLog) {
		return false;
	}

	std::string errortext = "";
	switch (loglevel) {
	case Loglevel::LOG_APP: break;
	case Loglevel::LOG_FATAL: errortext.append("FATAL: "); break;
	case Loglevel::LOG_ERROR: errortext.append("ERROR: "); break;
	case Loglevel::LOG_WARNING: errortext.append("WARNING: "); break;
	case Loglevel::LOG_INFO: errortext.append("INFO: "); break;
	case Loglevel::LOG_DEBUG: errortext.append("DEBUG: "); break;
	}
	errortext.append(text);

	// Write text to console.
	vfprintf(stdout, errortext.c_str(), argptr);

	logFile << charBuffer;
	logFile.flush();

	return true;
}

bool Logger::App(std::string text, ...)
{
	va_list argptr;
	va_start(argptr, text);
	bool returnValue = Logger::GetInstance().LogVariadic(Loglevel::LOG_APP, text + "\n", argptr);
	va_end(argptr);
	return returnValue;
}
bool Logger::Fatal(std::string text, ...)
{
	va_list argptr;
	va_start(argptr, text);
	bool returnValue = Logger::GetInstance().LogVariadic(Loglevel::LOG_APP, text + "\n", argptr);
	va_end(argptr);
	return returnValue;
}
bool Logger::Error(std::string text, ...)
{
	va_list argptr;
	va_start(argptr, text);
	bool returnValue = Logger::GetInstance().LogVariadic(Loglevel::LOG_APP, text + "\n", argptr);
	va_end(argptr);
	return returnValue;
}
bool Logger::Warning(std::string text, ...)
{
	va_list argptr;
	va_start(argptr, text);
	bool returnValue = Logger::GetInstance().LogVariadic(Loglevel::LOG_APP, text + "\n", argptr);
	va_end(argptr);
	return returnValue;
}
bool Logger::Info(std::string text, ...)
{
	va_list argptr;
	va_start(argptr, text);
	bool returnValue = Logger::GetInstance().LogVariadic(Loglevel::LOG_APP, text + "\n", argptr);
	va_end(argptr);
	return returnValue;
}
bool Logger::Debug(std::string text, ...)
{
	va_list argptr;
	va_start(argptr, text);
	bool returnValue = Logger::GetInstance().LogVariadic(Loglevel::LOG_APP, text + "\n", argptr);
	va_end(argptr);
	return returnValue;
}
