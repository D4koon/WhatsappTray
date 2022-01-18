/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright(C) 1998 - 2017 WhatsappTray Sebastian Amann */

#include "SharedDefines.h"
#include "WindowsMessage.h"
#include "WinSockLogger.h"

#include "inttypes.h"
#include <iostream>
#include <sstream>
#include <string>
#include <windows.h>
#include <future>
#include <thread>
#include <psapi.h> // OpenProcess()
#include <shobjidl.h>   // For ITaskbarList3
//#include <shellscalingapi.h> // For dpi-scaling stuff

//#pragma comment(lib, "SHCore") // For dpi-scaling stuff

// Problems with OutputDebugString:
// Had problems that the messages from OutputDebugString in the hook-functions did not work anymore after some playing arround with old versions, suddenly it worked again.
// Maybe its best to restart pc and hope that it will be fixed, when this happens again...

// NOTES:
// It looks like as if the contrlos like _ and X for minimize or close are custom controls and do not trigger all normal messgages
// For example SC_CLOSE seems to be not sent when clicking the X

// Use DebugView (https://docs.microsoft.com/en-us/sysinternals/downloads/debugview) to see OutputDebugString() messages.

constexpr char* MODULE_NAME = "WhatsappTrayHook";

#define DLLIMPORT __declspec(dllexport)

// For dpi-scaling
typedef enum MONITOR_DPI_TYPE {
	MDT_EFFECTIVE_DPI = 0,
	MDT_ANGULAR_DPI = 1,
	MDT_RAW_DPI = 2,
	MDT_DEFAULT = MDT_EFFECTIVE_DPI
} MONITOR_DPI_TYPE;

static DWORD _processID = NULL;
static HWND _whatsAppWindowHandle = NULL;
static WNDPROC _originalWndProc = NULL;

static std::string _whatsappTrayPath;

static ITaskbarList3* _pTaskbarList = nullptr;   // careful, COM objects should only be accessed from apartment they are created in
static intptr_t _setOverlayIconMemoryAddress = NULL;
static uint64_t _iconCounter = 1; // Start with 1 so 0 can be the signal for no new message

static UINT _dpiX; /* The horizontal dpi-size. Is set in Windows settings. Default 100% = 96 */
static UINT _dpiY; /* The vertical dpi-size. Is set in Windows settings. Default 100% = 96 */

static bool _showWindowFunctionIsBlocked = false;

// Functions from ReadRegister.asm
extern "C" int64_t ReturnRdx();
extern "C" int64_t ReturnRcx();
extern "C" int64_t ReturnRdi();
extern "C" int64_t ReturnR8();
extern "C" int64_t ReturnR9();

typedef HRESULT (STDAPICALLTYPE* GetDpiForMonitorFunc)(HMONITOR, MONITOR_DPI_TYPE, UINT*, UINT*);

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved);
static DWORD WINAPI Init(LPVOID lpParam);
static LRESULT APIENTRY RedirectedWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static void UpdateDpi(HWND hwnd);
// NOTE: extern "C" is important to disable name-mangling, so that the functions can be found with GetProcAddress(), which is used in WhatsappTray.cpp
extern "C" DLLIMPORT LRESULT CALLBACK CallWndRetProc(int nCode, WPARAM wParam, LPARAM lParam);
static HWND GetTopLevelWindowhandleWithName(std::string searchedWindowTitle);
static std::string GetWindowTitle(HWND hwnd);
static std::string GetFilepathFromProcessID(DWORD processId);
static std::string WideToUtf8(const std::wstring& inputString);
static std::string GetEnviromentVariable(const std::string& inputString);
static POINT LParamToPoint(LPARAM lParam);
static bool SendMessageToWhatsappTray(UINT message, WPARAM wParam = 0, LPARAM lParam = 0);
static bool BlockShowWindowFunction();
static bool UnblockShowWindowFunction();

static void StartInitThread();
void OnWhatsAppFullyInitialized();

