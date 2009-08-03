// Copyright (c) 2009 - Decho Corp.

#include "file.h"

#include "mordor/common/exception.h"

FileStream::FileStream(std::string filename, Flags flags, CreateFlags createFlags)
{
    NativeHandle handle;
#ifdef WINDOWS
    DWORD access = 0;
    if (flags & READ)
        access |= GENERIC_READ;
    if (flags & WRITE)
        access |= GENERIC_WRITE;
    // TODO: UTF-8
    handle = CreateFileA(filename.c_str(),
        access,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        createFlags,
        0,
        NULL);
#else
    int oflags = (int)flags;
    switch (createFlags) {
        case CREATE_NEW:
            oflags |= O_CREAT | O_EXCL;
            break;
        case CREATE_ALWAYS:
            oflags |= O_CREAT | O_TRUNC;
            break;
        case OPEN_EXISTING:
            break;
        case OPEN_ALWAYS:
            oflags |= O_CREAT;
            break;
        case TRUNCATE_EXISTING:
            oflags |= O_TRUNC;
            break;
        default:
            ASSERT(false);
    }
    handle = open(filename.c_str(), oflags, 0777);
#endif
    if (handle == (NativeHandle)-1) {
        throwExceptionFromLastError();
    }
    init(handle);
    m_supportsRead = flags == READ || flags == READWRITE;
    m_supportsWrite = flags == WRITE || flags == READWRITE;
}

#ifdef WINDOWS
FileStream::FileStream(std::wstring filename, Flags flags, CreateFlags createFlags)
{
    NativeHandle handle;
    DWORD access = 0;
    if (flags & READ)
        access |= GENERIC_READ;
    if (flags & WRITE)
        access |= GENERIC_WRITE;
    handle = CreateFileW(filename.c_str(),
        access,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        createFlags,
        0,
        NULL);
    if (handle == (NativeHandle)-1) {
        throwExceptionFromLastError();
    }
    init(handle);
    m_supportsRead = flags == READ || flags == READWRITE;
    m_supportsWrite = flags == WRITE || flags == READWRITE;
}
#endif
