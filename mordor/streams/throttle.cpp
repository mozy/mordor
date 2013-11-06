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
        if (m_read > 0){
            m_read = 0;
            MORDOR_LOG_DEBUG(g_log) << this << " no longer throttling on read";
        }
        MORDOR_LOG_DEBUG(g_log) << this << " read " << len << "B unthrottled ";
        return parent()->read(b, len);
    }
    unsigned long long now = TimerManager::now();
    unsigned long long minTime = 1000000ull * (m_read * 8) / throttle;
    unsigned long long actualTime = (now - m_readTimestamp);
    if (m_read == 0) {
        MORDOR_LOG_DEBUG(g_log) << this << " starting throttling on read " << throttle << "bps actual " << actualTime << "us";
    } else {
        MORDOR_LOG_DEBUG(g_log) << this << " read " << m_read << "B throttle "
            << throttle << "bps now " << now << "us last " << m_readTimestamp
            << "us min" << minTime << " us actual " << actualTime << "us";
        if (actualTime < minTime) {
            unsigned long long sleepTime = minTime - actualTime;
            // Never sleep for longer than a tenth of a second
            sleepTime = (std::min)(100000ull, sleepTime);
            if (m_timerManager)
                sleep(*m_timerManager, sleepTime);
            else
                sleep(sleepTime);
        }
    }
    // Aim for no more than a tenth of a second's worth of data
    len = std::min<size_t>(throttle / 8 / 10, len);
    if (len == 0)
        len = 1;
    m_readTimestamp = TimerManager::now();
    return m_read = parent()->read(b, len);
}

size_t
ThrottleStream::write(const Buffer &b, size_t len)
{
    MORDOR_ASSERT(len != 0);
    unsigned int throttle = m_dg();
    if (throttle == 0 || throttle == ~0u) {
        if (m_written > 0){
            m_written = 0;
            MORDOR_LOG_DEBUG(g_log) << this << " no longer throttling";
        }
        MORDOR_LOG_DEBUG(g_log) << this << " write " << len << "B unthrottled ";
        m_writeTimestamp = TimerManager::now();
        return parent()->write(b, len);
    }
    unsigned long long now = TimerManager::now();
    unsigned long long minTime = 1000000ull * (m_written * 8) / throttle;
    unsigned long long actualTime = (now - m_writeTimestamp);
    if (m_written == 0) {
        MORDOR_LOG_DEBUG(g_log) << this << " starting throttling " << throttle << "bps actual " << actualTime << "us";
    } else {
        MORDOR_LOG_DEBUG(g_log) << this << " write " << m_written << "B throttle "
            << throttle << "bps now " << now << "us last " << m_writeTimestamp
            << "us min " << minTime << "us actual " << actualTime << "us";
        if (actualTime < minTime) {
            unsigned long long sleepTime = minTime - actualTime;
            // Never sleep for longer than a tenth of a second
            sleepTime = (std::min)(100000ull, sleepTime);
            if (m_timerManager)
                sleep(*m_timerManager, sleepTime);
            else
                sleep(sleepTime);
        }
    }
    // Aim for no more than a tenth of a second's worth of data
    len = std::min<size_t>(throttle / 8 / 10, len);
    if (len == 0)
        len = 1;
    // set timestamp to now. This needs to include the sleep time above - with multiple connections per host, we may be doing context switching when we sleep.
    m_writeTimestamp = TimerManager::now();
    return m_written = parent()->write(b, len);
}

}