bool SaveHIconToFile(HICON hIcon, std::string fileName);
bool SaveHBITMAPToFile(HBITMAP hBitmap, LPCTSTR lpszFileName);
std::string GetWhatsappTrayPath();
intptr_t* GetITaskbarList3Vtable();
intptr_t GetSetOverlayIconMemoryAddressFromVtable(intptr_t* iTaskbarList3_vtableAddress);
bool WriteJumpToFunctionInVtable(intptr_t* iTaskbarList3_vtableAddress, intptr_t Rerouted_SetOverlayIcon_Address);
HRESULT Rerouted_SetOverlayIcon();

/**
 * @brief The entry point for the dll
 *
 * This is called when the hook is attached to the thread
*/
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH: {

		StartInitThread();

		break;
	}
	case DLL_PROCESS_DETACH: {
		LogString("Detach hook.dll from ProcessID: 0x%08X", _processID);

		// Remove our window-proc from the chain by setting the original window-proc.
		if (_originalWndProc != NULL) {
			SetWindowLongPtr(_whatsAppWindowHandle, GWLP_WNDPROC, (LONG_PTR)_originalWndProc);
		}

		// NOTE: For some reason this works here. 
		// Because according to this:https://docs.microsoft.com/en-ca/windows/win32/dlls/dynamic-link-library-best-practices?redirectedfrom=MSDN
		// All threads should be terminated already?
		// Anyway without stopping a messagebox with an error will appear.
		SocketStop();

		// Cleanup COM-lib usage (for _pTaskbarList)
		CoUninitialize();

	} break;
	}

	return true;
}

DWORD WINAPI Init(LPVOID lpParam)
{
	//OutputDebugStringA("Hook-init-thread is started");

	SocketStart(LOGGER_IP, LOGGER_PORT);

	// Find the WhatsApp window-handle that we need to replace the window-proc
	_processID = GetCurrentProcessId();

	LogString("Attached hook.dll to ProcessID: 0x%08X", _processID);

	auto filepath = GetFilepathFromProcessID(_processID);

	/* LoadLibrary() triggers DllMain with DLL_PROCESS_ATTACH. This is used in WhatsappTray.cpp
	 * to prevent tirggering the wndProc redirect for WhatsappTray we need to detect if this happend.
	 * the easiest/best way to detect that is by setting a enviroment variable before LoadLibrary() */
	auto envValue = GetEnviromentVariable(WHATSAPPTRAY_LOAD_LIBRARY_TEST_ENV_VAR);

	LogString("Filepath: '%s' " WHATSAPPTRAY_LOAD_LIBRARY_TEST_ENV_VAR ": '%s'", filepath.c_str(), envValue.c_str());

	if (envValue.compare(WHATSAPPTRAY_LOAD_LIBRARY_TEST_ENV_VAR_VALUE) == 0) {
		LogString("Detected that this Attache was triggered by LoadLibrary() => Cancel further processing");

		// It is best to remove the variable here so we can be sure it is not removed before it was detected.
		// Delete enviroment-variable by setting it to "".
		_putenv_s(WHATSAPPTRAY_LOAD_LIBRARY_TEST_ENV_VAR, "");
		return 1;
	}

	_whatsAppWindowHandle = GetTopLevelWindowhandleWithName(WHATSAPP_CLIENT_NAME);
	auto windowTitle = GetWindowTitle(_whatsAppWindowHandle);

	LogString("Attached in window '%s' _whatsAppWindowHandle: 0x%08X", windowTitle.c_str(), _whatsAppWindowHandle);

	if (_whatsAppWindowHandle == NULL) {
		LogString("Error, window-handle for '" WHATSAPP_CLIENT_NAME "' was not found");
		return 2;
	}

	// Update the windows scaling for the monitor that whatsapp is currently on
	UpdateDpi(_whatsAppWindowHandle);

	// Replace the original window-proc with our own. This is called subclassing.
	// Our window-proc will call after the processing the original window-proc.
	_originalWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(_whatsAppWindowHandle, GWLP_WNDPROC, (LONG_PTR)RedirectedWndProc));

	if (BlockShowWindowFunction() == true) {
		// Notify WhatsAppTray that ShowWindow-function is blocked and the minmizing can be done if needed.
		SendMessageToWhatsappTray(WM_WHATSAPP_SHOWWINDOW_BLOCKED, 0, 0);
	}

	_whatsappTrayPath = GetWhatsappTrayPath();
	if (_whatsappTrayPath.length() == 0) {
		LogString("GetWhatsappTrayPath FAILED");
		return 3;
	}
	LogString("_whatsappTrayPath=%s", _whatsappTrayPath.c_str());

	// === Initialize iTaskbarList3-stuff to reroute SetOverlayIcon()-function ===
	HRESULT hrInit = CoInitialize(NULL);
	if (FAILED(hrInit)) {
		LogString("CoInitialize FAILED");
		return 4;
	}

	auto iTaskbarList3Vtable = GetITaskbarList3Vtable();
	_setOverlayIconMemoryAddress = GetSetOverlayIconMemoryAddressFromVtable(iTaskbarList3Vtable);

	auto Rerouted_SetOverlayIcon_Addr = (intptr_t)Rerouted_SetOverlayIcon;
	LogString("Rerouted_SetOverlayIcon-address=%llX.", Rerouted_SetOverlayIcon);

	if (WriteJumpToFunctionInVtable(iTaskbarList3Vtable, Rerouted_SetOverlayIcon_Addr) == false) {
		LogString("WriteJumpToFunctionInVtable FAILED");
		return 5;
	}

	// This is a good point to see if WhatsApp is fully initialized, because it sets the overlay-icon very late in the init-process.
	std::async(&OnWhatsAppFullyInitialized);

	return 0;
}

