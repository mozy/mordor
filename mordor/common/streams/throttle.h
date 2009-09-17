#ifndef __THROTTLE_STREAM_H__
#define __THROTTLE_STREAM_H__
// Copyright (c) 2009 - Decho Corp.

#include "filter.h"
#include "mordor/common/config.h"

class TimerManager;

class ThrottleStream : public FilterStream
{
public:
    ThrottleStream(Stream::ptr parent, ConfigVar<unsigned int> &throttle,
        TimerManager &timerManager, bool own = true)
        : FilterStream(parent, own),
          m_throttle(throttle),
          m_read(0),
          m_written(0),
          m_readTimestamp(0),
          m_writeTimestamp(0),
          m_timerManager(&timerManager)
    {}
    ThrottleStream(Stream::ptr parent, ConfigVar<unsigned int> &throttle,
        bool own = true)
        : FilterStream(parent, own),
          m_throttle(throttle),
          m_read(0),
          m_written(0),
          m_readTimestamp(0),
          m_writeTimestamp(0),
          m_timerManager(NULL)
    {}

    size_t read(Buffer &b, size_t len);
    size_t write(const Buffer &b, size_t len);

private:
    ConfigVar<unsigned int> m_throttle;
    unsigned long long m_read, m_written, m_readTimestamp, m_writeTimestamp;
    TimerManager *m_timerManager;
};

#endif
