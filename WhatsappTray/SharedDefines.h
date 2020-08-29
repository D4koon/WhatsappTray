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

#define NAME TEXT("WhatsappTray")
#define WHATSAPP_CLIENT_NAME TEXT("WhatsApp")

#define WM_ADDTRAY  0x0401
#define WM_TRAYCMD  0x0404
#define WM_WHAHTSAPP_CLOSING  0x0405
#define WM_REAPPLY_HOOK  0x0406
#define WM_WHATSAPP_API_NEW_MESSAGE  0x0407
#define WM_WHATSAPP_TO_WHATSAPPTRAY_RECEIVED_WM_CLOSE  0x0408 /* WhatsApp received a WM_CLOSE-message. This message is ment to be sent from the hook inside WhatsApp */
#define WM_WHATSAPPTRAY_TO_WHATSAPP_SEND_WM_CLOSE  0x8000 - 100 /* This message is ment to send to the Whatsapp-window and the hook processes it and should close Whatsapp */
#define IDM_RESTORE 0x1001
#define IDM_CLOSE   0x1002
#define IDM_ABOUT   0x1004
#define IDM_SETTING_CLOSE_TO_TRAY   0x1005
#define IDM_SETTING_LAUNCH_ON_WINDOWS_STARTUP   0x1006
#define IDM_SETTING_START_MINIMIZED   0x1007
#define IDM_SETTING_SHOW_UNREAD_MESSAGES   0x1008

#define DLLIMPORT __declspec(dllexport)

#include <windows.h>

BOOL DLLIMPORT RegisterHook(HMODULE hLib, DWORD threadId, bool closeToTray);
void DLLIMPORT UnRegisterHook();
