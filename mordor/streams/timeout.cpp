// Copyright (c) 2010 - Mozy, Inc.

#include "timeout.h"

#include "mordor/assert.h"
#include "mordor/exception.h"
#include "mordor/log.h"
#include "mordor/socket.h"
#include "mordor/timer.h"

namespace Mordor {

static Logger::ptr g_log = Log::lookup("mordor:streams:timeout");

static void cancelReadLocal(Stream::ptr stream) { stream->cancelRead(); }
static void cancelWriteLocal(Stream::ptr stream) { stream->cancelWrite(); }
static void cancelReadWriteLocal(Stream::ptr stream)
{
    cancelReadLocal(stream);
    cancelWriteLocal(stream);
}

TimeoutHandler::~TimeoutHandler()
{
    if (m_timer)
        cancelTimer();
}

void
TimeoutHandler::onTimeout()
{
    if (m_lastTimedOut == TIMING) {
        MORDOR_LOG_DEBUG(g_log) << this << " timeout";
        m_lastTimedOut = m_permaTimedOut = TIMEDOUT;
        if (m_timeoutDg)
            m_timeoutDg();
    } else {
        MORDOR_LOG_DEBUG(g_log) << this << " timeout no longer registered";
    }
}

void
TimeoutHandler::setTimeout(unsigned long long timeout, TimeoutDg dg)
{
    m_timeout = timeout;
    m_timeoutDg = dg;
    if (m_timer) {
        // timer is running, need to stop it or reset its counting down
        if (!isTimeoutSet()) {
            cancelTimer();
        } else {
            m_timer->reset(timeout, true);
        }
    } else {
        // timer is not running, start it
        if (isTimeoutSet()) {
            // start new timer if read/write is ongoing
            // OR auto start is set
            if (m_lastTimedOut == TIMING || m_autoStart) {
                m_timer = m_timerManager.registerConditionTimer(timeout,
                        boost::bind(&TimeoutHandler::onTimeout, this),
                        shared_from_this());
            }
        }
    }
}

void
TimeoutHandler::startTimer()
{
    MORDOR_LOG_TRACE(g_log) << this << " startTimer()";
    if (m_permaTimedOut == TIMEDOUT)
        MORDOR_THROW_EXCEPTION(TimedOutException());
    MORDOR_ASSERT(!m_timer);
    m_lastTimedOut = TIMING;
    if (isTimeoutSet())
        m_timer = m_timerManager.registerConditionTimer(m_timeout,
            boost::bind(&TimeoutHandler::onTimeout, this),
            shared_from_this());
}

bool
TimeoutHandler::cancelTimer()
{
    MORDOR_LOG_TRACE(g_log) << this << " cancelTimer()";
    bool res = (m_lastTimedOut == TIMEDOUT);
    if (m_timer) {
        m_timer->cancel();
        m_timer.reset();
    }
    m_lastTimedOut = NONE;
    return res;
}

bool
TimeoutHandler::refreshTimer()
{
    MORDOR_LOG_TRACE(g_log) << this << " refreshTimer()";
    bool res = (m_lastTimedOut == TIMEDOUT);
    if (m_timer) {
        m_timer->refresh();
    }
    m_lastTimedOut = TIMING;
    return res;
}

void
TimeoutStream::readTimeout(unsigned long long readTimeout)
{
    FiberMutex::ScopedLock lock(m_mutex);
    m_reader->setTimeout(readTimeout, boost::bind(&cancelReadLocal, parent()));
}

void
TimeoutStream::writeTimeout(unsigned long long writeTimeout)
{
    FiberMutex::ScopedLock lock(m_mutex);
    m_writer->setTimeout(writeTimeout, boost::bind(&cancelWriteLocal, parent()));
}

void
TimeoutStream::idleTimeout(unsigned long long idleTimeout)
{
    FiberMutex::ScopedLock lock(m_mutex);
    m_idler->setTimeout(idleTimeout, boost::bind(&cancelReadWriteLocal, parent()));
}

size_t
TimeoutStream::read(Buffer &buffer, size_t length)
{
    FiberMutex::ScopedLock lock(m_mutex);
    // start read timer & tickle idle
    m_reader->startTimer();
    m_idler->refreshTimer();
    lock.unlock();
    size_t result;
    try {
        result = parent()->read(buffer, length);
    } catch (OperationAbortedException &) {
        lock.lock();
        if (m_reader->cancelTimer() || m_idler->cancelTimer())
            MORDOR_THROW_EXCEPTION(TimedOutException());
        throw;
    } catch (...) {
        lock.lock();
        m_reader->cancelTimer();
        m_idler->cancelTimer();
        throw;
    }
    lock.lock();
    // read done, stop read timer & tickle idle
    m_reader->cancelTimer();
    m_idler->refreshTimer();
    return result;
}

size_t
TimeoutStream::write(const Buffer &buffer, size_t length)
{
    FiberMutex::ScopedLock lock(m_mutex);
    // start write timer & tickle idle
    m_writer->startTimer();
    m_idler->refreshTimer();
    lock.unlock();
    size_t result;
    try {
        result = parent()->write(buffer, length);
    } catch (OperationAbortedException &) {
        lock.lock();
        if (m_writer->cancelTimer() || m_idler->cancelTimer())
            MORDOR_THROW_EXCEPTION(TimedOutException());
        throw;
    } catch (...) {
        lock.lock();
        m_writer->cancelTimer();
        m_idler->cancelTimer();
        throw;
    }
    lock.lock();
    // write done, stop write timer & tickle idle
    m_writer->cancelTimer();
    m_idler->refreshTimer();
    return result;
}

}
