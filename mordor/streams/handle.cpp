// Copyright (c) 2009 - Mozy, Inc.

#include "handle.h"

#include "mordor/assert.h"
#include "mordor/log.h"
#include "mordor/runtime_linking.h"

namespace Mordor {

static Logger::ptr g_log = Log::lookup("mordor:streams:handle");

HandleStream::HandleStream()
: m_ioManager(NULL),
  m_skipCompletionPortOnSuccess(false),
  m_scheduler(NULL),
  m_pos(0),
  m_hFile(INVALID_HANDLE_VALUE),
  m_own(false),
  m_cancelRead(false),
  m_cancelWrite(false),
  m_maxOpSize(0xffffffff)
{}

void
HandleStream::init(HANDLE hFile, IOManager *ioManager, Scheduler *scheduler,
                   bool own)
{
    MORDOR_ASSERT(hFile != NULL);
    MORDOR_ASSERT(hFile != INVALID_HANDLE_VALUE);
    m_hFile = hFile;
    m_own = own;
    m_cancelRead = m_cancelWrite = false;
    m_ioManager = ioManager;
    m_scheduler = scheduler;
    m_type = GetFileType(hFile);
    if (m_type == FILE_TYPE_CHAR) {
        m_ioManager = NULL;
        CONSOLE_SCREEN_BUFFER_INFO info;
        if (GetConsoleScreenBufferInfo(hFile, &info))
            m_maxOpSize = info.dwSize.X * info.dwSize.Y / 2;
    }
    if (m_ioManager) {
        try {
            m_ioManager->registerFile(m_hFile);
            m_skipCompletionPortOnSuccess = !!pSetFileCompletionNotificationModes(m_hFile,
                FILE_SKIP_COMPLETION_PORT_ON_SUCCESS | FILE_SKIP_SET_EVENT_ON_HANDLE);
        } catch(...) {
            if (own) {
                CloseHandle(m_hFile);
                m_hFile = INVALID_HANDLE_VALUE;
            }
            throw;
        }
    }
}

HandleStream::~HandleStream()
{
    if (m_hFile != INVALID_HANDLE_VALUE && m_own) {
        SchedulerSwitcher switcher(m_scheduler);
        BOOL result = CloseHandle(m_hFile);
        MORDOR_LOG_LEVEL(g_log, result ? Log::VERBOSE : Log::ERROR) << this
            << " CloseHandle(" << m_hFile << "): " << result << " ("
            << lastError() << ")";
    }
}

static bool g_supportsCancel;
static bool g_queriedSupportsCancel;

bool
HandleStream::supportsCancel()
{
    if (m_ioManager)
        return true;
    if (!g_supportsCancel && !g_queriedSupportsCancel) {
        BOOL bRet = pCancelIoEx(INVALID_HANDLE_VALUE, NULL);
        MORDOR_ASSERT(!bRet);
        g_supportsCancel = lastError() != ERROR_CALL_NOT_IMPLEMENTED;
        g_queriedSupportsCancel = true;
    }
    return g_supportsCancel;
}

void
HandleStream::close(CloseType type)
{
    MORDOR_ASSERT(type == BOTH);
    if (m_hFile != INVALID_HANDLE_VALUE && m_own) {
        SchedulerSwitcher switcher(m_scheduler);
        BOOL result = CloseHandle(m_hFile);
        error_t error = lastError();
        MORDOR_LOG_LEVEL(g_log, result ? Log::VERBOSE : Log::ERROR) << this
            << " CloseHandle(" << m_hFile << "): " << result << " ("
            << error << ")";
        if (!result)
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "CloseHandle");
        m_hFile = INVALID_HANDLE_VALUE;
    }
}

