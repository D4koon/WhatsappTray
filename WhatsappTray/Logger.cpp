/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright(C) 1998 - 2018 WhatsappTray Sebastian Amann */

#include "stdafx.h"
#include "Logger.h"

#include "Helper.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>

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

	auto timeString = Logger::GetTimeString("%Y-%m-%d_%H#%M#%S");

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
	auto returnValue = LogVariadic(Loglevel::LOG_APP, text + "\n", argptr);
	va_end(argptr);
	return returnValue;
}
bool Logger::Fatal(std::string text, ...)
{
	va_list argptr;
	va_start(argptr, text);
	auto returnValue = LogVariadic(Loglevel::LOG_APP, text + "\n", argptr);
	va_end(argptr);
	return returnValue;
}
bool Logger::Error(std::string text, ...)
{
	va_list argptr;
	va_start(argptr, text);
	auto returnValue = LogVariadic(Loglevel::LOG_APP, text + "\n", argptr);
	va_end(argptr);
	return returnValue;
}
bool Logger::Warning(std::string text, ...)
{
	va_list argptr;
	va_start(argptr, text);
	auto returnValue = LogVariadic(Loglevel::LOG_APP, text + "\n", argptr);
	va_end(argptr);
	return returnValue;
}
bool Logger::Info(std::string text, ...)
{
	va_list argptr;
	va_start(argptr, text);
	auto returnValue = LogVariadic(Loglevel::LOG_APP, text + "\n", argptr);
	va_end(argptr);
	return returnValue;
}
bool Logger::Debug(std::string text, ...)
{
	va_list argptr;
	va_start(argptr, text);
	auto returnValue = LogVariadic(Loglevel::LOG_APP, text + "\n", argptr);
	va_end(argptr);
	return returnValue;
}

bool Logger::LogLine(Loglevel loglevel, std::string text, ...)
{
	va_list argptr;
	va_start(argptr, text);
	auto returnValue = LogVariadic(loglevel, text + "\n", argptr);
	va_end(argptr);

	return returnValue;
}

bool Logger::Log(Loglevel loglevel, std::string text, ...)
{
	va_list argptr;
	va_start(argptr, text);
	auto returnvalue = LogVariadic(loglevel, text, argptr);
	va_end(argptr);

	return returnvalue;
}

bool Logger::LogVariadic(Loglevel loglevel, std::string logFormatString, va_list argptr)
{
	const size_t buffersize = 5000;
	char logStringBuffer[buffersize];
	auto count = vsnprintf(logStringBuffer, buffersize, logFormatString.c_str(), argptr);

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
	// Append timestamp
	auto timeString = Logger::GetTimeString("%H:%M:%S", true);
	logText.append(timeString + " - ");

	// Append loglevel
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

std::string Logger::GetTimeString(const char* formatString, bool withMilliseconds)
{
	using namespace std::chrono;

	// Get current time
	auto now = system_clock::now();

	// Get number of milliseconds for the current second
	// (remainder after division into seconds)
	auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

	// Convert to std::time_t in order to convert to std::tm (broken time)
	auto timer = system_clock::to_time_t(now);

	// Convert to broken time
	struct tm local_time;
	localtime_s(&local_time, &timer);

	std::ostringstream oss;

	oss << std::put_time(&local_time, formatString);
	if (withMilliseconds)
	{
		oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
	}
	
	return oss.str();
}
