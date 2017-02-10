#ifndef __MORDOR_TIMEOUT_STREAM__
#define __MORDOR_TIMEOUT_STREAM__
// Copyright (c) 2010 - Mozy, Inc.

#include <boost/enable_shared_from_this.hpp>

#include "filter.h"
#include "mordor/fibersynchronization.h"

namespace Mordor {

class TimerManager;
class Timer;

class TimeoutHandler : public boost::enable_shared_from_this<TimeoutHandler>
{
public:
    typedef boost::function<void ()> TimeoutDg;

private:
    enum STATUS {
        NONE,
        TIMING,
        TIMEDOUT
    };

public:
    TimeoutHandler(TimerManager &timerManager, bool autoRestart = false):
        m_timeout(~0ull),
        m_lastTimedOut(NONE),
        m_permaTimedOut(NONE),
        m_autoStart(autoRestart),
        m_timerManager(timerManager)
    {}

    ~TimeoutHandler();

    unsigned long long getTimeout() const { return m_timeout; }
    void setTimeout(unsigned long long timeout, TimeoutDg dg);
    bool isTimeoutSet() const { return m_timeout != ~0ull; }

    /// start timer
    /// @throws TimedOutException if it already timed out
    void startTimer();

    /// cancel the timer
    /// @return if it already timed out before cancelling
    bool cancelTimer();

    /// refresh the timer to restart
    /// @return if it already timed out before refreshing
    bool refreshTimer();

private:
    void onTimeout();

private:
    unsigned long long m_timeout;
    STATUS m_lastTimedOut, m_permaTimedOut;
    bool m_autoStart;
    TimeoutDg m_timeoutDg;
    boost::shared_ptr<Timer> m_timer;
    TimerManager &m_timerManager;
};

/// @brief Timeout Stream
/// @details
/// Provide timeout mechanism for read/write operations.
/// 3 kinds of timeouts are supported:
/// - Read: timed out when read() takes so long.
/// - Write: timed out when write() takes so long.
/// - Idle: timed out when no IO(read/write) on the stream.
class TimeoutStream : public FilterStream
{
public:
    typedef boost::shared_ptr<TimeoutStream> ptr;

public:
    TimeoutStream(Stream::ptr parent, TimerManager &timerManager, bool own = true)
        : FilterStream(parent, own),
          m_reader(new TimeoutHandler(timerManager, false)),
          m_writer(new TimeoutHandler(timerManager, false)),
          m_idler(new TimeoutHandler(timerManager, true))
    {}

    unsigned long long readTimeout() const { return m_reader->getTimeout(); }
    void readTimeout(unsigned long long readTimeout);
    unsigned long long writeTimeout() const { return m_writer->getTimeout(); }
    void writeTimeout(unsigned long long writeTimeout);
    unsigned long long idleTimeout() const { return m_idler->getTimeout(); }
    void idleTimeout(unsigned long long idleTimeout);

    using FilterStream::read;
    size_t read(Buffer &buffer, size_t length);
    using FilterStream::write;
    size_t write(const Buffer &buffer, size_t length);

private:
    boost::shared_ptr<TimeoutHandler> m_reader, m_writer, m_idler;
    FiberMutex m_mutex;
};

}

#endif
