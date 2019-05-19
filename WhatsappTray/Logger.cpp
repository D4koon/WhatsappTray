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

std::ofstream Logger::logFile;
Loglevel Logger::loglevelToLog = Loglevel::LOG_ERROR;
bool Logger::isSetupDone = false;

Logger::Logger()
{
}

Logger::~Logger()
{
}

/**
 * Setup the logging.
 */
void Logger::Setup()
{
	if (isSetupDone == true) {
		OutputDebugStringA("ERROR: The setup for the logger was already done.\n");
		return;
	}

	if (Logger::loglevelToLog == Loglevel::LOG_NONE) {
		OutputDebugStringA("WARNING: The log-level is 'NONE'.\n");
		return;
	}

	time_t now = time(0);
	struct tm localTimeStruct;
	int errorNr = localtime_s(&localTimeStruct, &now);

	const int LOGNAME_SIZE = 50;
	static char timeString[LOGNAME_SIZE];
	strftime(timeString, LOGNAME_SIZE, "%Y-%m-%d_%H#%M#%S", &localTimeStruct);

	std::string logPath = Helper::GetApplicationDirectory() + "log\\";
	std::string logFileName = std::string("Log_") + timeString + std::string(".txt");

	// Create log-folder
	if (CreateDirectory(logPath.c_str(), NULL) == false && ERROR_ALREADY_EXISTS != GetLastError()) {
		OutputDebugStringA("ERROR: Creating the log-folder failed and it was NOT because it did already existed.\n");
		return;
	}

	OutputDebugStringA((std::string("Log to ") + logPath + logFileName + "\n").c_str());

	logFile.open((logPath + logFileName).c_str(), std::ofstream::out);
	if ((logFile.rdstate() & std::ofstream::failbit) != 0) {
		OutputDebugStringA("ERROR: Logfile could not be created!\n");
	}

	isSetupDone = true;
}

void Logger::ReleaseInstance()
{
	if (logFile.is_open()) {
		logFile.close();
	}

	isSetupDone = false;
}

bool Logger::App(std::string text, ...)
{
	va_list argptr;
	va_start(argptr, text);
	bool returnValue = LogVariadic(Loglevel::LOG_APP, text + "\n", argptr);
	va_end(argptr);
	return returnValue;
}
bool Logger::Fatal(std::string text, ...)
{
	va_list argptr;
	va_start(argptr, text);
	bool returnValue = LogVariadic(Loglevel::LOG_APP, text + "\n", argptr);
	va_end(argptr);
	return returnValue;
}
bool Logger::Error(std::string text, ...)
{
	va_list argptr;
	va_start(argptr, text);
	bool returnValue = LogVariadic(Loglevel::LOG_APP, text + "\n", argptr);
	va_end(argptr);
	return returnValue;
}
bool Logger::Warning(std::string text, ...)
{
	va_list argptr;
	va_start(argptr, text);
	bool returnValue = LogVariadic(Loglevel::LOG_APP, text + "\n", argptr);
	va_end(argptr);
	return returnValue;
}
bool Logger::Info(std::string text, ...)
{
	va_list argptr;
	va_start(argptr, text);
	bool returnValue = LogVariadic(Loglevel::LOG_APP, text + "\n", argptr);
	va_end(argptr);
	return returnValue;
}
bool Logger::Debug(std::string text, ...)
{
	va_list argptr;
	va_start(argptr, text);
	bool returnValue = LogVariadic(Loglevel::LOG_APP, text + "\n", argptr);
	va_end(argptr);
	return returnValue;
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

bool Logger::Log(Loglevel loglevel, std::string text, ...)
{
	bool returnvalue = false;

	va_list argptr;
	va_start(argptr, text);
	LogVariadic(loglevel, text, argptr);
	va_end(argptr);

	return returnvalue;
}

bool Logger::LogVariadic(Loglevel loglevel, std::string logFormatString, va_list argptr)
{
	const size_t buffersize = 5000;
	int count = 0;
	char logStringBuffer[buffersize];
	count = vsnprintf(logStringBuffer, buffersize, logFormatString.c_str(), argptr);

	if (count > buffersize) {
		OutputDebugStringA("Error: The buffer was to small for the logstring. It has to be adusted in the code if that happens.");
		std::cerr << "Error: The buffer was to small for the logstring. It has to be adusted in the code if that happens.";
	}

	ProcessLog(loglevel, logStringBuffer);

	return true;
}

void Logger::ProcessLog(const Loglevel loglevel, const char* logTextBuffer)
{
	if (isSetupDone == false) {
		OutputDebugStringA("ERROR: Logger setup was not done!\n");
		return;
	}

	std::string logText = "";
	switch (loglevel) {
	case Loglevel::LOG_APP: break;
	case Loglevel::LOG_FATAL: logText = "FATAL: "; break;
	case Loglevel::LOG_ERROR: logText = "ERROR: "; break;
	case Loglevel::LOG_WARNING: logText = "WARNING: "; break;
	case Loglevel::LOG_INFO: logText = "INFO: "; break;
	case Loglevel::LOG_DEBUG: logText = "DEBUG: "; break;
	}
	logText.append(logTextBuffer);

	// Log everything to VS-console/DebugView for debugging.
#ifdef _DEBUG
	std::wstring wideLogText = Helper::Utf8ToWide(logText);
	OutputDebugStringW(wideLogText.c_str());
#endif

	// If the loglevel is above the maximum, nothing shall be loged.
	if (loglevel > Logger::loglevelToLog) {
		return;
	}

	logFile << logText;
	logFile.flush();
}
