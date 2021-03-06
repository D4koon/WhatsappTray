// stdafx.h: Includedatei für Standardsystem-Includedateien
// oder häufig verwendete projektspezifische Includedateien,
// die nur in unregelmäßigen Abständen geändert werden.
//

#pragma once

//#include "targetver.h"

// Define to be able to use a equivalent to #warning
// usage:
// #pragma WARNING(FIXME: Code removed because...)
#define STRINGIZE_HELPER(x) #x
#define STRINGIZE(x) STRINGIZE_HELPER(x)
#define WARNING(desc) message(__FILE__ "(" STRINGIZE(__LINE__) ") : warning: " #desc)


//#define WIN32_LEAN_AND_MEAN             // Selten verwendete Komponenten aus Windows-Headern ausschließen
// Windows-Headerdateien:
#include <windows.h>

// NOTE(SA): This was copied from the ReadDirectoryChanges-project. Don't know if needed.
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS      // some CString constructors will be explicit

