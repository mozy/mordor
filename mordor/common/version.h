#ifndef __VERSION_H__
#define __VERSION_H__

// OS
#ifdef _WIN32
#   define WINDOWS
#else
#   define POSIX
#endif
#ifdef __CYGWIN__
#   define WINDOWS
#   define CYGWIN
#endif

#ifdef linux
#   define LINUX
#endif

#ifdef __APPLE__
#   define OSX
#   define BSD
#endif

#ifdef __FreeBSD__
#   define FREEBSD
#   define BSD
#endif

// Architecture
#ifdef _MSC_VER
#   define MSVC
#   ifdef _M_X64
#       define X86_64
#   elif defined(_M_IX86)
#       define X86
#   endif
#endif

#ifdef __GNUC__
#   define GCC
#   ifdef __x86_64
#       define X86_64
#   elif defined(i386)
#       define X86
#   endif
#endif

#ifdef _DEBUG
#define DEBUG
#endif

#ifdef MSVC
#define USE_PCH
#endif

#endif
