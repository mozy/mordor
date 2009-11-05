// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include "throttle.h"

#include "mordor/log.h"
#include "mordor/sleep.h"
#include "mordor/timer.h"

namespace Mordor {

static Logger::ptr g_log = Log::lookup("mordor:streams:throttle");

size_t
ThrottleStream::read(Buffer &b, size_t len)
{
    unsigned int throttle = m_dg();
    unsigned long long now = TimerManager::now();
    unsigned long long minTime
        = throttle ? 1000000ull * (m_read * 8) / throttle : 0;
    unsigned long long actualTime = (now - m_readTimestamp);
    MORDOR_LOG_DEBUG(g_log) << this << " read " << m_read << "B throttle "
        << throttle << "bps now " << now << "us last " << m_readTimestamp
        << "us min" << minTime << " us actual " << actualTime << "us";
    if (actualTime < minTime) {
        unsigned long long sleepTime = minTime - actualTime;
        if (m_timerManager)
            sleep(*m_timerManager, sleepTime);
        else
            sleep(sleepTime);
        m_readTimestamp = TimerManager::now();
    } else {
        m_readTimestamp = now;
    }
    // Aim for no more than a 10th of a second
    len = std::min<size_t>(throttle / 8 / 10, len);
    size_t result = parent()->read(b, len);
    m_read = result;
    return result;
}

size_t
ThrottleStream::write(const Buffer &b, size_t len)
{
    unsigned int throttle = m_dg();
    unsigned long long now = TimerManager::now();
    unsigned long long minTime =
        throttle ? 1000000ull * (m_written * 8) / throttle : 0;
    unsigned long long actualTime = (now - m_writeTimestamp);
    MORDOR_LOG_DEBUG(g_log) << this << " write " << m_written << "B throttle "
        << throttle << "bps now " << now << "us last " << m_writeTimestamp
        << "us min" << minTime << " us actual " << actualTime << "us";
    if (actualTime < minTime) {
        unsigned long long sleepTime = minTime - actualTime;
        if (m_timerManager)
            sleep(*m_timerManager, sleepTime);
        else
            sleep(sleepTime);
        m_writeTimestamp = TimerManager::now();
    } else {
        m_writeTimestamp = now;
    }
    // Aim for no more than a 10th of a second
    len = std::min<size_t>(throttle / 8 / 10, len);
    size_t result = parent()->write(b, len);
    m_read = result;
    return result;
}

}
