// WinSockServer.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <iostream>
#include <functional>

bool SocketStart(char portString[]);
void SocketNotifyOnNewMessage(const std::function<void(std::string)>& messageReceivedEvent);
void SocketStopServer();
