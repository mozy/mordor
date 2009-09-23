// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include "runtime_linking.h"

RUNTIME_LINK_DEFINITION(CancelIoEx, BOOL, WINAPI,
    (HANDLE hFile, LPOVERLAPPED lpOverlapped),
    (hFile, lpOverlapped),
    L"kernel32.dll")
{
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

RUNTIME_LINK_DEFINITION(FindFirstStreamW, HANDLE, WINAPI,
    (LPCWSTR lpFileName, STREAM_INFO_LEVELS InfoLevel, LPVOID lpFindStreamData, DWORD dwFlags),
    (lpFileName, InfoLevel, lpFindStreamData, dwFlags),
    L"kernel32.dll")
{
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return INVALID_HANDLE_VALUE;
}

RUNTIME_LINK_DEFINITION(FindNextStreamW, BOOL, APIENTRY,
    (HANDLE hFindStream, LPVOID lpFindStreamData),
    (hFindStream, lpFindStreamData),
    L"kernel32.dll")
{
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}
