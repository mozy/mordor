// Copyright (c) 2009 - Mozy, Inc.

#include "namedpipe.h"

#include "mordor/assert.h"
#include "mordor/exception.h"
#include "mordor/log.h"
#include "mordor/runtime_linking.h"
#include "mordor/string.h"

namespace Mordor {

static Logger::ptr g_log = Log::lookup("mordor:streams::namedpipe");

NamedPipeStream::NamedPipeStream(const std::string &name, Flags flags, IOManager *ioManager, Scheduler *scheduler, DWORD pipeModeFlags)
{
    init(toUtf16(name), flags, pipeModeFlags,ioManager, scheduler);
}

NamedPipeStream::NamedPipeStream(const std::wstring &name, Flags flags, IOManager *ioManager, Scheduler *scheduler, DWORD pipeModeFlags)
{
    init(name, flags, pipeModeFlags, ioManager, scheduler);
}

void NamedPipeStream::init(const std::wstring &name, Flags flags, DWORD pipeModeFlags, IOManager *ioManager, Scheduler *scheduler)
{
    if (pipeModeFlags == (DWORD)-1) {
        pipeModeFlags = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT;
    }

    HANDLE hPipe = CreateNamedPipeW(name.c_str(),
        (DWORD)flags | (ioManager ? FILE_FLAG_OVERLAPPED : 0),
        pipeModeFlags,
        PIPE_UNLIMITED_INSTANCES,
        0, 0, 0, NULL);
    error_t error = lastError();
    MORDOR_LOG_LEVEL(g_log, hPipe == INVALID_HANDLE_VALUE ? Log::ERROR : Log::VERBOSE)
        << this << " CreateNamedPipeW(" << toUtf8(name) << ", " << flags
        << "): " << hPipe << " (" << error << ")";
    if (hPipe == INVALID_HANDLE_VALUE)
        MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "CreateNamedPipeW");
    HandleStream::init(hPipe, ioManager, scheduler);
    m_supportsRead = !!(flags & READ);
    m_supportsWrite = !!(flags & WRITE);
}

void
NamedPipeStream::close(CloseType type)
{
    if (m_hFile != INVALID_HANDLE_VALUE) {
        SchedulerSwitcher switcher(m_scheduler);
        BOOL ret = DisconnectNamedPipe(m_hFile);
        error_t error = lastError();
        MORDOR_LOG_VERBOSE(g_log) << this << " DisconnectNamedPipe(" << m_hFile
            << "): " << ret << " (" << error << ")";
        if (!ret)
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "DisconnectNamedPipe");
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
    Log::Level level = Log::INFO;
    if (!ret) {
        if (lastError() == ERROR_PIPE_CONNECTED) {
        } else if (m_ioManager) {
            if (lastError() == ERROR_IO_PENDING)
                level = Log::TRACE;
            else
                level = Log::ERROR;
        } else {
            level = Log::ERROR;
        }
    }
    error_t error = lastError();
    MORDOR_LOG_LEVEL(g_log, level) << this
        << " ConnectNamedPipe(" << m_hFile << "): " << ret << " ("
        << error << ")";
    if (m_ioManager) {
        if (!ret && error == ERROR_PIPE_CONNECTED) {
            m_ioManager->unregisterEvent(&m_readEvent);
            return;
        }
        if (!ret && error != ERROR_IO_PENDING) {
            m_ioManager->unregisterEvent(&m_readEvent);
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("ConnectNamedPipe");
        }
        if (ret && m_skipCompletionPortOnSuccess)
            m_ioManager->unregisterEvent(&m_readEvent);
        else
            Scheduler::yieldTo();
        error_t error = pRtlNtStatusToDosError((NTSTATUS)m_readEvent.overlapped.Internal);
        MORDOR_LOG_LEVEL(g_log, error ? Log::ERROR : Log::INFO) << this
            << " ConnectNamedPipe(" << m_hFile << "): (" << error
            << ")";
        if (error)
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "ConnectNamedPipe");
    } else {
        if (!ret && error == ERROR_PIPE_CONNECTED)
            return;
        if (!ret)
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "ConnectNamedPipe");
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

void
NamedPipeStream::disconnectClient()
{
    // Prepare for reuse of the named pipe handle
    FlushFileBuffers(m_hFile);
    DisconnectNamedPipe(m_hFile);
}

}
