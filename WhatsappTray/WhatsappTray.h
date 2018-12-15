
#include <objidl.h>
#include <gdiplus.h>
#pragma comment (lib,"Gdiplus.lib")

#define NAME TEXT("WhatsappTray")
#define WHATSAPP_CLIENT_NAME TEXT("WhatsApp")

#define WM_ADDTRAY  0x0401
#define WM_TRAYCMD  0x0404
#define WM_WHAHTSAPP_CLOSING  0x0405
#define WM_REAPPLY_HOOK  0x0406
#define WM_INDEXED_DB_CHANGED  0x0407
#define IDM_RESTORE 0x1001
#define IDM_CLOSE   0x1002
#define IDM_ABOUT   0x1004
#define IDM_SETTING_CLOSE_TO_TRAY   0x1005
#define IDM_SETTING_LAUNCH_ON_WINDOWS_STARTUP   0x1006

#define DLLIMPORT __declspec(dllexport)

BOOL DLLIMPORT RegisterHook(HMODULE hLib, DWORD threadId, bool closeToTray);
void DLLIMPORT UnRegisterHook();
