#ifndef __MORDOR_PREDEF_H__
# define __MORDOR_PREDEF_H__

# include "version.h"

# ifdef WINDOWS
// Get Vista+ APIs
#  ifndef _WIN32_WINNT
#    define _WIN32_WINNT 0x0600
#  endif
// Don't include tons of crap from windows.h
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
// Define this so security.h works
#  ifndef SECURITY_WIN32
#    define SECURITY_WIN32
#  endif
// Shut up, CRT
#  ifndef _CRT_SECURE_NO_WARNINGS
#    define _CRT_SECURE_NO_WARNINGS
#  endif
#  ifndef _CRT_NONSTDC_NO_WARNINGS
#    define _CRT_NONSTDC_NO_WARNINGS
#  endif
#  ifndef _SCL_SECURE_NO_WARNINGS
#    define _SCL_SECURE_NO_WARNINGS
#  endif

// Use more common names for functions
// (cross-platform 64-bit, strip the underscores)
#  define atoll _atoi64
#  define strtoll _strtoi64
#  define strtoull _strtoui64
#  define strnicmp _strnicmp
#  define mkdir _mkdir
#  define snprintf _snprintf

#  include <ntstatus.h>
#  define WIN32_NO_STATUS
#  include <windows.h>
#  include <ws2tcpip.h>

// Take things out of the preprocessor, and put into the global namespace
// From WinGDI.h: #define ERROR 0
#  ifdef ERROR
#   undef ERROR
enum {
    ERROR = 0
};
#  endif

// From WinNT.h: #define DELETE (0x00010000L)
#  ifdef DELETE
#   undef DELETE
enum {
    DELETE =  (0x00010000L)
};
#  endif

# else // WINDOWS
#  ifdef _GNU_SOURCE
#   include <sys/sysmacros.h>

#   ifdef major
#    undef major
#   endif
#   ifdef minor
#    undef minor
#   endif
#  endif

#  define stricmp strcasecmp
#  define strnicmp strncasecmp

# endif // WINDOWS


#endif
