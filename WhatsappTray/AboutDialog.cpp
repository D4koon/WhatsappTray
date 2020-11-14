/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright(C) 1998 - 2018 WhatsappTray Sebastian Amann */

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
