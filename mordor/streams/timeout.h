#ifndef __MORDOR_TIMEOUT_STREAM__
#define __MORDOR_TIMEOUT_STREAM__
// Copyright (c) 2010 - Mozy, Inc.

#include "filter.h"
#include "scheduler.h"

namespace Mordor {

class TimerManager;
class Timer;

class TimeoutStream : public FilterStream
{
public:
    typedef boost::shared_ptr<TimeoutStream> ptr;

public:
    TimeoutStream(Stream::ptr parent, TimerManager &timerManager, bool own = true)
        : FilterStream(parent),
          m_timerManager(timerManager),
          m_readTimeout(~0ull),
          m_writeTimeout(~0ull),
          m_readTimedOut(true),
          m_writeTimedOut(true),
          m_permaReadTimedOut(false),
          m_permaWriteTimedOut(false)
    {}

    unsigned long long readTimeout() const { return m_readTimeout; }
    void readTimeout(unsigned long long readTimeout);
    unsigned long long writeTimeout() const { return m_writeTimeout; }
    void writeTimeout(unsigned long long writeTimeout);

    size_t read(Buffer &buffer, size_t length);
    size_t write(const Buffer &buffer, size_t length);

private:
    TimerManager &m_timerManager;
    unsigned long long m_readTimeout, m_writeTimeout;
    bool m_readTimedOut, m_writeTimedOut, m_permaReadTimedOut, m_permaWriteTimedOut;
    boost::shared_ptr<Timer> m_readTimer, m_writeTimer;
    FiberMutex m_mutex;
};

}

#endif
