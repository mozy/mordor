#ifndef __MORDOR_RUNTIME_LINKING_H__
#define __MORDOR_RUNTIME_LINKING_H__
// Copyright (c) 2009 - Decho Corp.

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
MORDOR_RUNTIME_LINK_DECLARATION(FindFirstStreamW, HANDLE, WINAPI, (LPCWSTR, STREAM_INFO_LEVELS, LPVOID, DWORD));
MORDOR_RUNTIME_LINK_DECLARATION(FindNextStreamW, BOOL, APIENTRY, (HANDLE, LPVOID));
MORDOR_RUNTIME_LINK_DECLARATION(RtlCaptureStackBackTrace, WORD, NTAPI, (DWORD, DWORD, PVOID *, PDWORD));

}

#endif
