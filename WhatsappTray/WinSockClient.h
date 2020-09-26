// WinSockClient.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <iostream>

bool SocketInit();
void SocketCleanup();
bool SocketSendMessage(const char ipString[], const char portString[], const char message[]);