/**
 * @brief WhatsApp is fully initialized
 */
void OnWhatsAppFullyInitialized()
{
	LogString("WhatsAppFullyInitialized");

	std::this_thread::sleep_for(std::chrono::seconds(5));

	// It is no longer necessary to block the ShowWindow()-function, so unblock it again...
	UnblockShowWindowFunction();
}

/**
 * @brief This is the new main window-proc. After we are done we call the original window-proc.
 *
 * This method has the advantage over SetWindowsHookEx(WH_CALLWNDPROC... that here we can skip or modifie messages.
 */
static LRESULT APIENTRY RedirectedWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static UINT s_uTBBC = WM_NULL;

	if (s_uTBBC == WM_NULL) {
		LogString("RegisterWindowMessage");

		// In case the application is run elevated, allow the
		// TaskbarButtonCreated message through.
		ChangeWindowMessageFilter(s_uTBBC, MSGFLT_ADD);

		// Compute the value for the TaskbarButtonCreated message
		s_uTBBC = RegisterWindowMessage("TaskbarButtonCreated");

		// Normally you should wait until you get the message s_uTBBC, but that does not always work in this hook-dll.
		// Maybe because Whatsapp already registerd, and the message was already received and Windows does not notify again
		// So we just run it always on startup
		// Look at this how it should normaly work: https://github.com/microsoft/Windows-classic-samples/tree/main/Samples/Win7Samples/winui/shell/appshellintegration/TaskbarPeripheralStatus
		if (!_pTaskbarList) {
			HRESULT hr = CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&_pTaskbarList));
			if (SUCCEEDED(hr)) {
				LogString("CoCreateInstance SUCCEEDED %llX", _pTaskbarList);

				hr = _pTaskbarList->HrInit();
				if (FAILED(hr)) {
					LogString("CoCreateInstance FAILED");

					_pTaskbarList->Release();
					_pTaskbarList = NULL;
				}
			}
		}

	}

	std::ostringstream traceBuffer;

#ifdef _DEBUG
	if (uMsg != WM_GETTEXT) {
		// Dont print WM_GETTEXT so there is not so much "spam"

		traceBuffer << MODULE_NAME << "::" << __func__ << ": " << WindowsMessage::GetString(uMsg) << "(0x" << std::uppercase << std::hex << uMsg << ") ";
		traceBuffer << "windowTitle='" << GetWindowTitle(hwnd) << "' ";
		traceBuffer << "hwnd=0x'" << std::uppercase << std::hex << hwnd << "' ";
		traceBuffer << "wParam=0x'" << std::uppercase << std::hex << wParam << "' ";
		WinSockLogger::TraceStream(traceBuffer);
	}
