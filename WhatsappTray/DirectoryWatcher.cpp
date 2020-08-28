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

#include "DirectoryWatcher.h"
#include "Logger.h"
#include "Helper.h"
#include "ReadDirectoryChanges/ReadDirectoryChanges.h"

/*
 * NOTE: It is better to use wstring for paths because of unicode-charcters that can happen in other languages
 */
DirectoryWatcher::DirectoryWatcher(std::wstring directory, const std::function<void(const DWORD, std::wstring)>& directoryChangedHandler)
	: watchedDirectory(directory)
	, directoryChangedEvent(directoryChangedHandler)
	, watcherThread(&DirectoryWatcher::WatchDirectoryWorker, this, directory)
	, terminate(false)
	, WaitHandle()
{
}

DirectoryWatcher::~DirectoryWatcher()
{
	// We need to wait for the thread to finish to not trigger an exception.
	StopThread();
}

void DirectoryWatcher::WatchDirectoryWorker(std::wstring directory)
{
	const DWORD dwNotificationFlags =
		FILE_NOTIFY_CHANGE_LAST_WRITE
		| FILE_NOTIFY_CHANGE_CREATION
		| FILE_NOTIFY_CHANGE_FILE_NAME;

	CReadDirectoryChanges changes;
	changes.AddDirectory(directory.c_str(), false, dwNotificationFlags);

	WaitHandle = changes.GetWaitHandle();

	while (true) {
		DWORD rc = ::WaitForSingleObjectEx(WaitHandle, INFINITE, true);
		if (terminate) {
			return;
		}
		switch (rc) {
		case WAIT_OBJECT_0 + 0:
		{
			// We've received a notification in the queue.
			if (changes.CheckOverflow()) {
				Logger::Error("Queue overflowed.");
			} else {
				DWORD dwAction;
				CStringA strFilename;
				changes.Pop(dwAction, strFilename);
				//Logger::Debug("%s %s", CReadDirectoryChanges::ActionToString(dwAction).c_str(), strFilename);

				directoryChangedEvent(dwAction, Helper::Utf8ToWide(std::string(strFilename)));
			}
		}
		break;
		case WAIT_IO_COMPLETION:
			// Nothing to do.
			break;
		}
	}
}

/**
 * Stops the thread and waits till it is finished.
 */
void DirectoryWatcher::StopThread()
{
	terminate = true;
	// Increase count by one so the thread continues and can end.
	ReleaseSemaphore(WaitHandle, 1, NULL);
	watcherThread.join();
}