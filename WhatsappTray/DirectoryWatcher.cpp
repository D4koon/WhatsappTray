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
#include "ReadDirectoryChanges/ReadDirectoryChanges.h"

DirectoryWatcher::DirectoryWatcher(std::string directory, const std::function<void(const DWORD, std::string)>& directoryChangedHandler)
	: watchedDirectory(directory)
	, directoryChangedEvent(directoryChangedHandler)
	, watcherThread(&DirectoryWatcher::WatchDirectoryWorker, this, directory)
{
}

void DirectoryWatcher::WatchDirectoryWorker(std::string directory)
{
	const DWORD dwNotificationFlags =
		FILE_NOTIFY_CHANGE_LAST_WRITE
		| FILE_NOTIFY_CHANGE_CREATION
		| FILE_NOTIFY_CHANGE_FILE_NAME;


	CReadDirectoryChanges changes;
	changes.AddDirectory(directory.c_str(), false, dwNotificationFlags);

	HANDLE hStdIn = ::GetStdHandle(STD_INPUT_HANDLE);
	const HANDLE handles = changes.GetWaitHandle();

	bool bTerminate = false;

	while (!bTerminate) {
		DWORD rc = ::WaitForSingleObjectEx(handles, INFINITE, true);
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
				Logger::Debug("%s %s", ExplainAction(dwAction).c_str(), strFilename);

				directoryChangedEvent(dwAction, std::string(strFilename));
			}
		}
		break;
		case WAIT_IO_COMPLETION:
			// Nothing to do.
			break;
		}
	}
}

std::string DirectoryWatcher::ExplainAction(DWORD dwAction)
{
	switch (dwAction) {
	case FILE_ACTION_ADDED:
		return "Added";
	case FILE_ACTION_REMOVED:
		return "Deleted";
	case FILE_ACTION_MODIFIED:
		return "Modified";
	case FILE_ACTION_RENAMED_OLD_NAME:
		return "Renamed From";
	case FILE_ACTION_RENAMED_NEW_NAME:
		return "Renamed To";
	default:
		return "BAD DATA";
	}
}
