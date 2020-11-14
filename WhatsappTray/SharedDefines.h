/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright(C) 1998 - 2018 WhatsappTray Sebastian Amann */

#pragma once

#define NAME TEXT("WhatsappTray")
#define WHATSAPP_CLIENT_NAME TEXT("WhatsApp")
#define WHATSAPPTRAY_LOAD_LIBRARY_TEST_ENV_VAR "WhatsappTrayLoadLibraryTest" /* The enviroment-variable used to test if the hook.dll was triggerd by WhatsappTray's LoadLibrary() */
#define WHATSAPPTRAY_LOAD_LIBRARY_TEST_ENV_VAR_VALUE "TRUE" /* The value of the enviroment-variable used to test if the hook.dll was triggerd by WhatsappTray's LoadLibrary() */

#define LOGGER_IP "127.0.0.1"
// What port to use: https://stackoverflow.com/a/53667220/4870255
// Ports 49152 - 65535 - Free to use these in client programs
#define LOGGER_PORT "52677"

#define WM_WA_MINIMIZE_BUTTON_PRESSED  0x0401 /* The minimize-button in WhatsApp was pressed */
#define WM_WA_CLOSE_BUTTON_PRESSED  0x0402 /* The close-button in WhatsApp was pressed (X) */
#define WM_WA_KEY_PRESSED  0x0403 /* A key in WhatsApp was pressed (X) */
#define WM_TRAYCMD  0x0404
#define WM_WHAHTSAPP_CLOSING  0x0405
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

#include <memory>
#include <string>
#include <stdexcept>

/**
 * @brief 
 * 
 * https://stackoverflow.com/a/26221725/4870255
 */
template<typename ... Args>
std::string string_format(const std::string& format, Args ... args)
{
    size_t size = snprintf(nullptr, 0, format.c_str(), args ...) + 1; // Extra space for '\0'
    if (size <= 0) { throw std::runtime_error("Error during formatting."); }
    std::unique_ptr<char[]> buf(new char[size]);
    snprintf(buf.get(), size, format.c_str(), args ...);
    return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
}