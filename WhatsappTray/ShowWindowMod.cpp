/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright(C) 2020 - 2022 WhatsappTray Sebastian Amann */

#include "ShowWindowMod.h"
#include "WinSockLogger.h"

#include "inttypes.h"
#include <iostream>
#include <sstream>
#include <string>
#include <windows.h>

constexpr char* MODULE_NAME = "ShowWindowMod";

static bool _showWindowFunctionIsBlocked = false;

/**
 * @brief Block the ShowWindow()-function so that WhatsApp can no longer call it.
 *        This is done because otherwise it messes with the start-minimized-feature of WhatsappTray
 *
 *        Normaly WhatsApp calls ShowWindow() shortly before it is finished with initialization.
 *
 * @warning This has some sideeffects, see issue 115 and 118
 *          -> The context-menue in the textfield will be blocked and the SaveAs-dialog is broke, when for example trying to download some documents
 *          -> Unblock as soon as it is no longer needed!
 */
bool BlockShowWindowFunction()
{
	// Get the User32.dll-handle
	auto hLib = LoadLibrary("User32.dll");
	if (hLib == NULL) {
		LogString("Error loading User32.dll");
		return false;
	}
	LogString("loading User32.dll finished");

	// Get the address of the ShowWindow()-function of the User32.dll
	auto showWindowFunc = (HOOKPROC)GetProcAddress(hLib, "ShowWindow");
	if (showWindowFunc == NULL) {
		LogString("The function 'ShowWindow' was NOT found");
		return false;
	}
	LogString("The function 'ShowWindow' was found (0x%" PRIx64 ")", showWindowFunc);

	// Change the protection-level of this memory-region, because it normaly has read,execute
	// NOTE: If this is not done WhatsApp will crash!
	DWORD oldProtect;
	if (VirtualProtect(showWindowFunc, 20, PAGE_EXECUTE_READWRITE, &oldProtect) == NULL) {
		return false;
	}

	// Read the first byte of the ShowWindow()-function
	auto showWindowFunc_FirstByte = *((uint8_t*)showWindowFunc);
	LogString("First byte of the ShowWindow()-function =0x%" PRIX8 " (before change) NOTE: 0xFF is expected", showWindowFunc_FirstByte);

	// Write 0xC3 to the first byte of the ShowWindow()-function
	// This translate to a "RET"-command so the function will immediatly return instead of the normal jmp-command
	*((uint8_t*)showWindowFunc) = 0xC3;

	// Read the first byte of the ShowWindow()-function to see that it has worked
	showWindowFunc_FirstByte = *((uint8_t*)showWindowFunc);
	LogString("First byte of the ShowWindow()-function =0x%" PRIX8 " (after change) NOTE: 0xC3 is expected", showWindowFunc_FirstByte);

	_showWindowFunctionIsBlocked = true;

	return true;
}

/**
 * @brief Restore the ShowWindow()-function so that WhatsApp can call it again.
 */
bool UnblockShowWindowFunction()
{
	LogString("Try to unbock ShowWindow()-function");

	if (_showWindowFunctionIsBlocked == true) {
		LogString("Unblock ShowWindow()-function");

		// Get the User32.dll-handle
		auto hLib = LoadLibrary("User32.dll");
		if (hLib == NULL) {
			LogString("Error loading User32.dll");
			return false;
		}
		LogString("loading User32.dll finished");

		// Get the address of the ShowWindow()-function of the User32.dll
		auto showWindowFunc = (HOOKPROC)GetProcAddress(hLib, "ShowWindow");
		if (showWindowFunc == NULL) {
			LogString("The function 'ShowWindow' was NOT found");
			return false;
		}
		LogString("The function 'ShowWindow' was found (0x%" PRIx64 ")", showWindowFunc);

		// Change the protection-level of this memory-region, because it normaly has read,execute
		// NOTE: If this is not done WhatsApp will crash!
		DWORD oldProtect;
		if (VirtualProtect(showWindowFunc, 20, PAGE_EXECUTE_READWRITE, &oldProtect) == NULL) {
			return false;
		}

		// Read the first byte of the ShowWindow()-function
		auto showWindowFunc_FirstByte = *((uint8_t*)showWindowFunc);
		LogString("First byte of the ShowWindow()-function =0x%" PRIX8 " (before change) NOTE: 0xC3 is expected", showWindowFunc_FirstByte);

		// Write 0xFF to the first byte of the ShowWindow()-function
		// This should restore the original function
		*((uint8_t*)showWindowFunc) = 0xFF;

		// Read the first byte of the ShowWindow()-function to see that it has worked
		showWindowFunc_FirstByte = *((uint8_t*)showWindowFunc);
		LogString("First byte of the ShowWindow()-function =0x%" PRIX8 " (after change) NOTE: 0xFF is expected", showWindowFunc_FirstByte);

		_showWindowFunctionIsBlocked = false;
	}

	return true;
}