#endif

	if (uMsg == WM_SYSCOMMAND) {
		// Description for WM_SYSCOMMAND: https://msdn.microsoft.com/de-de/library/windows/desktop/ms646360(v=vs.85).aspx
		if (wParam == SC_MINIMIZE) {
			LogString("SC_MINIMIZE received");

			// Here i check if the windowtitle matches. Vorher hatte ich das Problem das sich Chrome auch minimiert hat.
			if (hwnd == _whatsAppWindowHandle) {
				SendMessageToWhatsappTray(WM_WA_MINIMIZE_BUTTON_PRESSED);
			}
		}
	} else if (uMsg == WM_NCDESTROY) {
		LogString("WM_NCDESTROY received");

		if (hwnd == _whatsAppWindowHandle) {
			auto successfulSent = SendMessageToWhatsappTray(WM_WHAHTSAPP_CLOSING);
			if (successfulSent) {
				LogString("WM_WHAHTSAPP_CLOSING successful sent.");
			}
		}
	} else if (uMsg == WM_CLOSE) {
		// This happens when alt + f4 is pressed.
		LogString("WM_CLOSE received. Probably Alt + F4");

		// Notify WhatsappTray and if it wants to close it can do so...
		SendMessageToWhatsappTray(WM_WHATSAPP_TO_WHATSAPPTRAY_RECEIVED_WM_CLOSE);

		LogString("WM_CLOSE blocked.");

		// Block WM_CLOSE
		return 0;
	} else if (uMsg == WM_WHATSAPPTRAY_TO_WHATSAPP_SEND_WM_CLOSE) {
		// This message is defined by me and should only come from WhatsappTray.
		// It more or less replaces WM_CLOSE which is now always blocked...
		// To have a way to still send WM_CLOSE this message was made.
		LogString("WM_WHATSAPPTRAY_TO_WHATSAPP_SEND_WM_CLOSE received");

		LogString("Send WM_CLOSE to WhatsApp.");
		// NOTE: lParam/wParam are not used in WM_CLOSE.
		return CallWindowProc(_originalWndProc, hwnd, WM_CLOSE, 0, 0);
	} else if (uMsg == WM_DPICHANGED) {
		LogString("WM_DPICHANGED received");

		LogString("Updating the Dpi");
		UpdateDpi(_whatsAppWindowHandle);
	} else if (uMsg == WM_LBUTTONUP) {
		// Unblock ShowWindow()-function if a mouseclick is registered.
		UnblockShowWindowFunction();

		// Note x and y are clientare-coordiantes
		auto clickPoint = LParamToPoint(lParam);
		LogString("WM_LBUTTONUP received x=%d y=%d", clickPoint.x, clickPoint.y);

		RECT rect;
		GetClientRect(hwnd, &rect);

		constexpr int defaultDpi = 96;
		int widthOfButton = 46; /* dpi 96 (100%) window not maximized */
		int heightOfButton = 34; /* dpi 96 (100%) window not maximized */

		// I use percent because it is a fraction like 1,25. 100 to get value in percent
		int dpiRatioPercentX = (100 * _dpiX) / defaultDpi;
		// NOTE: The width is little to big but it is better to wrongly send to tray instead of maximize then close instead of send to tray
		widthOfButton = (widthOfButton * dpiRatioPercentX) / 100;
		int dpiRatioPercentY = (100 * _dpiY) / defaultDpi;
		heightOfButton = (heightOfButton * dpiRatioPercentY) / 100;

		// calculate x-distance fom right window border
		int windowWidth = rect.right - rect.left;
		int xDistanceFromRight = windowWidth - clickPoint.x;
		LogString("WM_LBUTTONUP => windowWidth=%d xDistanceFromRight=%d widthOfButton=%d", windowWidth, xDistanceFromRight, widthOfButton);

		if (xDistanceFromRight <= widthOfButton && clickPoint.y <= heightOfButton) {
			SendMessageToWhatsappTray(WM_WA_CLOSE_BUTTON_PRESSED);

			LogString("Block WM_LBUTTONUP");
			return 0;
		}
	} else if (uMsg == WM_RBUTTONUP) {
		// Unblock ShowWindow()-function if a mouseclick is registered.
		UnblockShowWindowFunction();
	} else if (uMsg == WM_KEYUP) {
		LogString("WM_KEYUP received key=%d", wParam);

		SendMessageToWhatsappTray(WM_WA_KEY_PRESSED, wParam, lParam);
	}

	// Call the original window-proc.
	return CallWindowProc(_originalWndProc, hwnd, uMsg, wParam, lParam);
}

