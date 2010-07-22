// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/pch.h"

#include "file.h"

#include "mordor/assert.h"
#include "mordor/string.h"

namespace Mordor {

static Logger::ptr g_log = Log::lookup("mordor:streams:file");

FileStream::FileStream()
: m_supportsRead(false),
  m_supportsWrite(false),
  m_supportsSeek(false)
{}

void
FileStream::init(const std::string &path, AccessFlags accessFlags,
    CreateFlags createFlags, IOManager *ioManager, Scheduler *scheduler)
{
    NativeHandle handle;
#ifdef WINDOWS
    DWORD access = 0;
    if (accessFlags & READ)
        access |= GENERIC_READ;
    if (accessFlags & WRITE)
        access |= GENERIC_WRITE;
    if (accessFlags == APPEND)
        access = FILE_APPEND_DATA | SYNCHRONIZE;
    DWORD flags = 0;
    if (createFlags & DELETE_ON_CLOSE) {
        flags |= FILE_FLAG_DELETE_ON_CLOSE;
        createFlags = (CreateFlags)(createFlags & ~DELETE_ON_CLOSE);
    }
    if (ioManager)
        flags |= FILE_FLAG_OVERLAPPED;
    MORDOR_ASSERT(createFlags >= CREATE_NEW && createFlags <= TRUNCATE_EXISTING);
    handle = CreateFileW(toUtf16(path).c_str(),
        access,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        createFlags,
        flags,
        NULL);
    DWORD error = GetLastError();
    MORDOR_LOG_VERBOSE(g_log) << "CreateFileW(" << path << ", " << access
        << ", " << createFlags << ", " << flags << "): " << handle << " ("
        << error << ")";
    if (handle == INVALID_HANDLE_VALUE)
        MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "CreateFileW");
#else
    int oflags = (int)accessFlags;
    switch (createFlags & ~DELETE_ON_CLOSE) {
        case OPEN:
            break;
        case CREATE:
            oflags |= O_CREAT | O_EXCL;
            break;
        case OPEN_OR_CREATE:
            oflags |= O_CREAT;
            break;
        case OVERWRITE:
            oflags |= O_TRUNC;
            break;
        case OVERWRITE_OR_CREATE:
            oflags |= O_CREAT | O_TRUNC;
            break;
        default:
            MORDOR_NOTREACHED();
    }
    handle = open(path.c_str(), oflags, 0777);
    int error = errno;
    MORDOR_LOG_VERBOSE(g_log) << "open(" << path << ", " << oflags << "): "
        << handle << " (" << error << ")";
    if (handle < 0)
        MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "open");
    if (createFlags & DELETE_ON_CLOSE) {
        int rc = unlink(path.c_str());
        if (rc != 0) {
            int error = errno;
            ::close(handle);
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "unlink");
        }
    }
#endif
    NativeStream::init(handle, ioManager, scheduler);
    m_supportsRead = accessFlags == READ || accessFlags == READWRITE;
    m_supportsWrite = accessFlags == WRITE || accessFlags == READWRITE ||
        accessFlags == APPEND;
    m_supportsSeek = accessFlags != APPEND;
    m_path = path;
}

#ifdef WINDOWS
void
FileStream::init(const std::wstring &path, AccessFlags accessFlags,
    CreateFlags createFlags, IOManager *ioManager, Scheduler *scheduler)
{
    NativeHandle handle;
    DWORD access = 0;
    if (accessFlags & READ)
        access |= GENERIC_READ;
    if (accessFlags & WRITE)
        access |= GENERIC_WRITE;
    if (accessFlags == APPEND)
        access = FILE_APPEND_DATA | SYNCHRONIZE;
    DWORD flags = 0;
    if (createFlags & DELETE_ON_CLOSE) {
        flags |= FILE_FLAG_DELETE_ON_CLOSE;
        createFlags = (CreateFlags)(createFlags & ~DELETE_ON_CLOSE);
    }
    if (ioManager)
        flags |= FILE_FLAG_OVERLAPPED;
    MORDOR_ASSERT(createFlags >= CREATE_NEW && createFlags <= TRUNCATE_EXISTING);
    handle = CreateFileW(path.c_str(),
        access,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        createFlags,
        flags,
        NULL);
    DWORD error = GetLastError();
    MORDOR_LOG_VERBOSE(g_log) << "CreateFileW(" << toUtf8(path) << ", "
        << access << ", " << createFlags << ", " << flags << "): " << handle
        << " (" << error << ")";
    if (handle == INVALID_HANDLE_VALUE)
        MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "CreateFileW");
    NativeStream::init(handle, ioManager, scheduler);
    m_supportsRead = accessFlags == READ || accessFlags == READWRITE;
    m_supportsWrite = accessFlags == WRITE || accessFlags == READWRITE ||
        accessFlags == APPEND;
    m_supportsSeek = accessFlags != APPEND;
    m_path = toUtf8(path);
}
#endif

}
