/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright(C) 2020 - 2020 WhatsappTray Sebastian Amann */

#pragma once

#include <thread>
#include <functional>

class DirectoryWatcher;

class WhatsAppApi
{
public:
	static bool IsFullInit;
	static void WhatsAppApi::NotifyOnNewMessage(const std::function<void()>& newMessageHandler);
	static void WhatsAppApi::NotifyOnFullInit(const std::function<void()>& newMessageHandler);
	static void Init();
private:
	WhatsAppApi() { }
	~WhatsAppApi() { }
	static std::unique_ptr<DirectoryWatcher> dirWatcher;
	static std::function<void()> receivedMessageEvent;
	static std::function<void()> receivedFullInitEvent;
	static void IndexedDbChanged(const DWORD dwAction, std::wstring strFilename);
};

