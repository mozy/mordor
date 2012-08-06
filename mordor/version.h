#ifndef __MORDOR_VERSION_H__
#define __MORDOR_VERSION_H__

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

#if defined(linux) || defined(__linux__)
#   define LINUX
#endif

#ifdef __APPLE__
#   define OSX
#   ifndef BSD
#       define BSD
#   endif
#endif

#ifdef __FreeBSD__
#   define FREEBSD
#   define BSD
#endif

#ifdef WINDOWS
#define MORDOR_NATIVE(win32, posix) win32
#else
#define MORDOR_NATIVE(win32, posix) posix
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
#   elif defined(__ppc__)
#       define PPC
#   elif defined(__arm__)
#       define ARM
#   endif
#endif

#ifdef MSVC
#   ifndef _DEBUG
#       ifndef NDEBUG
#           define NDEBUG
#       endif
#   endif
#endif

#endif
