// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/pch.h"

#include "runtime_linking.h"

#include "wspiapi.h"

#pragma comment(lib, "ws2_32")

namespace Mordor {

MORDOR_RUNTIME_LINK_DEFINITION(CancelIoEx, BOOL, WINAPI,
    (HANDLE hFile, LPOVERLAPPED lpOverlapped),
    (hFile, lpOverlapped),
    L"kernel32.dll")
{
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

MORDOR_RUNTIME_LINK_DEFINITION(ConvertFiberToThread, BOOL, WINAPI,
    (VOID),
    (),
    L"kernel32.dll")
{
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

MORDOR_RUNTIME_LINK_DEFINITION(CreateFiberEx, LPVOID, WINAPI,
    (SIZE_T dwStackCommitSize, SIZE_T dwStackReserveSize, DWORD dwFlags, LPFIBER_START_ROUTINE lpStartAddress, LPVOID lpParameter),
    (dwStackCommitSize, dwStackReserveSize, dwFlags, lpStartAddress, lpParameter),
    L"kernel32.dll")
{
    return CreateFiber(dwStackCommitSize, lpStartAddress, lpParameter);
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

MORDOR_RUNTIME_LINK_DEFINITION(FlsAlloc, DWORD, WINAPI,
    (PFLS_CALLBACK_FUNCTION LpCallback),
    (LpCallback),
    L"kernel32.dll")
{
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FLS_OUT_OF_INDEXES;
}

MORDOR_RUNTIME_LINK_DEFINITION(FlsFree, BOOL, WINAPI,
    (DWORD dwFlsIndex),
    (dwFlsIndex),
    L"kernel32.dll")
{
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

MORDOR_RUNTIME_LINK_DEFINITION(FlsGetValue, PVOID, WINAPI,
    (DWORD dwFlsIndex),
    (dwFlsIndex),
    L"kernel32.dll")
{
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return NULL;
}

MORDOR_RUNTIME_LINK_DEFINITION(FlsSetValue, BOOL, WINAPI,
    (DWORD dwFlsIndex, PVOID lpFlsData),
    (dwFlsIndex, lpFlsData),
    L"kernel32.dll")
{
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

MORDOR_RUNTIME_LINK_DEFINITION(FreeAddrInfoW, VOID, WSAAPI,
    (PADDRINFOW pAddrInfo),
    (pAddrInfo),
    L"ws2_32.dll")
{
    ADDRINFOW *nextw = pAddrInfo;
    while (nextw) {
        if (nextw->ai_canonname)
            LocalFree((HLOCAL)nextw->ai_canonname);
        if (nextw->ai_addr)
            LocalFree((HLOCAL)nextw->ai_addr);
        nextw = nextw->ai_next;
    }
    if (pAddrInfo)
        LocalFree((HLOCAL)pAddrInfo);
}

MORDOR_RUNTIME_LINK_DEFINITION(GetAddrInfoW, INT, WSAAPI,
    (PCWSTR pNodeName, PCWSTR pServiceName, const ADDRINFOW *pHints, PADDRINFOW *ppResult),
    (pNodeName, pServiceName, pHints, ppResult),
    L"ws2_32.dll")
{
    int result = 0;
    addrinfo hintsA;
    addrinfo *pResultA = NULL;
    size_t count = 0;
    ADDRINFOW *nextw;
    addrinfo *nexta;
    char *pNodeNameA = NULL, *pServiceNameA = NULL;

    if (!ppResult)
        return WSA_INVALID_PARAMETER;
    if (pHints)
        if (pHints->ai_addrlen != 0 ||
            pHints->ai_canonname != NULL ||
            pHints->ai_addr != NULL ||
            pHints->ai_next != NULL)
            return WSA_INVALID_PARAMETER;
    *ppResult = NULL;
    if (pNodeName) {
        int len = WideCharToMultiByte(CP_THREAD_ACP, 0, pNodeName, -1, NULL, 0, NULL, NULL);
        if (len <= 0) {
            result = WSANO_RECOVERY;
            goto out;
        }
        pNodeNameA = (char *)LocalAlloc(LMEM_FIXED, len);
        if (!pNodeNameA) {
            result = WSA_NOT_ENOUGH_MEMORY;
            goto out;
        }
        int newlen = WideCharToMultiByte(CP_THREAD_ACP, 0, pNodeName, -1, pNodeNameA, len, NULL, NULL);
        if (len != newlen) {
            result = WSANO_RECOVERY;
            goto out;
        }
    }
    if (pServiceName) {
        int len = WideCharToMultiByte(CP_THREAD_ACP, 0, pServiceName, -1, NULL, 0, NULL, NULL);
        if (len <= 0) {
            result = WSANO_RECOVERY;
            goto out;
        }
        pServiceNameA = (char *)LocalAlloc(LMEM_FIXED, len);
        if (!pServiceNameA) {
            result = WSA_NOT_ENOUGH_MEMORY;
            goto out;
        }
        int newlen = WideCharToMultiByte(CP_THREAD_ACP, 0, pServiceName, -1, pServiceNameA, len, NULL, NULL);
        if (len != newlen) {
            result = WSANO_RECOVERY;
            goto out;
        }
    }

    if (pHints) {
        pResultA = &hintsA;
        hintsA.ai_flags = pHints->ai_flags;
        hintsA.ai_family = pHints->ai_family;
        hintsA.ai_socktype = pHints->ai_socktype;
        hintsA.ai_protocol = pHints->ai_protocol;
        hintsA.ai_addrlen = 0;
        hintsA.ai_canonname = NULL;
        hintsA.ai_addr = NULL;
        hintsA.ai_next = NULL;
    }

    result = getaddrinfo(pNodeNameA, pServiceNameA, pResultA, &pResultA);
    if (result == 0) {
        count = 0;
        nexta = pResultA;
        while (nexta) {
            ++count;
            nexta = nexta->ai_next;
        }
        if (count) {
            *ppResult = (ADDRINFOW*)LocalAlloc(LMEM_FIXED, count * sizeof(ADDRINFOW));
            if (!*ppResult) {
                result = WSA_NOT_ENOUGH_MEMORY;
                goto out;
            }
            for (size_t i = 0; i < count; ++i) {
                (*ppResult)[i].ai_canonname = NULL;
                if (i + 1 == count)
                    (*ppResult)[i].ai_next = NULL;
                else
                    (*ppResult)[i].ai_next = &(*ppResult)[i + 1];
            }
            nextw = *ppResult;
            nexta = pResultA;
            while (nexta) {
                nextw->ai_flags = nexta->ai_flags;
                nextw->ai_family = nexta->ai_family;
                nextw->ai_socktype = nexta->ai_socktype;
                nextw->ai_protocol = nexta->ai_protocol;
                nextw->ai_addrlen = nexta->ai_addrlen;
                nextw->ai_addr = nexta->ai_addr;
                if (nexta->ai_addr) {
                    nextw->ai_addr = (sockaddr*)LocalAlloc(LMEM_FIXED, nextw->ai_addrlen);
                    if (!nextw->ai_addr) {
                        result = WSA_NOT_ENOUGH_MEMORY;
                        goto out;
                    }
                    memcpy(nextw->ai_addr, nexta->ai_addr, nextw->ai_addrlen);
                }
                if (nexta->ai_canonname) {
                    int len = MultiByteToWideChar(CP_THREAD_ACP, MB_PRECOMPOSED, nexta->ai_canonname, -1, NULL, 0);
                    if (len <= 0) {
                        result = WSANO_RECOVERY;
                        goto out;
                    }
                    nextw->ai_canonname = (PWSTR)LocalAlloc(LMEM_FIXED, len * sizeof(WCHAR));
                    if (!nexta->ai_canonname) {
                        result = WSA_NOT_ENOUGH_MEMORY;
                        goto out;
                    }
                    int newlen = MultiByteToWideChar(CP_THREAD_ACP, MB_PRECOMPOSED, nexta->ai_canonname, -1, nextw->ai_canonname, len);
                    if (len != newlen) {
                        result = WSANO_RECOVERY;
                        goto out;
                    }
                }
                nexta = nexta->ai_next;
                nextw = nextw->ai_next;
            }
        }
    }

out:
    if (result != 0) {
        nextw = *ppResult;
        while (nextw) {
            if (nextw->ai_canonname)
                LocalFree((HLOCAL)nextw->ai_canonname);
            if (nextw->ai_addr)
                LocalFree((HLOCAL)nextw->ai_addr);
            nextw = nextw->ai_next;
        }
        if (*ppResult) {
            LocalFree((HLOCAL)*ppResult);
            *ppResult = NULL;
        }
    }
    if (pNodeNameA)
        LocalFree((HLOCAL)pNodeNameA);
    if (pServiceNameA)
        LocalFree((HLOCAL)pServiceNameA);
    if (pResultA)
        freeaddrinfo(pResultA);
    return result;
}

MORDOR_RUNTIME_LINK_DEFINITION(GetQueuedCompletionStatusEx, BOOL, WINAPI,
    (HANDLE CompletionPort, LPOVERLAPPED_ENTRY lpCompletionPortEntries, ULONG ulCount, PULONG ulNumEntriesRemoved, DWORD dwMilliseconds, BOOL fAlertable),
    (CompletionPort, lpCompletionPortEntries, ulCount, ulNumEntriesRemoved, dwMilliseconds, fAlertable),
    L"kernel32.dll")
{
    if (ulNumEntriesRemoved)
        *ulNumEntriesRemoved = 0;
    if (fAlertable || ulCount == 0 || !ulNumEntriesRemoved || !lpCompletionPortEntries) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    BOOL bRet = GetQueuedCompletionStatus(CompletionPort,
        &lpCompletionPortEntries->dwNumberOfBytesTransferred,
        &lpCompletionPortEntries->lpCompletionKey,
        &lpCompletionPortEntries->lpOverlapped,
        dwMilliseconds);
    if (!bRet && !lpCompletionPortEntries->lpOverlapped)
        return FALSE;
    *ulNumEntriesRemoved = 1;
    return TRUE;
}

MORDOR_RUNTIME_LINK_DEFINITION(IsThreadAFiber, BOOL, WINAPI,
    (VOID),
    (),
    L"kernel32.dll")
{
    PVOID fiber = GetCurrentFiber();
    return (fiber == NULL || fiber == (PVOID)0x1e00) ? FALSE : TRUE;
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

MORDOR_RUNTIME_LINK_DEFINITION(RtlNtStatusToDosError, ULONG, NTAPI,
    (NTSTATUS Status),
    (Status),
    L"ntdll.dll")
{
    return ERROR_CALL_NOT_IMPLEMENTED;
}

MORDOR_RUNTIME_LINK_DEFINITION(SetFileCompletionNotificationModes, BOOL, WINAPI,
    (HANDLE FileHandle, UCHAR Flags),
    (FileHandle, Flags),
    L"kernel32.dll")
{
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

MORDOR_RUNTIME_LINK_DEFINITION(SymFromAddr, BOOL, __stdcall,
    (HANDLE hProcess, DWORD64 Address, PDWORD64 Displacement, PSYMBOL_INFO Symbol),
    (hProcess, Address, Displacement, Symbol),
    L"dbghelp.dll")
{
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

MORDOR_RUNTIME_LINK_DEFINITION(SymGetLineFromAddr64, BOOL, __stdcall,
    (HANDLE hProcess, DWORD64 qwAddr, PDWORD pdwDisplacement, PIMAGEHLP_LINE64 Line64),
    (hProcess, qwAddr, pdwDisplacement, Line64),
    L"dbghelp.dll")
{
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

MORDOR_RUNTIME_LINK_DEFINITION(WinHttpCloseHandle, BOOL, WINAPI,
    (HINTERNET hInternet),
    (hInternet),
    L"winhttp.dll")
{
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

MORDOR_RUNTIME_LINK_DEFINITION(WinHttpGetDefaultProxyConfiguration, BOOL, WINAPI,
    (WINHTTP_PROXY_INFO *pProxyInfo),
    (pProxyInfo),
    L"winhttp.dll")
{
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

MORDOR_RUNTIME_LINK_DEFINITION(WinHttpGetIEProxyConfigForCurrentUser, BOOL, WINAPI,
    (WINHTTP_CURRENT_USER_IE_PROXY_CONFIG *pProxyConfig),
    (pProxyConfig),
    L"winhttp.dll")
{
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

MORDOR_RUNTIME_LINK_DEFINITION(WinHttpGetProxyForUrl, BOOL, WINAPI,
    (HINTERNET hSession, LPCWSTR lpcwszUrl, WINHTTP_AUTOPROXY_OPTIONS *pAutoProxyOptions, WINHTTP_PROXY_INFO *pProxyInfo),
    (hSession, lpcwszUrl, pAutoProxyOptions, pProxyInfo),
    L"winhttp.dll")
{
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

MORDOR_RUNTIME_LINK_DEFINITION(WinHttpOpen, HINTERNET, WINAPI,
    (LPCWSTR pszAgentW, DWORD dwAccessType, LPCWSTR pszProxyW, LPCWSTR pszProxyBypassW, DWORD dwFlags),
    (pszAgentW, dwAccessType, pszProxyW, pszProxyBypassW, dwFlags),
    L"winhttp.dll")
{
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return NULL;
}

}
