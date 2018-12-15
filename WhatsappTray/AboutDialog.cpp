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

#include "AboutDialog.h"

#include "resource.h"
#include "Helper.h"
#include "Logger.h"

#include <map>
#include <string>

#undef MODULE_NAME
#define MODULE_NAME "AboutDialog::"

void AboutDialog::Create(_In_opt_ HINSTANCE hInstance, HWND _hwndWhatsappTray)
{
	DialogBoxA(hInstance, MAKEINTRESOURCE(IDD_ABOUT), _hwndWhatsappTray, (DLGPROC)AboutDialog::DlgProc);
}

INT_PTR CALLBACK AboutDialog::DlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_INITDIALOG:
	{
		std::string labelString = std::string("WhatsappTray ") + Helper::GetProductAndVersion();
		auto versionLabel = GetDlgItem(hWnd, IDC_VERSION_TEXT);
		SetWindowTextA(versionLabel, labelString.c_str());
		break;
	}
	case WM_CLOSE:
		PostMessage(hWnd, WM_COMMAND, IDCANCEL, 0);
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			EndDialog(hWnd, TRUE);
			break;
		case IDCANCEL:
			EndDialog(hWnd, FALSE);
			break;
		}
		break;
	default:
		return FALSE;
	}
	return TRUE;
}
