#ifndef __MORDOR_PREDEF_H__
#define __MORDOR_PREDEF_H__

#include "version.h"

#ifdef WINDOWS
// Get Win7+ APIs
#define _WIN32_WINNT 0x0601
// Don't include tons of crap from windows.h
#define WIN32_LEAN_AND_MEAN
// Define this so security.h works
#define SECURITY_WIN32
// Shut up, CRT
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS

// Use more common names for functions
// (cross-platform 64-bit, strip the underscores)
#define atoll _atoi64
#define strtoll _strtoi64
#define strtoull _strtoui64
#define strnicmp _strnicmp
#define mkdir _mkdir
#define snprintf _snprintf

#include <ntstatus.h>
#define WIN32_NO_STATUS
#include <windows.h>
// define the PSAPI_VERSION to 1 so that we can still run on old XP systems
// The specific function that was failing fo kalypso was GetModuleName
/* http://msdn.microsoft.com/en-us/library/windows/desktop/ms683196(v=vs.85).aspx */
#define PSAPI_VERSION (1)

// 'inet_addr' is deprecated but will break xp machines if we replace it.
//  Use inet_pton() or InetPton() instead or define _WINSOCK_DEPRECATED_NO_WARNINGS to disable deprecated API warnings
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <ws2tcpip.h>

// Take things out of the preprocessor, and put into the global namespace
// From WinGDI.h: #define ERROR 0
#ifdef ERROR
#undef ERROR
enum {
    ERROR = 0
};
#endif

// From WinNT.h: #define DELETE (0x00010000L)
#ifdef DELETE
#undef DELETE
enum {
    DELETE =  (0x00010000L)
};
#endif

#else
#ifdef LINUX
#include <sys/sysmacros.h>

#ifdef major
#undef major
#endif
#ifdef minor
#undef minor
#endif
#endif

#define stricmp strcasecmp
#define strnicmp strncasecmp

#endif


#endif
