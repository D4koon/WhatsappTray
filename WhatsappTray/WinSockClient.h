/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright(C) 2020 - 2020 WhatsappTray Sebastian Amann */

// WinSockClient header

#pragma once

#include <iostream>
#include <thread>

void SocketSendMessage(const char message[]);
void SocketStart(const char ipString[], const char portString[]);
void SocketStop(bool waitForEmptyBuffer = true, bool waitForShutdown = true);
