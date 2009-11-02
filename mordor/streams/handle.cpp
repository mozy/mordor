// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include "handle.h"

#include "mordor/exception.h"
#include "mordor/runtime_linking.h"

namespace Mordor {

HandleStream::HandleStream()
: m_ioManager(NULL),
  m_scheduler(NULL),
  m_pos(0),
  m_hFile(INVALID_HANDLE_VALUE),
  m_own(false),
  m_cancelRead(false),
  m_cancelWrite(false)
{}

void
HandleStream::init(IOManagerIOCP *ioManager, Scheduler *scheduler, HANDLE hFile, bool own)
{
    MORDOR_ASSERT(hFile != NULL);
    MORDOR_ASSERT(hFile != INVALID_HANDLE_VALUE);
    m_hFile = hFile;
    m_own = own;
    m_cancelRead = m_cancelWrite = false;
    m_ioManager = ioManager;
    m_scheduler = scheduler;
    if (m_ioManager) {
        try {
            m_ioManager->registerFile(m_hFile);
        } catch(...) {
            if (own) {
                CloseHandle(m_hFile);
            }
            throw;
        }
    }
}

HandleStream::~HandleStream()
{
    if (m_hFile != INVALID_HANDLE_VALUE && m_own) {
        SchedulerSwitcher switcher(m_scheduler);
        CloseHandle(m_hFile);
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
        g_supportsCancel = GetLastError() != ERROR_CALL_NOT_IMPLEMENTED;
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
        if (!CloseHandle(m_hFile))
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CloseHandle");
        m_hFile = INVALID_HANDLE_VALUE;
    }
}

size_t
HandleStream::read(Buffer &b, size_t len)
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
    if (len > 0xffffffff)
        len = 0xffffffff;
    Buffer::SegmentData buf = b.writeBuf(len);
    BOOL ret = ReadFile(m_hFile, buf.start(), (DWORD)len, &read, overlapped);
    if (m_ioManager) {
        if (!ret && GetLastError() == ERROR_HANDLE_EOF) {
            m_ioManager->unregisterEvent(&m_readEvent);
            return 0;
        }
        if (!ret && GetLastError() != ERROR_IO_PENDING) {
            m_ioManager->unregisterEvent(&m_readEvent);
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("ReadFile");
        }
        Scheduler::getThis()->yieldTo();
        if (!m_readEvent.ret && m_readEvent.lastError == ERROR_HANDLE_EOF) {
            return 0;
        }
        if (!m_readEvent.ret) {
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_readEvent.lastError, "ReadFile");
        }
        if (supportsSeek()) {
            m_pos = ((long long)overlapped->Offset | ((long long)overlapped->OffsetHigh << 32)) +
                m_readEvent.numberOfBytes;
        }
        b.produce(m_readEvent.numberOfBytes);
        return m_readEvent.numberOfBytes;
    }
    if (!ret) {
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("ReadFile");
    }
    b.produce(read);
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
HandleStream::write(const Buffer &b, size_t len)
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
    if (len > 0xffffffff)
        len = 0xffffffff;
    const Buffer::SegmentData buf = b.readBuf(len);
    BOOL ret = WriteFile(m_hFile, buf.start(), (DWORD)len, &written, overlapped);
    if (m_ioManager) {
        if (!ret && GetLastError() != ERROR_IO_PENDING) {
            m_ioManager->unregisterEvent(&m_writeEvent);
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WriteFile");
        }
        Scheduler::getThis()->yieldTo();
        if (!m_writeEvent.ret) {
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_writeEvent.lastError, "WriteFile");
        }
        if (supportsSeek()) {
            m_pos = ((long long)overlapped->Offset | ((long long)overlapped->OffsetHigh << 32)) +
                m_writeEvent.numberOfBytes;
        }
        return m_writeEvent.numberOfBytes;
    }
    if (!ret) {
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WriteFile");
    }
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
    if (!SetFilePointerEx(m_hFile, *(LARGE_INTEGER*)&offset,
        (LARGE_INTEGER*)&pos, (DWORD)anchor)) {
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("SetFilePointerEx");
    }
    return pos;
}

long long
HandleStream::size()
{
    SchedulerSwitcher switcher(m_scheduler);
    long long size;
    if (!GetFileSizeEx(m_hFile, (LARGE_INTEGER*)&size)) {
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("GetFileSizeEx");
    }
    return size;
}

void
HandleStream::truncate(long long size)
{
    SchedulerSwitcher switcher(m_scheduler);
    long long pos = seek(0, CURRENT);
    seek(size, BEGIN);
    BOOL ret = SetEndOfFile(m_hFile);
    DWORD lastError = GetLastError();
    seek(pos, BEGIN);
    if (!ret) {
        MORDOR_THROW_EXCEPTION_FROM_ERROR_API(lastError, "SetEndOfFile");
    }
}

}
