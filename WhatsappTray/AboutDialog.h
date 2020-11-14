/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright(C) 1998 - 2018 WhatsappTray Sebastian Amann */

#pragma once

#include <stdint.h>

class AboutDialog
{
public:
	AboutDialog() { }
	~AboutDialog() { }
	static void Create(_In_opt_ HINSTANCE hInstance, HWND _hwndWhatsappTray);
	static INT_PTR DlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	
private:

};

