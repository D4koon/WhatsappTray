/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright(C) 1998 - 2018 WhatsappTray Sebastian Amann */

#pragma once
#include "Helper.h"

#include <fstream>
#include <string>

#define LogInfo(logString, ...) Logger::Info(MODULE_NAME + std::string("::") + std::string(__func__) + ": " + string_format(logString, __VA_ARGS__))
#define LogError(logString, ...) Logger::Error(MODULE_NAME + std::string("::") + std::string(__func__) + ": " + string_format(logString, __VA_ARGS__))

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
	static bool LogVariadic(Loglevel loglevel, std::string text, va_list vadriaicList);
	static void ProcessLog(const Loglevel loglevel, const char* logTextBuffer);
	static std::string GetTimeString(const char* formatString, bool withMilliseconds = false);

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
	static bool LogLine(Loglevel loglevel, std::string text, ...);
};
