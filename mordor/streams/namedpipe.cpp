// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/pch.h"

#include "namedpipe.h"

#include "mordor/exception.h"
#include "mordor/runtime_linking.h"
#include "mordor/string.h"

namespace Mordor {

NamedPipeStream::NamedPipeStream(const std::string &name, Flags flags, IOManager *ioManager, Scheduler *scheduler)
{
    HANDLE hPipe = CreateNamedPipeW(toUtf16(name).c_str(),
        (DWORD)flags | (ioManager ? FILE_FLAG_OVERLAPPED : 0),
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES,
        0, 0, 0, NULL);
    if (hPipe == INVALID_HANDLE_VALUE)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateNamedPipeW");
    HandleStream::init(hPipe, ioManager, scheduler);
    m_supportsRead = !!(flags & READ);
    m_supportsWrite = !!(flags & WRITE);
}

NamedPipeStream::NamedPipeStream(const std::wstring &name, Flags flags, IOManager *ioManager, Scheduler *scheduler)
{
    HANDLE hPipe = CreateNamedPipeW(name.c_str(),
        (DWORD)flags | (ioManager ? FILE_FLAG_OVERLAPPED : 0),
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES,
        0, 0, 0, NULL);
    if (hPipe == INVALID_HANDLE_VALUE)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateNamedPipeW");
    HandleStream::init(hPipe, ioManager, scheduler);
    m_supportsRead = !!(flags & READ);
    m_supportsWrite = !!(flags & WRITE);
}

void
NamedPipeStream::close(CloseType type)
{
    if (m_hFile != INVALID_HANDLE_VALUE) {
        SchedulerSwitcher switcher(m_scheduler);
        if (!DisconnectNamedPipe(m_hFile))
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("DisconnectNamedPipe");
    }
    HandleStream::close(type);
}

void
NamedPipeStream::accept()
{
    if (m_cancelRead)
        MORDOR_THROW_EXCEPTION(OperationAbortedException());
    SchedulerSwitcher switcher(m_ioManager ? NULL : m_scheduler);
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
        if (ret && m_skipCompletionPortOnSuccess)
            m_ioManager->unregisterEvent(&m_readEvent);
        else
            Scheduler::getThis()->yieldTo();
        DWORD error = pRtlNtStatusToDosError((NTSTATUS)m_readEvent.overlapped.Internal);
        if (error)
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "ConnectNamedPipe");
    } else {
        if (!ret && GetLastError() == ERROR_PIPE_CONNECTED) {
            return;
        }
        if (!ret) {
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("ConnectNamedPipe");
        }
    }
}

void
NamedPipeStream::cancelAccept()
{
    m_cancelRead = true;
    if (m_ioManager) {
        m_ioManager->cancelEvent(m_hFile, &m_readEvent);
    } else {
        MORDOR_ASSERT(supportsCancel());
        if (!pCancelIoEx(m_hFile, NULL))
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CancelIoEx");
    }
}

}
