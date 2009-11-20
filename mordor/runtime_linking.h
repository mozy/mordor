#ifndef __MORDOR_RUNTIME_LINKING_H__
#define __MORDOR_RUNTIME_LINKING_H__
// Copyright (c) 2009 - Decho Corp.

#include <dbghelp.h>

#define MORDOR_RUNTIME_LINK_DECLARATION(Name, ReturnType, CallingConvention, ArgTypes) \
    typedef ReturnType (CallingConvention * LPFN_##Name) ArgTypes;              \
    extern LPFN_##Name p##Name;

#define MORDOR_RUNTIME_LINK_DEFINITION(Name, ReturnType, CallingConvention, ArgTypes, Args, Library)\
    static ReturnType CallingConvention Name##NotImpl ArgTypes;                 \
    static ReturnType CallingConvention Name##Thunk ArgTypes                    \
    {                                                                           \
        HINSTANCE hInstance = GetModuleHandleW(Library);                        \
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
MORDOR_RUNTIME_LINK_DECLARATION(FindFirstStreamW, HANDLE, WINAPI, (LPCWSTR, STREAM_INFO_LEVELS, LPVOID, DWORD));
MORDOR_RUNTIME_LINK_DECLARATION(FindNextStreamW, BOOL, APIENTRY, (HANDLE, LPVOID));
MORDOR_RUNTIME_LINK_DECLARATION(FlsAlloc, DWORD, WINAPI, (PFLS_CALLBACK_FUNCTION));
MORDOR_RUNTIME_LINK_DECLARATION(FlsFree, BOOL, WINAPI, (DWORD));
MORDOR_RUNTIME_LINK_DECLARATION(FlsGetValue, PVOID, WINAPI, (DWORD));
MORDOR_RUNTIME_LINK_DECLARATION(FlsSetValue, BOOL, WINAPI, (DWORD, PVOID));
MORDOR_RUNTIME_LINK_DECLARATION(FreeAddrInfoW, VOID, WSAAPI, (PADDRINFOW));
MORDOR_RUNTIME_LINK_DECLARATION(GetAddrInfoW, INT, WSAAPI, (PCWSTR, PCWSTR, const ADDRINFOW *, PADDRINFOW *));
MORDOR_RUNTIME_LINK_DECLARATION(IsThreadAFiber, BOOL, WINAPI, (VOID));
MORDOR_RUNTIME_LINK_DECLARATION(RtlCaptureStackBackTrace, WORD, NTAPI, (DWORD, DWORD, PVOID *, PDWORD));
MORDOR_RUNTIME_LINK_DECLARATION(SymFromAddr, BOOL, __stdcall, (HANDLE, DWORD64, PDWORD64, PSYMBOL_INFO));
MORDOR_RUNTIME_LINK_DECLARATION(SymGetLineFromAddr64, BOOL, __stdcall, (HANDLE, DWORD64, PDWORD, PIMAGEHLP_LINE64));

}

#endif
