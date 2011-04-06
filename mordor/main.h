#ifndef __MORDOR_MAIN_H__
#define __MORDOR_MAIN_H__
// Copyright (c) 2010 - Mozy, Inc.

#include "version.h"

#ifdef WINDOWS

#include <ShellAPI.h>

namespace Mordor {
/// @return argv array in UTF-8.  Caller should free with LocalFree when it is
///         no longer needed
char **CommandLineToUtf8(int argc, wchar_t **argv);
}
/// Defines main in a cross-platform way, ensuring UTF-8 for argv
/// @example
/// MORDOR_MAIN(int argc, char **argv)
/// {
///     return 0;
/// }
#define MORDOR_MAIN(argctype, argvtype)                                         \
static int utf8main(argctype, argvtype);                                        \
extern "C" int main(int argc, char *argv[])                                     \
{                                                                               \
    LPWSTR lpCmdLineW = GetCommandLineW();                                      \
    int localArgc = 0;                                                          \
    LPWSTR *argvW = CommandLineToArgvW(lpCmdLineW, &localArgc);                 \
    if (!argvW)                                                                 \
        return GetLastError();                                                  \
    char **argvUtf8 = ::Mordor::CommandLineToUtf8(localArgc, argvW);            \
    LocalFree(argvW);                                                           \
    if (!argvUtf8)                                                              \
        return GetLastError();                                                  \
    int result = utf8main(localArgc, argvUtf8);                                 \
    LocalFree(argvUtf8);                                                        \
    return result;                                                              \
}                                                                               \
static int utf8main(argctype, argvtype)
#else
#define MORDOR_MAIN(argc, argv)                                                 \
int main(argc, argv)
#endif

#endif
