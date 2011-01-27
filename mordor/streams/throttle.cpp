// Copyright (c) 2009 - Mozy, Inc.

#include "throttle.h"

#include "mordor/assert.h"
#include "mordor/log.h"
#include "mordor/sleep.h"
#include "mordor/timer.h"

namespace Mordor {

static Logger::ptr g_log = Log::lookup("mordor:streams:throttle");

size_t
ThrottleStream::read(Buffer &b, size_t len)
{
    MORDOR_ASSERT(len != 0);
    unsigned int throttle = m_dg();
    if (throttle == 0 || throttle == ~0u) {
        m_read = 0;
        return parent()->read(b, len);
    }
    unsigned long long now = TimerManager::now();
    unsigned long long minTime = 1000000ull * (m_read * 8) / throttle;
    unsigned long long actualTime = (now - m_readTimestamp);
    MORDOR_LOG_DEBUG(g_log) << this << " read " << m_read << "B throttle "
        << throttle << "bps now " << now << "us last " << m_readTimestamp
        << "us min" << minTime << " us actual " << actualTime << "us";
    if (actualTime < minTime) {
        unsigned long long sleepTime = minTime - actualTime;
        // Never sleep for longer than a tenth of a second
        sleepTime = (std::max)(100000ull, sleepTime);
        if (m_timerManager)
            sleep(*m_timerManager, sleepTime);
        else
            sleep(sleepTime);
        m_readTimestamp = TimerManager::now();
    } else {
        m_readTimestamp = now;
    }
    // Aim for no more than a tenth of a second's worth of data
    len = std::min<size_t>(throttle / 8 / 10, len);
    if (len == 0)
        len = 1;
    return m_read = parent()->read(b, len);
}

size_t
ThrottleStream::write(const Buffer &b, size_t len)
{
    MORDOR_ASSERT(len != 0);
    unsigned int throttle = m_dg();
    if (throttle == 0 || throttle == ~0u) {
        m_written = 0;
        return parent()->write(b, len);
    }
    unsigned long long now = TimerManager::now();
    unsigned long long minTime = 1000000ull * (m_written * 8) / throttle;
    unsigned long long actualTime = (now - m_writeTimestamp);
    MORDOR_LOG_DEBUG(g_log) << this << " write " << m_written << "B throttle "
        << throttle << "bps now " << now << "us last " << m_writeTimestamp
        << "us min" << minTime << " us actual " << actualTime << "us";
    if (actualTime < minTime) {
        unsigned long long sleepTime = minTime - actualTime;
        // Never sleep for longer than a tenth of a second
        sleepTime = (std::max)(100000ull, sleepTime);
        if (m_timerManager)
            sleep(*m_timerManager, sleepTime);
        else
            sleep(sleepTime);
        m_writeTimestamp = TimerManager::now();
    } else {
        m_writeTimestamp = now;
    }
    // Aim for no more than a tenth of a second's worth of data
    len = std::min<size_t>(throttle / 8 / 10, len);
    if (len == 0)
        len = 1;
    return m_written = parent()->write(b, len);
}

}
