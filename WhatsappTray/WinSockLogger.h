/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright(C) 2020 - 2020 WhatsappTray Sebastian Amann */

#pragma once

#include "WinSockClient.h"
#include "Helper.h"

#include <string>
#include <iostream>
#include <sstream>

#define LogString(logString, ...) WinSockLogger::TraceString(MODULE_NAME + std::string("::") + std::string(__func__) + ": " + string_format(logString, __VA_ARGS__))

class WinSockLogger
{
public:
	static void TraceString(const std::string traceString);
	static void TraceStream(std::ostringstream& traceBuffer);
};
