/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright(C) 2020 - 2020 WhatsappTray Sebastian Amann */

#include "WinSockLogger.h"

void WinSockLogger::TraceString(const std::string traceString)
{
	SocketSendMessage(traceString.c_str());

	//#ifdef _DEBUG
	//	OutputDebugStringA(traceString.c_str());
	//#endif
}

void WinSockLogger::TraceStream(std::ostringstream& traceBuffer)
{
	SocketSendMessage(traceBuffer.str().c_str());
	traceBuffer.clear();
	traceBuffer.str(std::string());

	//#ifdef _DEBUG
	//	OutputDebugStringA(traceBuffer.str().c_str());
	//	traceBuffer.clear();
	//	traceBuffer.str(std::string());
	//#endif
}