/**
 * @brief Update the windows scaling for the monitor that the window is currently on
*/
static void UpdateDpi(HWND hwnd)
{
	// Windows 7 does not have screenscaling or atleast with Shcore, so if we can not find the dll, just skip the screenscaling logic.
	static HMODULE shcoreLib = NULL;

	if (shcoreLib == NULL) {
		shcoreLib = LoadLibrary("Shcore.dll");
		if (shcoreLib == NULL) {
			LogString("Could not load Shcore.dll");
			return;
		}
	}

	auto GetDpiForMonitor = reinterpret_cast<GetDpiForMonitorFunc>(GetProcAddress(shcoreLib, "GetDpiForMonitor"));
	if (CallWndRetProc == NULL) {
		LogString("The function 'GetDpiForMonitor' was not found");
		return;
	}

	// Im not cleaning this up but i thik i don't care... But leave it here as a reminder
	//FreeLibrary(shcoreLib);

	auto monitorHandle = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);

	auto result = GetDpiForMonitor(monitorHandle, MDT_DEFAULT, &_dpiX, &_dpiY);

	if (result != S_OK) {
		LogString("Error when getting the dpi for WhatsApp");
		// Continue even with the error...
	} else {
		LogString("The dpi for WhatsApp is dpiX: %d dpiY: %d", _dpiX, _dpiY);
	}

	return;
}

/**
 * THIS IS CURRENTLY ONLY A DUMMY. BUT THE SetWindowsHookEx() IS STILL USED TO INJECT THE DLL INTO THE TARGET (WhatsApp)
 * NOTE: This is better then the mousehook because it will inject the dll imediatly after SetWindowsHookEx()
 *       The Mousehook waits until it receives a mouse-message and then attaches the dll. This means the cursor has to be move on top of WhatsApp
 *
 * Only works for 32-bit apps or 64-bit apps depending on whether this is complied as 32-bit or 64-bit (Whatsapp is a 64Bit-App)
 * NOTE: This only caputers messages sent by SendMessage() and NOT PostMessage()!
 * NOTE: This function also receives messages from child-windows.
 * - This means we have to be carefull not accidently mix them.
 * - For example when watching for WM_NCDESTROY, it is possible that a childwindow does that but the mainwindow remains open.
 * @param nCode
 * @param wParam
 * @param lParam Contains a CWPSTRUCT*-struct in which the data can be found.
 */
LRESULT CALLBACK CallWndRetProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	return CallNextHookEx(NULL, nCode, wParam, lParam);
}

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
static HWND GetTopLevelWindowhandleWithName(std::string searchedWindowTitle)
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
static std::string GetWindowTitle(HWND hwnd)
{
	char windowNameBuffer[2000];
	GetWindowTextA(hwnd, windowNameBuffer, sizeof(windowNameBuffer));

	return std::string(windowNameBuffer);
}

/**
 * @brief Get the path to the executable for the ProcessID
 *
 * @param processId The ProcessID from which the path to the executable should be fetched
 * @return The path to the executable from the ProcessID
*/
static std::string GetFilepathFromProcessID(DWORD processId)
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

