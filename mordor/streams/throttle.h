#ifndef __MORDOR_THROTTLE_STREAM_H__
#define __MORDOR_THROTTLE_STREAM_H__
// Copyright (c) 2009 - Mozy, Inc.

#include <boost/function.hpp>

#include "filter.h"

namespace Mordor {

class TimerManager;

class ThrottleStream : public FilterStream
{
public:
    ThrottleStream(Stream::ptr parent, boost::function<unsigned int ()> dg,
        TimerManager &timerManager, bool own = true)
        : FilterStream(parent, own),
          m_dg(dg),
          m_read(0),
          m_written(0),
          m_readTimestamp(0),
          m_writeTimestamp(0),
          m_timerManager(&timerManager)
    {}
    ThrottleStream(Stream::ptr parent, boost::function<unsigned int ()> dg,
        bool own = true)
        : FilterStream(parent, own),
          m_dg(dg),
          m_read(0),
          m_written(0),
          m_readTimestamp(0),
          m_writeTimestamp(0),
          m_timerManager(NULL)
    {}

    size_t read(Buffer &b, size_t len);
    size_t write(const Buffer &b, size_t len);

private:
    boost::function<unsigned int ()> m_dg;
    unsigned long long m_read, m_written, m_readTimestamp, m_writeTimestamp;
    TimerManager *m_timerManager;
};

}

#endif
