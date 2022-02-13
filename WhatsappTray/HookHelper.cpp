/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright(C) 2020 - 2022 WhatsappTray Sebastian Amann */

#include "HookHelper.h"
#include "SharedDefines.h"

constexpr char* MODULE_NAME = "HookHelper";

/**
 * @brief Returns the window-handle for the window with the searched name in the current process
 *
 * https://stackoverflow.com/questions/31384004/can-several-windows-be-bound-to-the-same-process
 * Windows are associated with threads.
 * Threads are associated with processes.
 * A thread can create as many top-level windows as it likes.
 * Therefore you can perfectly well have multiple top-level windows associated with the same thread.
 * @return
*/
HWND GetTopLevelWindowhandleWithName(std::string searchedWindowTitle, DWORD _processID)
{
	HWND iteratedHwnd = NULL;
	while (true) {
		iteratedHwnd = FindWindowExA(NULL, iteratedHwnd, NULL, WHATSAPP_CLIENT_NAME);
		if (iteratedHwnd == NULL) {
			return NULL;
		}

		DWORD processId;
		DWORD threadId = GetWindowThreadProcessId(iteratedHwnd, &processId);

		// Check processId and Title
		// Already observerd an instance where the processId alone lead to an false match!
		if (_processID != processId) {
			// processId does not match -> continue looking
			continue;
		}
		// Found matching processId

		if (iteratedHwnd != GetAncestor(iteratedHwnd, GA_ROOT)) {
			//LogString("Window is not a toplevel-window");
			continue;
		}

		auto windowTitle = GetWindowTitle(iteratedHwnd);
		// Also check length because compare also matches strings that are longer like 'WhatsApp Voip'
		if (windowTitle.compare(searchedWindowTitle) != 0 || windowTitle.length() != strlen(WHATSAPP_CLIENT_NAME)) {
			//LogString("windowTitle='" + windowTitle + "' does not match '" WHATSAPP_CLIENT_NAME "'");
			continue;
		}

		// Window handle found
		break;
	}

	return iteratedHwnd;
}

/**
 * @brief Gets the text of a window.
 */
std::string GetWindowTitle(HWND hwnd)
{
	char windowNameBuffer[2000];
	GetWindowTextA(hwnd, windowNameBuffer, sizeof(windowNameBuffer));

	return std::string(windowNameBuffer);
}

POINT LParamToPoint(LPARAM lParam)
{
	POINT p = { LOWORD(lParam), HIWORD(lParam) };

	return p;
}

/**
 * @brief Get the path to the executable for the ProcessID
 *
 * @param processId The ProcessID from which the path to the executable should be fetched
 * @return The path to the executable from the ProcessID
*/
std::string GetFilepathFromProcessID(DWORD processId)
{
	HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
	if (processHandle == NULL) {
		return "";
	}

	wchar_t filepath[MAX_PATH];
	if (GetModuleFileNameExW(processHandle, NULL, filepath, MAX_PATH) == 0) {
		CloseHandle(processHandle);
		return "";
	}
	CloseHandle(processHandle);

	return WideToUtf8(filepath);
}

std::string GetEnviromentVariable(const std::string& inputString)
{
	size_t requiredSize;
	const size_t varbufferSize = 2000;
	char varbuffer[varbufferSize];

	getenv_s(&requiredSize, varbuffer, varbufferSize, inputString.c_str());

	return varbuffer;
}

std::string WideToUtf8(const std::wstring& inputString)
{
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, inputString.c_str(), (int)inputString.size(), NULL, 0, NULL, NULL);
	std::string strTo(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, inputString.c_str(), (int)inputString.size(), &strTo[0], size_needed, NULL, NULL);
	return strTo;
}

