#ifndef __MORDOR_PREDEF_H__
#define __MORDOR_PREDEF_H__

#include "version.h"

#ifdef WINDOWS
#define _WIN32_WINNT 0x0600
#define WIN32_LEAN_AND_MEAN
#define SECURITY_WIN32
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_WARNINGS

#define strtoull _strtoui64
#define strnicmp _strnicmp

#include <windows.h>

#undef min
#undef max
#undef ABSOLUTE
#undef RELATIVE
#undef ERROR

// Take things out of the preprocessor, and put into the global namespace
#undef DELETE
enum {
    DELETE = (0x00010000L)
};

#else

#ifdef major
#undef major
#undef minor
#endif

#define stricmp strcasecmp
#define strnicmp strncasecmp

#endif


#endif
