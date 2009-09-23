#ifndef __RUNTIME_LINKING_H__
#define __RUNTIME_LINKING_H__
// Copyright (c) 2009 - Decho Corp.

#define RUNTIME_LINK_DECLARATION(Name, ReturnType, CallingConvention, ArgTypes) \
    typedef ReturnType (CallingConvention * LPFN_##Name) ArgTypes;              \
    extern LPFN_##Name p##Name;

#define RUNTIME_LINK_DEFINITION(Name, ReturnType, CallingConvention, ArgTypes, Args, Library)\
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

RUNTIME_LINK_DECLARATION(CancelIoEx, BOOL, WINAPI, (HANDLE, LPOVERLAPPED));
RUNTIME_LINK_DECLARATION(FindFirstStreamW, HANDLE, WINAPI, (LPCWSTR, STREAM_INFO_LEVELS, LPVOID, DWORD));
RUNTIME_LINK_DECLARATION(FindNextStreamW, BOOL, APIENTRY, (HANDLE, LPVOID));

#endif
