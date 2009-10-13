// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include "namedpipe.h"

#include "mordor/exception.h"
#include "mordor/string.h"

namespace Mordor {

NamedPipeStream::NamedPipeStream(const std::string &name, Flags flags)
{
    HANDLE hPipe = CreateNamedPipeW(toUtf16(name).c_str(),
        (DWORD)flags,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES,
        0, 0, 0, NULL);
    if (hPipe == INVALID_HANDLE_VALUE)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateNamedPipeW");
    init(hPipe);
    m_supportsRead = !!(flags & READ);
    m_supportsWrite = !!(flags & WRITE);
}

NamedPipeStream::NamedPipeStream(IOManagerIOCP &ioManager,
                                 const std::string &name, Flags flags)
{
    HANDLE hPipe = CreateNamedPipeW(toUtf16(name).c_str(),
        (DWORD)flags | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES,
        0, 0, 0, NULL);
    if (hPipe == INVALID_HANDLE_VALUE)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateNamedPipeW");
    init(&ioManager, hPipe);
    m_supportsRead = !!(flags & READ);
    m_supportsWrite = !!(flags & WRITE);
}

NamedPipeStream::NamedPipeStream(const std::wstring &name, Flags flags)
{
    HANDLE hPipe = CreateNamedPipeW(name.c_str(),
        (DWORD)flags,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES,
        0, 0, 0, NULL);
    if (hPipe == INVALID_HANDLE_VALUE)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateNamedPipeW");
    init(hPipe);
    m_supportsRead = !!(flags & READ);
    m_supportsWrite = !!(flags & WRITE);
}

NamedPipeStream::NamedPipeStream(IOManagerIOCP &ioManager,
                                 const std::wstring &name, Flags flags)
{
    HANDLE hPipe = CreateNamedPipeW(name.c_str(),
        (DWORD)flags | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES,
        0, 0, 0, NULL);
    if (hPipe == INVALID_HANDLE_VALUE)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateNamedPipeW");
    init(&ioManager, hPipe);
    m_supportsRead = !!(flags & READ);
    m_supportsWrite = !!(flags & WRITE);
}

void
NamedPipeStream::close(CloseType type)
{
    if (!DisconnectNamedPipe(m_hFile))
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("DisconnectNamedPipe");
}

void
NamedPipeStream::accept()
{
    OVERLAPPED *overlapped = NULL;
    if (m_ioManager) {
        MORDOR_ASSERT(Scheduler::getThis());
        m_ioManager->registerEvent(&m_readEvent);
        overlapped = &m_readEvent.overlapped;
    }
    BOOL ret = ConnectNamedPipe(m_hFile, overlapped);
    if (m_ioManager) {
        if (!ret && GetLastError() == ERROR_PIPE_CONNECTED) {
            m_ioManager->unregisterEvent(&m_readEvent);
            return;
        }
        if (!ret && GetLastError() != ERROR_IO_PENDING) {
            m_ioManager->unregisterEvent(&m_readEvent);
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("ConnectNamedPipe");
        }
        Scheduler::getThis()->yieldTo();
        if (!m_readEvent.ret) {
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_readEvent.lastError, "ConnectNamedPipe");
        }
    } else {
        if (!ret && GetLastError() == ERROR_PIPE_CONNECTED) {
            return;
        }
        if (!ret) {
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("ConnectNamedPipe");
        }
    }
}

}
