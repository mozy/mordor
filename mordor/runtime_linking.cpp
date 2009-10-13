// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include "runtime_linking.h"

namespace Mordor {

MORDOR_RUNTIME_LINK_DEFINITION(CancelIoEx, BOOL, WINAPI,
    (HANDLE hFile, LPOVERLAPPED lpOverlapped),
    (hFile, lpOverlapped),
    L"kernel32.dll")
{
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

MORDOR_RUNTIME_LINK_DEFINITION(RtlCaptureStackBackTrace, WORD, NTAPI,
                        (DWORD FramesToSkip, DWORD FramesToCapture, PVOID *BackTrace, PDWORD BackTraceHash),
                        (FramesToSkip, FramesToCapture, BackTrace, BackTraceHash),
                        L"kernel32.dll")
{
    // TODO: asm implementation for x86
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
}

MORDOR_RUNTIME_LINK_DEFINITION(FindFirstStreamW, HANDLE, WINAPI,
    (LPCWSTR lpFileName, STREAM_INFO_LEVELS InfoLevel, LPVOID lpFindStreamData, DWORD dwFlags),
    (lpFileName, InfoLevel, lpFindStreamData, dwFlags),
    L"kernel32.dll")
{
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return INVALID_HANDLE_VALUE;
}

MORDOR_RUNTIME_LINK_DEFINITION(FindNextStreamW, BOOL, APIENTRY,
    (HANDLE hFindStream, LPVOID lpFindStreamData),
    (hFindStream, lpFindStreamData),
    L"kernel32.dll")
{
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

}