size_t
HandleStream::read(void *buffer, size_t length)
{
    if (m_cancelRead)
        MORDOR_THROW_EXCEPTION(OperationAbortedException());
    SchedulerSwitcher switcher(m_ioManager ? NULL : m_scheduler);
    DWORD read;
    OVERLAPPED *overlapped = NULL;
    if (m_ioManager) {
        MORDOR_ASSERT(Scheduler::getThis());
        m_ioManager->registerEvent(&m_readEvent);
        overlapped = &m_readEvent.overlapped;
        if (supportsSeek()) {
            overlapped->Offset = (DWORD)m_pos;
            overlapped->OffsetHigh = (DWORD)(m_pos >> 32);
        }
    }
    length = (std::min)(length, m_maxOpSize);
    BOOL ret = ReadFile(m_hFile, buffer, (DWORD)length, &read, overlapped);
    Log::Level level = Log::DEBUG;
    if (!ret) {
        if (lastError() == ERROR_HANDLE_EOF) {
        } else if (m_type == FILE_TYPE_PIPE && lastError() == ERROR_BROKEN_PIPE) {
            ret = TRUE;
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
    MORDOR_LOG_LEVEL(g_log, level) << this << " ReadFile(" << m_hFile << ", "
        << length << "): " << ret << " - " << read << " (" << error << ")";
    if (m_ioManager) {
        if (!ret && error == ERROR_HANDLE_EOF) {
            m_ioManager->unregisterEvent(&m_readEvent);
            return 0;
        }
        if (!ret && error != ERROR_IO_PENDING) {
            m_ioManager->unregisterEvent(&m_readEvent);
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("ReadFile");
        }
        if (m_skipCompletionPortOnSuccess && ret)
            m_ioManager->unregisterEvent(&m_readEvent);
        else
            Scheduler::yieldTo();
        DWORD error = pRtlNtStatusToDosError((NTSTATUS)m_readEvent.overlapped.Internal);
        MORDOR_LOG_LEVEL(g_log, error && error != ERROR_HANDLE_EOF ? Log::ERROR : Log::VERBOSE)
            << this << " ReadFile(" << m_hFile << ", " << length << "): "
            << m_readEvent.overlapped.InternalHigh << " (" << error << ")";
        if (error == ERROR_HANDLE_EOF)
            return 0;
        if (error)
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "ReadFile");
        if (supportsSeek()) {
            m_pos = ((long long)overlapped->Offset | ((long long)overlapped->OffsetHigh << 32)) +
                m_readEvent.overlapped.InternalHigh;
        }
        return m_readEvent.overlapped.InternalHigh;
    }
    if (!ret)
        MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "ReadFile");
    return read;
}

void
HandleStream::cancelRead()
{
    m_cancelRead = true;
    if (m_ioManager) {
        m_ioManager->cancelEvent(m_hFile, &m_readEvent);
    } else {
        if (!pCancelIoEx(m_hFile, NULL))
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CancelIoEx");
    }
}

size_t
HandleStream::write(const void *buffer, size_t length)
{
    if (m_cancelWrite)
        MORDOR_THROW_EXCEPTION(OperationAbortedException());
    SchedulerSwitcher switcher(m_ioManager ? NULL : m_scheduler);
    DWORD written;
    OVERLAPPED *overlapped = NULL;
    if (m_ioManager) {
        MORDOR_ASSERT(Scheduler::getThis());
        m_ioManager->registerEvent(&m_writeEvent);
        overlapped = &m_writeEvent.overlapped;
        if (supportsSeek()) {
            overlapped->Offset = (DWORD)m_pos;
            overlapped->OffsetHigh = (DWORD)(m_pos >> 32);
        }
    }
    length = (std::min)(length, m_maxOpSize);
    BOOL ret = WriteFile(m_hFile, buffer, (DWORD)length, &written, overlapped);
    Log::Level level = Log::DEBUG;
    if (!ret) {
        if (m_ioManager && lastError() == ERROR_IO_PENDING)
            level = Log::TRACE;
        else
            level = Log::ERROR;
    }
    error_t error = lastError();
    MORDOR_LOG_LEVEL(g_log, level) << this << " WriteFile(" << m_hFile << ", "
        << length << "): " << ret << " - " << written << " (" << error << ")";
    if (m_ioManager) {
        if (!ret && error != ERROR_IO_PENDING) {
            m_ioManager->unregisterEvent(&m_writeEvent);
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WriteFile");
        }
        if (m_skipCompletionPortOnSuccess && ret)
            m_ioManager->unregisterEvent(&m_writeEvent);
        else
            Scheduler::yieldTo();
        DWORD error = pRtlNtStatusToDosError((NTSTATUS)m_writeEvent.overlapped.Internal);
        MORDOR_LOG_LEVEL(g_log, error ? Log::ERROR : Log::VERBOSE) << this
            << " WriteFile(" << m_hFile << ", " << length << "): "
            << m_writeEvent.overlapped.InternalHigh << " (" << error << ")";
        if (error)
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "WriteFile");
        if (supportsSeek()) {
            m_pos = ((long long)overlapped->Offset | ((long long)overlapped->OffsetHigh << 32)) +
                m_writeEvent.overlapped.InternalHigh;
        }
        return m_writeEvent.overlapped.InternalHigh;
    }
    if (!ret)
        MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "WriteFile");
    return written;
}

