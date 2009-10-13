// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include "file.h"

#include "mordor/exception.h"
#include "mordor/string.h"

namespace Mordor {

FileStream::FileStream(std::string filename, Flags flags, CreateFlags createFlags)
{
    NativeHandle handle;
#ifdef WINDOWS
    DWORD access = 0;
    if (flags & READ)
        access |= GENERIC_READ;
    if (flags & WRITE)
        access |= GENERIC_WRITE;
    if (flags == APPEND)
        access = FILE_APPEND_DATA | SYNCHRONIZE;
    handle = CreateFileW(toUtf16(filename).c_str(),
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
            MORDOR_ASSERT(false);
    }
    handle = open(filename.c_str(), oflags, 0777);
#endif
    if (handle == (NativeHandle)-1)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR();
    init(handle);
    m_supportsRead = flags == READ || flags == READWRITE;
    m_supportsWrite = flags == WRITE || flags == READWRITE || flags == APPEND;
    m_supportsSeek = flags != APPEND;
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
    if (flags == APPEND)
        access = FILE_APPEND_DATA | SYNCHRONIZE;
    handle = CreateFileW(filename.c_str(),
        access,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        createFlags,
        0,
        NULL);
    if (handle == (NativeHandle)-1)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateFileW");
    init(handle);
    m_supportsRead = flags == READ || flags == READWRITE;
    m_supportsWrite = flags == WRITE || flags == READWRITE || flags == APPEND;
    m_supportsSeek = flags != APPEND;
}
#endif

}
