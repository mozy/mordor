// Copyright (c) 2010 - Mozy, Inc.

#include "mordor/pch.h"

#include "timeout.h"

#include "mordor/exception.h"
#include "mordor/log.h"
#include "mordor/socket.h"
#include "mordor/timer.h"

namespace Mordor {

static Logger::ptr g_log = Log::lookup("mordor:streams:timeout");

static void cancelReadLocal(Stream::ptr stream, bool &flag)
{
    if (!flag) {
        MORDOR_LOG_INFO(g_log) << "read timeout";
        stream->cancelRead();
        flag = true;
    } else {
        MORDOR_LOG_DEBUG(g_log) << "read timeout no longer registered";
    }
}

static void cancelWriteLocal(Stream::ptr stream, bool &flag)
{
    if (!flag) {
        MORDOR_LOG_INFO(g_log) << "write timeout";
        stream->cancelWrite();
        flag = true;
    } else {
        MORDOR_LOG_DEBUG(g_log) << "write timeout no longer registered";
    }
}

void
TimeoutStream::readTimeout(unsigned long long readTimeout)
{
    FiberMutex::ScopedLock lock(m_mutex);
    m_readTimeout = readTimeout;
    if (m_readTimer) {
        if (readTimeout == ~0ull) {
            m_readTimer->cancel();
            m_readTimer.reset();
        } else {
            m_readTimer->reset(readTimeout, true);
        }
    } else if (m_readTimeout != ~0ull && !m_readTimedOut) {
        m_readTimer = m_timerManager.registerTimer(m_readTimeout,
            boost::bind(&cancelReadLocal, parent(),
            boost::ref(m_readTimedOut)));
    }
}

void
TimeoutStream::writeTimeout(unsigned long long writeTimeout)
{
    FiberMutex::ScopedLock lock(m_mutex);
    m_writeTimeout = writeTimeout;
    if (m_writeTimer) {
        if (writeTimeout == ~0ull) {
            m_writeTimer->cancel();
            m_writeTimer.reset();
        } else {
            m_writeTimer->reset(writeTimeout, true);
        }
    } else if (m_writeTimeout != ~0ull && !m_writeTimedOut) {
        m_writeTimer = m_timerManager.registerTimer(m_writeTimeout,
            boost::bind(&cancelWriteLocal, parent(),
            boost::ref(m_writeTimedOut)));
    }
}

size_t
TimeoutStream::read(Buffer &buffer, size_t length)
{
    FiberMutex::ScopedLock lock(m_mutex);
    m_readTimedOut = false;
    MORDOR_ASSERT(!m_readTimer);
    if (m_readTimeout != ~0ull)
        m_readTimer = m_timerManager.registerTimer(m_readTimeout,
            boost::bind(&cancelReadLocal, parent(),
            boost::ref(m_readTimedOut)));
    lock.unlock();
    size_t result;
    try {
        result = parent()->read(buffer, length);
    } catch (OperationAbortedException &) {
        lock.lock();
        if (m_readTimer) {
            m_readTimer->cancel();
            m_readTimer.reset();
        }
        if (m_readTimedOut)
            MORDOR_THROW_EXCEPTION(TimedOutException());
        m_readTimedOut = true;
        throw;
    } catch (...) {
        lock.lock();
        if (m_readTimer) {
            m_readTimer->cancel();
            m_readTimer.reset();
        }
        m_readTimedOut = true;
        throw;
    }
    lock.lock();
    if (m_readTimer) {
        m_readTimer->cancel();
        m_readTimer.reset();
    }
    m_readTimedOut = true;
    return result;
}

size_t
TimeoutStream::write(const Buffer &buffer, size_t length)
{
    FiberMutex::ScopedLock lock(m_mutex);
    m_writeTimedOut = false;
    MORDOR_ASSERT(!m_writeTimer);
    if (m_writeTimeout != ~0ull)
        m_writeTimer = m_timerManager.registerTimer(m_writeTimeout,
            boost::bind(&cancelWriteLocal, parent(),
            boost::ref(m_writeTimedOut)));
    lock.unlock();
    size_t result;
    try {
        result = parent()->write(buffer, length);
    } catch (OperationAbortedException &) {
        lock.lock();
        if (m_writeTimer) {
            m_writeTimer->cancel();
            m_writeTimer.reset();
        }
        if (m_writeTimedOut)
            MORDOR_THROW_EXCEPTION(TimedOutException());
        m_writeTimedOut = true;
        throw;
    } catch (...) {
        lock.lock();
        if (m_writeTimer) {
            m_writeTimer->cancel();
            m_writeTimer.reset();
        }
        m_writeTimedOut = true;
        throw;
    }
    lock.lock();
    if (m_writeTimer) {
        m_writeTimer->cancel();
        m_writeTimer.reset();
    }
    m_writeTimedOut = true;
    return result;
}

}