void
HandleStream::cancelWrite()
{
    m_cancelWrite = true;
    if (m_ioManager) {
        m_ioManager->cancelEvent(m_hFile, &m_writeEvent);
    } else {
        MORDOR_ASSERT(supportsCancel());
        if (!pCancelIoEx(m_hFile, NULL))
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CancelIoEx");
    }
}

long long
HandleStream::seek(long long offset, Anchor anchor)
{
    SchedulerSwitcher switcher(m_ioManager ? NULL : m_scheduler);
    if (m_ioManager) {
        if (supportsSeek()) {
            switch (anchor) {
                case BEGIN:
                    if (offset < 0) {
                        MORDOR_THROW_EXCEPTION(std::invalid_argument("resulting offset is negative"));
                    }
                    return m_pos = offset;
                case CURRENT:
                    if (m_pos + offset < 0) {
                        MORDOR_THROW_EXCEPTION(std::invalid_argument("resulting offset is negative"));
                    }
                    return m_pos += offset;
                case END:
                    {
                        long long end = size();
                        if (end + offset < 0) {
                            MORDOR_THROW_EXCEPTION(std::invalid_argument("resulting offset is negative"));
                        }
                        return m_pos = end + offset;
                    }
                default:
                    MORDOR_ASSERT(false);
            }
        } else {
            MORDOR_ASSERT(false);
        }
    }

    long long pos;
    BOOL ret = SetFilePointerEx(m_hFile, *(LARGE_INTEGER*)&offset,
        (LARGE_INTEGER*)&pos, (DWORD)anchor);
    error_t error = lastError();
    MORDOR_LOG_LEVEL(g_log, ret ? Log::VERBOSE : Log::ERROR) << this
        << " SetFilePointerEx(" << m_hFile << ", " << offset << ", " << pos
        << ", " << anchor << "): " << ret << " (" << error << ")";
    if (!ret)
        MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "SetFilePointerEx");
    return pos;
}

long long
HandleStream::size()
{
    SchedulerSwitcher switcher(m_scheduler);
    long long size;
    BOOL ret = GetFileSizeEx(m_hFile, (LARGE_INTEGER*)&size);
    error_t error = lastError();
    MORDOR_LOG_VERBOSE(g_log) << this << " GetFileSizeEx(" << m_hFile << ", "
        << size << "): " << ret << " (" << error << ")";
    if (!ret)
        MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "GetFileSizeEx");
    return size;
}

void
HandleStream::truncate(long long size)
{
    SchedulerSwitcher switcher(m_scheduler);
    long long pos = seek(0, CURRENT);
    seek(size, BEGIN);
    BOOL ret = SetEndOfFile(m_hFile);
    error_t error = lastError();
    MORDOR_LOG_LEVEL(g_log, ret ? Log::VERBOSE : Log::ERROR) << this
        << " SetEndOfFile(" << m_hFile << "): " << ret << " ("
        << error << ")";
    seek(pos, BEGIN);
    if (!ret)
        MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "SetEndOfFile");
}

void
HandleStream::flush(bool flushParent)
{
    SchedulerSwitcher switcher(m_scheduler);
    BOOL ret = FlushFileBuffers(m_hFile);
    error_t error = lastError();
    MORDOR_LOG_LEVEL(g_log, ret ? Log::VERBOSE : Log::ERROR) << this
        << " FlushFileBuffers(" << m_hFile << "): " << ret << " (" << error
        << ")";
    if (!ret)
        MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "FlushFileBuffers");
}

}
