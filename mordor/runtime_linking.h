#ifndef __MORDOR_RUNTIME_LINKING_H__
#define __MORDOR_RUNTIME_LINKING_H__
// Copyright (c) 2009 - Mozy, Inc.

#include <dbghelp.h>
#include <IPHlpApi.h>
#include <lm.h>
#include <winhttp.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define MORDOR_RUNTIME_LINK_DECLARATION(Name, ReturnType, CallingConvention, ArgTypes) \
    typedef ReturnType (CallingConvention * LPFN_##Name) ArgTypes;              \
    extern LPFN_##Name p##Name;

#define MORDOR_RUNTIME_LINK_DEFINITION(Name, ReturnType, CallingConvention, ArgTypes, Args, Library)\
    static ReturnType CallingConvention Name##NotImpl ArgTypes;                 \
    static ReturnType CallingConvention Name##Thunk ArgTypes                    \
    {                                                                           \
        HINSTANCE hInstance = LoadLibraryW(Library);                            \
        p##Name = (LPFN_##Name)GetProcAddress(hInstance, #Name);                \
        if (!p##Name)                                                           \
            p##Name = &Name##NotImpl;                                           \
        return p##Name Args;                                                    \
    }                                                                           \
    LPFN_##Name p##Name = &Name##Thunk;                                         \
    static ReturnType CallingConvention Name##NotImpl ArgTypes

namespace Mordor {

MORDOR_RUNTIME_LINK_DECLARATION(CancelIoEx, BOOL, WINAPI, (HANDLE, LPOVERLAPPED));
MORDOR_RUNTIME_LINK_DECLARATION(ConvertFiberToThread, BOOL, WINAPI, (VOID));
MORDOR_RUNTIME_LINK_DECLARATION(CreateFiberEx, LPVOID, WINAPI, (SIZE_T, SIZE_T, DWORD, LPFIBER_START_ROUTINE, LPVOID));
MORDOR_RUNTIME_LINK_DECLARATION(FindFirstStreamW, HANDLE, WINAPI, (LPCWSTR, STREAM_INFO_LEVELS, LPVOID, DWORD));
MORDOR_RUNTIME_LINK_DECLARATION(FindNextStreamW, BOOL, APIENTRY, (HANDLE, LPVOID));
MORDOR_RUNTIME_LINK_DECLARATION(FlsAlloc, DWORD, WINAPI, (PFLS_CALLBACK_FUNCTION));
MORDOR_RUNTIME_LINK_DECLARATION(FlsFree, BOOL, WINAPI, (DWORD));
MORDOR_RUNTIME_LINK_DECLARATION(FlsGetValue, PVOID, WINAPI, (DWORD));
MORDOR_RUNTIME_LINK_DECLARATION(FlsSetValue, BOOL, WINAPI, (DWORD, PVOID));
MORDOR_RUNTIME_LINK_DECLARATION(FreeAddrInfoW, VOID, WSAAPI, (PADDRINFOW));
MORDOR_RUNTIME_LINK_DECLARATION(GetAdaptersAddresses, ULONG, WINAPI, (ULONG, ULONG, PVOID, PIP_ADAPTER_ADDRESSES, PULONG));
MORDOR_RUNTIME_LINK_DECLARATION(GetAddrInfoW, INT, WSAAPI, (PCWSTR, PCWSTR, const ADDRINFOW *, PADDRINFOW *));
MORDOR_RUNTIME_LINK_DECLARATION(GetQueuedCompletionStatusEx, BOOL, WINAPI, (HANDLE, LPOVERLAPPED_ENTRY, ULONG, PULONG, DWORD, BOOL));
MORDOR_RUNTIME_LINK_DECLARATION(IsThreadAFiber, BOOL, WINAPI, (VOID));
MORDOR_RUNTIME_LINK_DECLARATION(RtlCaptureStackBackTrace, WORD, NTAPI, (DWORD, DWORD, PVOID *, PDWORD));
MORDOR_RUNTIME_LINK_DECLARATION(RtlNtStatusToDosError, ULONG, NTAPI, (NTSTATUS));
MORDOR_RUNTIME_LINK_DECLARATION(SetFileCompletionNotificationModes, BOOL, WINAPI, (HANDLE, UCHAR));
MORDOR_RUNTIME_LINK_DECLARATION(SymFromAddr, BOOL, __stdcall, (HANDLE, DWORD64, PDWORD64, PSYMBOL_INFO));
MORDOR_RUNTIME_LINK_DECLARATION(SymGetLineFromAddr64, BOOL, __stdcall, (HANDLE, DWORD64, PDWORD, PIMAGEHLP_LINE64));
MORDOR_RUNTIME_LINK_DECLARATION(WinHttpCloseHandle, BOOL, WINAPI, (HINTERNET));
MORDOR_RUNTIME_LINK_DECLARATION(WinHttpGetDefaultProxyConfiguration, BOOL, WINAPI, (WINHTTP_PROXY_INFO *));
MORDOR_RUNTIME_LINK_DECLARATION(WinHttpGetIEProxyConfigForCurrentUser, BOOL, WINAPI, (WINHTTP_CURRENT_USER_IE_PROXY_CONFIG *));
MORDOR_RUNTIME_LINK_DECLARATION(WinHttpGetProxyForUrl, BOOL, WINAPI, (HINTERNET, LPCWSTR, WINHTTP_AUTOPROXY_OPTIONS *, WINHTTP_PROXY_INFO *));
MORDOR_RUNTIME_LINK_DECLARATION(WinHttpOpen, HINTERNET, WINAPI, (LPCWSTR, DWORD, LPCWSTR, LPCWSTR, DWORD));

}

#endif
