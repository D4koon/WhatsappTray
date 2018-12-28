/*
 * 
 * WhatsappTray
 * Copyright (C) 1998-2018 Sebastian Amann
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

#include "stdafx.h"

#include "WhatsAppApi.h"

#include "DirectoryWatcher.h"
#include "Logger.h"

#include <regex>
#include <Mmsystem.h>

#undef MODULE_NAME
#define MODULE_NAME "WhatsAppApi::"


std::unique_ptr<DirectoryWatcher> WhatsAppApi::dirWatcher(new DirectoryWatcher("C:\\Users\\Dakoon\\AppData\\Roaming\\WhatsApp\\IndexedDB\\file__0.indexeddb.leveldb", &WhatsAppApi::IndexedDbChanged));
std::function<void()> WhatsAppApi::newMessageEvent = NULL;

void WhatsAppApi::NotifyOnNewMessage(const std::function<void()>& newMessageHandler)
{
	newMessageEvent = newMessageHandler;
}

void WhatsAppApi::IndexedDbChanged(const DWORD dwAction, std::string strFilename)
{
	// The logfiles change so we have to keep track of them.
	static std::string lastLogfile = "";
	static size_t processedLineCount = 0;

	if (strFilename.find(".log") == std::string::npos) {
		return;
	}

	std::string lineBuffer;
	if (strFilename.compare(lastLogfile) != 0) {
		Logger::Info(MODULE_NAME "IndexedDbChanged() - The logfile has changed");
		lastLogfile = strFilename;

		// We have to reset the lineNumber when we find a new log-file ...
		// Open the file in binary-mode else the eof is wrong.
		std::ifstream file(strFilename.c_str(), std::ios_base::in | std::ios_base::binary);
		if (file.is_open() == false) {
			Logger::Error(MODULE_NAME "IndexedDbChanged() - file.is_open() == false");
			return;
		}

		for (size_t lineCounter = 0; true; ) {
			int character = file.get();

			if (character == '\r') {
				lineCounter++;
			}

			if (file.fail()) {
				Logger::Debug(MODULE_NAME "IndexedDbChanged() - file.fail()");
				processedLineCount = lineCounter;
				break;
			}

			if (file.eof()) {
				Logger::Debug(MODULE_NAME "IndexedDbChanged() - file.eof()");
				processedLineCount = lineCounter;
				break;
			}
		}

		return;
	}

	// Open the file in binary-mode else the eof is wrong.
	std::ifstream file(strFilename.c_str(), std::ios_base::in | std::ios_base::binary);
	if (file.is_open() == false) {
		Logger::Error(MODULE_NAME "IndexedDbChanged() - file.is_open() == false");
		return;
	}

	for (size_t lineCounter = 0; true; lineCounter++) {
		std::getline(file, lineBuffer, '\r');

		if (file.fail()) {
			Logger::Debug(MODULE_NAME "IndexedDbChanged() - file.fail()");
			return;
		}

		if (file.eof()) {
			Logger::Debug(MODULE_NAME "IndexedDbChanged() - file.eof()");
			return;
		}

		if (lineCounter <= processedLineCount) {
			// This line was already processed.
			continue;
		}
		processedLineCount = lineCounter;

		if (lineBuffer.find("recv:") == std::string::npos) {
			continue;
		}

		Logger::Debug(MODULE_NAME "IndexedDbChanged() - Found recv: '%s'", lineBuffer.c_str());

		// Match: recv: [0-9a-f]{16}[.]--[0-9a-f]+\"\ttimestampN
		if (std::regex_search(lineBuffer.c_str(), std::regex("recv: [0-9a-f]{16}[.]--[0-9a-f]+\"\\ttimestampN")) == false) {
			continue;
		}

		Logger::Debug(MODULE_NAME "IndexedDbChanged() - Found match");

		newMessageEvent();
	}
}
