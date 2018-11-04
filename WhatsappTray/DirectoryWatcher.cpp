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

#include <filesystem>


namespace fs = std::experimental::filesystem;

DirectoryWatcher::DirectoryWatcher(std::string directory, HWND notifyWindowHandle, UINT message)
	: watchedDirectory(directory)
	, notifyWindowHandle(notifyWindowHandle)
	, message(message)
	, watcherThread(&DirectoryWatcher::WatchDirectoryWorker, this, directory)
{
}

void DirectoryWatcher::WatchDirectoryWorker(std::string directory)
{
	DWORD dwWaitStatus;
	HANDLE dwChangeHandle;

	// Watch the directory for file creation and deletion. 
	dwChangeHandle = FindFirstChangeNotification(
		directory.c_str(),                         // directory to watch 
		FALSE,                         // do not watch subtree 
		FILE_NOTIFY_CHANGE_FILE_NAME); // watch file name changes 

	if (dwChangeHandle == INVALID_HANDLE_VALUE) {
		Logger::Error("ERROR: FindFirstChangeNotification function failed.");
		return;
	}

	// Make a final validation check on our handle.

	if (dwChangeHandle == NULL) {
		Logger::Error("ERROR: Unexpected NULL from FindFirstChangeNotification.");
		return;
	}

	// Change notification is set. Now wait on both notification 
	// handles and refresh accordingly. 

	while (TRUE) {
		// Wait for notification.

		Logger::Debug("Waiting for notification...");

		dwWaitStatus = WaitForSingleObject(dwChangeHandle, INFINITE);

		switch (dwWaitStatus) {
		case WAIT_OBJECT_0:

			// A file was created, renamed, or deleted in the directory.
			// Refresh this directory and restart the notification.

			RefreshDirectory();
			if (FindNextChangeNotification(dwChangeHandle) == FALSE) {
				Logger::Error("ERROR: FindNextChangeNotification function failed.");
				ExitProcess(GetLastError());
			}
			break;

		case WAIT_TIMEOUT:

			// A timeout occurred, this would happen if some value other 
			// than INFINITE is used in the Wait call and no changes occur.
			// In a single-threaded environment you might not want an
			// INFINITE wait.

			Logger::Debug("No changes in the timeout period.");
			break;

		default:
			Logger::Error("ERROR: Unhandled dwWaitStatus.");
			ExitProcess(GetLastError());
			break;
		}
	}
}

void DirectoryWatcher::RefreshDirectory()
{
	// This is where you might place code to refresh your
	// directory listing, but not the subtree because it
	// would not be necessary.

	Logger::Debug("Directory (%s) changed.", watchedDirectory.c_str());

	for (const auto& p : fs::directory_iterator(watchedDirectory)) {
		std::string filnameString = p.path().filename().u8string();
		Logger::Debug("File '%s'", filnameString.c_str());
		//std::cout << p << std::endl; // "p" is the directory entry. Get the path with "p.path()".
	}

	PostMessageA(notifyWindowHandle, message, 0, 0);
}