static std::string WideToUtf8(const std::wstring& inputString)
{
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, inputString.c_str(), (int)inputString.size(), NULL, 0, NULL, NULL);
	std::string strTo(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, inputString.c_str(), (int)inputString.size(), &strTo[0], size_needed, NULL, NULL);
	return strTo;
}

static std::string GetEnviromentVariable(const std::string& inputString)
{
	size_t requiredSize;
	const size_t varbufferSize = 2000;
	char varbuffer[varbufferSize];

	getenv_s(&requiredSize, varbuffer, varbufferSize, inputString.c_str());

	return varbuffer;
}

static POINT LParamToPoint(LPARAM lParam)
{
	POINT p = { LOWORD(lParam), HIWORD(lParam) };

	return p;
}

/**
 * @brief Send message to WhatsappTray.
 */
static bool SendMessageToWhatsappTray(UINT message, WPARAM wParam, LPARAM lParam)
{
	return PostMessage(FindWindow(NAME, NAME), message, wParam, lParam);
}

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
static bool BlockShowWindowFunction()
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
static bool UnblockShowWindowFunction()
{
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

/**
 * @brief Starts the hook-init in an seperate thread
 *
 * Because it is better to not use DllMain for more complex initialisaition, a seperate thread will be created in this function.
 * NOTE: I had problems with '_processMessagesThread = std::thread(ProcessMessageQueue);' in WinSockClient, which would not run because of some limitations of DllMain
 * IMPORTANT: Don't wait for the thread to finish! This should not be done in DllMain...
*/
void StartInitThread()
{
	DWORD threadId;
	HANDLE threadHandle = CreateThread(
		NULL,         // default security attributes
		0,            // use default stack size  
		Init,         // thread function name
		NULL,         // argument to thread function 
		0,            // use default creation flags 
		&threadId);   // returns the thread identifier 

	// Check the return value for success.
	if (threadHandle == NULL) {
		MessageBox(NULL, "Hook.dll::StartInitThread: Thread could not be created.", "WhatsappTray (Hook.dll)", MB_OK);
	}

	// Close thread handle. NOTE(SAM): For now i do not clean up the thread handle because it shouldn't be such a big deal...
	//CloseHandle(threadHandle);
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

std::string GetWhatsappTrayPath()
{
	std::string whatsappTrayPath = "";

	auto waTrayHwnd = FindWindow(NAME, NAME);

	DWORD waTrayProcessId;
	auto waTrayThreadId = GetWindowThreadProcessId(waTrayHwnd, &waTrayProcessId);

	auto waTrayProcessHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, waTrayProcessId);
	if (waTrayProcessHandle == NULL) {
		LogString("Failed to open process.");
		return "";
	}

	char filename[MAX_PATH];
	if (GetModuleFileNameEx(waTrayProcessHandle, NULL, filename, MAX_PATH) == 0) {
		LogString("Failed to get module filename.");
		return "";
	}

	CloseHandle(waTrayProcessHandle);

	//LogString("Module filename is: %s", filename);
	whatsappTrayPath = filename;

	// Remove the exe-filename so we get the folder
	whatsappTrayPath = whatsappTrayPath.substr(0, whatsappTrayPath.find_last_of('\\'));

	return whatsappTrayPath;
}

intptr_t* GetITaskbarList3Vtable()
{
	HRESULT hr = CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&_pTaskbarList));
	if (FAILED(hr)) {
		LogString("CoCreateInstance FAILED");
		return NULL;
	}

	//LogString("CoCreateInstance SUCCEEDED %llX", _pTaskbarList);

	// NOTE: For normal usage of this api, more initialization would be necessarie
	//       but we only need the pointer to the _pTaskbarList to get to the vtable...
	//       Normal usage can be seen here: https://github.com/microsoft/Windows-classic-samples/tree/main/Samples/Win7Samples/winui/shell/appshellintegration/TaskbarPeripheralStatus

	auto iTaskbarList3_address = (intptr_t*)_pTaskbarList;
	LogString("ITaskbarList3-class-address=0x%llX", iTaskbarList3_address);

	auto iTaskbarList3_vtableAddress = (intptr_t*)iTaskbarList3_address[0];
	LogString("iTaskbarList3_vtableAddress=0x%llX", iTaskbarList3_vtableAddress);

	return iTaskbarList3_vtableAddress;
}

