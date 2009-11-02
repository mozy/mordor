// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include "timer.h"

#include <algorithm>
#include <vector>

#include "assert.h"
#include "exception.h"
#include "log.h"
#include "version.h"

#ifdef OSX
 #include <mach/mach_time.h>
#elif defined(WINDOWS)
 #include <windows.h>  // for LARGE_INTEGER, QueryPerformanceFrequency()
#else
 #include <sys/time.h>
 #include <time.h>
#endif

namespace Mordor {

static Logger::ptr g_log = Log::lookup("mordor:timer");

#ifdef WINDOWS
static unsigned long long queryFrequency()
{
    LARGE_INTEGER frequency;
    BOOL bRet = QueryPerformanceFrequency(&frequency);
    MORDOR_ASSERT(bRet);
    return (unsigned long long)frequency.QuadPart;
}

unsigned long long g_frequency = queryFrequency();
#elif defined (OSX)
static mach_timebase_info_data_t queryTimebase()
{
    mach_timebase_info_data_t timebase;
    mach_timebase_info(&timebase);
    return timebase;
}
mach_timebase_info_data_t g_timebase = queryTimebase();
#endif

unsigned long long
TimerManager::now()
{
#ifdef WINDOWS
    LARGE_INTEGER count;
    if (!QueryPerformanceCounter(&count))
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("QueryPerformanceCounter");
    unsigned long long countUll = (unsigned long long)count.QuadPart;
    return countUll * 1000000 / g_frequency;
#elif defined(OSX)
    unsigned long long absoluteTime = mach_absolute_time();
    return absoluteTime * g_timebase.numer / g_timebase.denom / 1000;
#else
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts))
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("clock_gettime");
    return ts.tv_sec * 1000000ull + ts.tv_nsec / 1000;
#endif
}

Timer::Timer(unsigned long long us, boost::function<void ()> dg, bool recurring,
             TimerManager *manager)
    : m_us(us),
      m_dg(dg),
      m_recurring(recurring),
      m_manager(manager)
{
    MORDOR_ASSERT(m_dg);
    m_next = TimerManager::now() + m_us;
}

Timer::Timer(unsigned long long next)
    : m_next(next)
{}

void
Timer::cancel()
{
    MORDOR_LOG_VERBOSE(g_log) << this << " cancel";
    if (m_next != 0) {
        boost::mutex::scoped_lock lock(m_manager->m_mutex);
        std::set<Timer::ptr, Timer::Comparator>::iterator it =
            m_manager->m_timers.find(shared_from_this());
        MORDOR_ASSERT(it != m_manager->m_timers.end());
        m_next = 0;
        m_manager->m_timers.erase(it);
    }
}

TimerManager::~TimerManager()
{
#ifndef NDEBUG
    boost::mutex::scoped_lock lock(m_mutex);
    MORDOR_ASSERT(m_timers.empty());
#endif
}

Timer::ptr
TimerManager::registerTimer(unsigned long long us, boost::function<void ()> dg,
        bool recurring)
{
    bool atFront;
    return registerTimer(us, dg, recurring, atFront);
}

Timer::ptr
TimerManager::registerTimer(unsigned long long us, boost::function<void ()> dg,
        bool recurring, bool &atFront)
{
    Timer::ptr result(new Timer(us, dg, recurring, this));
    {
        boost::mutex::scoped_lock lock(m_mutex);
        atFront = (m_timers.insert(result).first == m_timers.begin());
    }
    MORDOR_LOG_VERBOSE(g_log) << result.get() << " registerTimer(" << us
        << ", " << us << ", " << recurring << "): " << atFront;
    return result;
}

unsigned long long
TimerManager::nextTimer()
{
    boost::mutex::scoped_lock lock(m_mutex);
    if (m_timers.empty()) {
        MORDOR_LOG_VERBOSE(g_log) << this << " nextTimer(): ~0ull";
        return ~0ull;
    }
    const Timer::ptr &next = *m_timers.begin();
    unsigned long long nowUs = now();
    unsigned long long result;
    if (nowUs >= next->m_next)
        result = 0;
    else
        result = next->m_next - nowUs;
    MORDOR_LOG_VERBOSE(g_log) << this << " nextTimer(): " << result;
    return result;
}

static
void delete_nothing(Timer *t)
{}

void
TimerManager::processTimers()
{
    std::vector<Timer::ptr> expired;
    unsigned long long nowUs = now();
    {
        boost::mutex::scoped_lock lock(m_mutex);
        if (m_timers.empty() || (*m_timers.begin())->m_next > nowUs)
            return;
        Timer nowTimer(nowUs);
        Timer::ptr nowTimerPtr(&nowTimer, &delete_nothing);
        // Find all timers that are expired
        std::set<Timer::ptr, Timer::Comparator>::iterator it =
            m_timers.lower_bound(nowTimerPtr);
        while (it != m_timers.end() && (*it)->m_next == nowUs ) ++it;
        // Copy to expired, remove from m_timers;
        expired.insert(expired.begin(), m_timers.begin(), it);
        m_timers.erase(m_timers.begin(), it);
        // Look at expired timers and re-register recurring timers
        // (while under the same lock)
        for (std::vector<Timer::ptr>::iterator it2(expired.begin());
            it2 != expired.end();
            ++it2) {
            Timer::ptr &timer = *it2;
            if (timer->m_recurring) {
                timer->m_next = nowUs + timer->m_us;
                m_timers.insert(timer);
            }
        }                        
    }
    // Run the callbacks for each expired timer (not under a lock)
    for (std::vector<Timer::ptr>::iterator it2(expired.begin());
        it2 != expired.end();
        ++it2) {
        Timer::ptr &timer = *it2;
        // Make sure someone else hasn't cancelled us
        // TODO: need a per-timer lock for this?
        if (timer->m_next != 0) {
            if (!timer->m_recurring) timer->m_next = 0;
            MORDOR_LOG_VERBOSE(g_log) << timer.get() << " dg()";
            timer->m_dg();
        }
    }
}

bool
Timer::Comparator::operator()(const Timer::ptr &lhs,
                              const Timer::ptr &rhs) const
{
    // Order NULL before everything else
    if (!lhs && !rhs)
        return false;
    if (!lhs)
        return true;
    if (!rhs)
        return false;
    // Order primarily on m_next
    if (lhs->m_next < rhs->m_next)
        return true;
    if (rhs->m_next < lhs->m_next)
        return false;
    // Order by raw pointer for equivalent timeout values
    return lhs.get() < rhs.get();
}

}
