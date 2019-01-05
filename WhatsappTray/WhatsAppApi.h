/*
*
* WhatsappTray
* Copyright (C) 1998-2018  Sebastian Amann
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

#pragma once

#include <thread>

class DirectoryWatcher;

class WhatsAppApi
{
public:
	static void WhatsAppApi::NotifyOnNewMessage(const std::function<void()>& newMessageHandler);
	static void WhatsAppApi::NotifyOnFullInit(const std::function<void()>& newMessageHandler);
private:
	WhatsAppApi() { }
	~WhatsAppApi() { }
	static std::unique_ptr<DirectoryWatcher> dirWatcher;
	static void IndexedDbChanged(const DWORD dwAction, std::string strFilename);
	static std::function<void()> receivedMessageEvent;
	static std::function<void()> receivedFullInitEvent;

	/// Dummy-value used for initialization. Like a static constructor.
	static bool initDone;
};