intptr_t GetSetOverlayIconMemoryAddressFromVtable(intptr_t* iTaskbarList3_vtableAddress)
{
	auto addressToSetOverlayIcon = iTaskbarList3_vtableAddress[18];
	LogString("SetOverlayIcon address=0x%llX", addressToSetOverlayIcon);

	return addressToSetOverlayIcon;
}

/**
 * @brief Write address of the Rerouted_SetOverlayIcon()-function to vtable of ITaskbarList3
*/
bool WriteJumpToFunctionInVtable(intptr_t* iTaskbarList3_vtableAddress, intptr_t Rerouted_SetOverlayIcon_Address)
{
	auto iTaskbarList3_vtableAddress_To_SetOverlayIcon = iTaskbarList3_vtableAddress + 18;

	LogString("iTaskbarList3_vtableAddress_To_SetOverlayIcon=0x%llX", iTaskbarList3_vtableAddress_To_SetOverlayIcon);

	// Change the protection-level of this memory-region, because it normaly has read, write, execute
	// NOTE: If this is not done WhatsApp will crash!
	DWORD oldProtect;
	if (VirtualProtect((PVOID)iTaskbarList3_vtableAddress_To_SetOverlayIcon, 8, PAGE_EXECUTE_READWRITE, &oldProtect) == NULL) {
		LogString("Failed to change protection-level of memorysection for iTaskbarList3 v-table");
		return false;
	}

	// Write address of Rerouted_SetOverlayIcon()-function to v-table
	*(iTaskbarList3_vtableAddress_To_SetOverlayIcon) = Rerouted_SetOverlayIcon_Address;

	return true;
}

HRESULT Rerouted_SetOverlayIcon()
{
	// WARNING: The registers values have to be read before any other code is executed!
	//          Otherwise it is likely that the values are overwritten
	auto rdxValue = ReturnRdx();
	auto r8Value = ReturnR8();

	// Get hicon-parameter
	auto hIcon = (HICON)r8Value;
	LogString("SetOverlayIcon() hicon-parameter=0x%llX (from r8-register)", hIcon);

	if (hIcon != NULL) {
		LogString("New message(s)");

		// Create new bitmaps for every function-call so to avoid errors due to race conditions
		// Because the read from WhatsappTray is not synchronized with the writing from the hook.
		auto bitmap_path = _whatsappTrayPath + "\\unread_messages_" + std::to_string(_iconCounter) + ".bmp";
		SaveHIconToFile(hIcon, bitmap_path.c_str());

		// Notify WhatsappTray that a new bitmap(icon) is ready
		SendMessageToWhatsappTray(WM_WHATSAPP_API_NEW_MESSAGE, _iconCounter, NULL);
		_iconCounter++;
	} else {
		LogString("No new messages");

		SendMessageToWhatsappTray(WM_WHATSAPP_API_NEW_MESSAGE, 0, NULL);
	}

	// Get hwnd-parameter
	// NOTE: This should be the hwnd of the WhatsApp-Window
	auto hwnd = (HWND)rdxValue;
	LogString("SetOverlayIcon() hwnd-parameter=0x%llX (from rdx-register)", hwnd);

	// Call the original function to get icon-overlay also in the taskbar
	// NOTE: When the WhatsApp-window is removed from the taskbar (by minimize to tray), the overlay-icon will be removed.
	//       This has nothing to do with this hack here.
	LogString("_setOverlayIconMemoryAddress=0x%llX", _setOverlayIconMemoryAddress);
	auto result = ((HRESULT(*)(ITaskbarList3*, HWND, HICON, LPCWSTR))_setOverlayIconMemoryAddress)(_pTaskbarList, hwnd, hIcon, NULL);

	return result;
}
