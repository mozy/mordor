#ifndef __MORDOR_PREDEF_H__
#define __MORDOR_PREDEF_H__

#include "version.h"

#ifdef WINDOWS
#define _WIN32_WINNT 0x0600
#define WIN32_LEAN_AND_MEAN
#define SECURITY_WIN32
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS

#define strtoll _strtoi64
#define strtoull _strtoui64
#define strnicmp _strnicmp
#define mkdir _mkdir
#define snprintf _snprintf

#include <ntstatus.h>
#define WIN32_NO_STATUS
#include <windows.h>
#include <ws2tcpip.h>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#ifdef ABSOLUTE
#undef ABSOLUTE
#endif
#ifdef RELATIVE
#undef RELATIVE
#endif
#ifdef ERROR
#undef ERROR
#endif

// Take things out of the preprocessor, and put into the global namespace
#ifdef DELETE
#undef DELETE
enum {
    DELETE = (0x00010000L)
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
