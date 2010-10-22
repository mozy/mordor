// Copyright (c) 2010 - Decho Corporation

#include "main.h"

#ifdef WINDOWS
namespace Mordor {

char **CommandLineToUtf8(int argc, wchar_t **argvW)
{
    DWORD totalLength = (argc + 1) * sizeof(char *);
    DWORD wcFlags = WC_ERR_INVALID_CHARS;
    for (int i = 0; i < argc; ++i) {
        int ret = WideCharToMultiByte(CP_UTF8, wcFlags, argvW[i], -1, NULL, 0, NULL, NULL);
        if (ret == 0 && GetLastError() == ERROR_INVALID_FLAGS) {
            wcFlags = 0;
            ret = WideCharToMultiByte(CP_UTF8, wcFlags, argvW[i], -1, NULL, 0, NULL, NULL);
        }
        if (ret == 0)
            return NULL;
        totalLength += ret;
    }
    char **result = (char **)LocalAlloc(LMEM_FIXED, totalLength);
    if (!result)
        return NULL;
    char *strings = (char *)result + (argc + 1) * sizeof(char *);
    totalLength -= (argc + 1) * sizeof(char *);
    for (int i = 0; i < argc; ++i) {
        result[i] = strings;
        int ret = WideCharToMultiByte(CP_UTF8, wcFlags, argvW[i], -1, strings,
            totalLength, NULL, NULL);
        if (ret == 0) {
            LocalFree(result);
            return NULL;
        }
        strings += ret;
        totalLength -= ret;
    }
    result[argc] = NULL;
    return result;
}

}
#endif