/**
 * @brief Save a HBitmap to a file
*/
bool SaveHBITMAPToFile(HBITMAP hBitmap, LPCTSTR fileName)
{
	DWORD dwPaletteSize = 0, dwDIBSize = 0;
	HDC hDC = CreateDC(TEXT("DISPLAY"), NULL, NULL, NULL);
	int iBits = GetDeviceCaps(hDC, BITSPIXEL) * GetDeviceCaps(hDC, PLANES);
	DeleteDC(hDC);

	WORD wBitCount;
	if (iBits <= 1) {
		wBitCount = 1;
	} else if (iBits <= 4) {
		wBitCount = 4;
	} else if (iBits <= 8) {
		wBitCount = 8;
	} else {
		wBitCount = 24;
	}

	BITMAP bitmap;
	GetObject(hBitmap, sizeof(bitmap), (LPSTR)&bitmap);

	BITMAPINFOHEADER bi{ 0 };
	bi.biSize = sizeof(BITMAPINFOHEADER);
	bi.biWidth = bitmap.bmWidth;
	bi.biHeight = -bitmap.bmHeight;
	bi.biPlanes = 1;
	bi.biBitCount = wBitCount;
	bi.biCompression = BI_RGB;
	bi.biSizeImage = 0;
	bi.biXPelsPerMeter = 0;
	bi.biYPelsPerMeter = 0;
	bi.biClrImportant = 0;
	bi.biClrUsed = 256;
	auto dwBmBitsSize = (((bitmap.bmWidth * wBitCount + 31) & ~31) / 8 * bitmap.bmHeight);
	HANDLE hDib = GlobalAlloc(GHND, sizeof(BITMAPINFOHEADER) + dwBmBitsSize + dwPaletteSize);

	if (hDib == NULL) {
		LogString("hDib == NULL");
		return false;
	}

	LPBITMAPINFOHEADER lpbi = (LPBITMAPINFOHEADER)GlobalLock(hDib);
	if (lpbi == NULL) {
		LogString("lpbi == NULL");
		return false;
	}
	*lpbi = bi;

	HANDLE hPal = GetStockObject(DEFAULT_PALETTE);
	HANDLE hOldPal2 = NULL;
	if (hPal) {
		hDC = GetDC(NULL);
		hOldPal2 = SelectPalette(hDC, (HPALETTE)hPal, FALSE);
		RealizePalette(hDC);
	}

	GetDIBits(hDC, hBitmap, 0, (UINT)bitmap.bmHeight, (LPSTR)lpbi + sizeof(BITMAPINFOHEADER) + dwPaletteSize, (BITMAPINFO*)lpbi, DIB_RGB_COLORS);

	if (hOldPal2) {
		SelectPalette(hDC, (HPALETTE)hOldPal2, TRUE);
		RealizePalette(hDC);
		ReleaseDC(NULL, hDC);
	}

	HANDLE fh = CreateFile(fileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);

	if (fh == INVALID_HANDLE_VALUE) {
		LogString("fh == INVALID_HANDLE_VALUE");
		return false;
	}

	BITMAPFILEHEADER bmfHdr;
	bmfHdr.bfType = 0x4D42; // "BM"
	dwDIBSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + dwPaletteSize + dwBmBitsSize;
	bmfHdr.bfSize = dwDIBSize;
	bmfHdr.bfReserved1 = 0;
	bmfHdr.bfReserved2 = 0;
	bmfHdr.bfOffBits = (DWORD)sizeof(BITMAPFILEHEADER) + (DWORD)sizeof(BITMAPINFOHEADER) + dwPaletteSize;

	DWORD bytesWritten;
	WriteFile(fh, (LPSTR)&bmfHdr, sizeof(BITMAPFILEHEADER), &bytesWritten, NULL);
	WriteFile(fh, (LPSTR)lpbi, dwDIBSize, &bytesWritten, NULL);
	LogString("Icon was written to file-path=%s", fileName);

	GlobalUnlock(hDib);
	GlobalFree(hDib);
	CloseHandle(fh);

	return true;
}

/**
 * @brief Save a HIcon to a file
*/
bool SaveHIconToFile(HICON hIcon, std::string fileName)
{
	ICONINFO picInfo;
	auto infoRet = GetIconInfo(hIcon, &picInfo);
	LogString("GetIconInfo-returnvalue=%d", infoRet);

	BITMAP bitmap{};
	auto getObjectRet = GetObject(picInfo.hbmColor, sizeof(bitmap), &bitmap);
	LogString("getObjectRet=%d widht=%d height=%d", getObjectRet, bitmap.bmWidth, bitmap.bmHeight);

	auto retValue = SaveHBITMAPToFile(picInfo.hbmColor, fileName.c_str());

	// Clean up the data from GetIconInfo()
	::DeleteObject(picInfo.hbmMask);
	::DeleteObject(picInfo.hbmColor);

	return retValue;
}
