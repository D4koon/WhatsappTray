/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright(C) 2020 - 2020 WhatsappTray Sebastian Amann */

#pragma once

#include <iostream>
#include <functional>

bool SocketStart(char portString[]);
void SocketNotifyOnNewMessage(const std::function<void(std::string)>& messageReceivedEvent);
void SocketStopServer();
