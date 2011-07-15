// Copyright (c) 2009 - Mozy, Inc.

#include "timer.h"

#include <algorithm>
#include <vector>

#include "assert.h"
#include "atomic.h"
#include "exception.h"
#include "log.h"
#include "version.h"
#include "util.h"
#include "config.h"

#ifdef OSX
 #include <mach/mach_time.h>
#elif defined(WINDOWS)
 #include <windows.h>  // for LARGE_INTEGER, QueryPerformanceFrequency()
#else
 #include <sys/time.h>
 #include <time.h>
#endif

namespace Mordor {

boost::function<unsigned long long ()> TimerManager::ms_clockDg;

static Logger::ptr g_log = Log::lookup("mordor:timer");

// now() doesn't roll over at 0xffff...., because underlying hardware timers
// do not count in microseconds; also, prevent clock anomalies from causing
// timers that will never expire
static ConfigVar<unsigned long long>::ptr g_clockRolloverThreshold =
    Config::lookup<unsigned long long>("timer.clockrolloverthreshold", 5000000ULL,
    "Expire all timers if the clock goes backward by >= this amount");

static void
stubOnTimer(boost::weak_ptr<void> weakCond, boost::function<void ()> dg);

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
    if (ms_clockDg)
        return ms_clockDg();

#ifdef WINDOWS
    LARGE_INTEGER count;
    if (!QueryPerformanceCounter(&count))
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("QueryPerformanceCounter");
    unsigned long long countUll = (unsigned long long)count.QuadPart;
    if (g_frequency == 0)
        g_frequency = queryFrequency();
    return muldiv64(countUll, 1000000, g_frequency);
#elif defined(OSX)
    unsigned long long absoluteTime = mach_absolute_time();
    if (g_timebase.denom == 0)
        g_timebase = queryTimebase();
    return muldiv64(absoluteTime, g_timebase.numer, (uint64_t)g_timebase.denom * 1000);
#else
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts))
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("clock_gettime");
    return ts.tv_sec * 1000000ull + ts.tv_nsec / 1000;
#endif
}

Timer::Timer(unsigned long long us, boost::function<void ()> dg, bool recurring,
             TimerManager *manager)
    : m_recurring(recurring),
      m_us(us),
      m_dg(dg),
      m_manager(manager)
{
    MORDOR_ASSERT(m_dg);
    m_next = TimerManager::now() + m_us;
}

Timer::Timer(unsigned long long next)
    : m_next(next)
{}

bool
Timer::cancel()
{
    MORDOR_LOG_DEBUG(g_log) << this << " cancel";
    boost::mutex::scoped_lock lock(m_manager->m_mutex);
    if (m_dg) {
        m_dg = NULL;
        std::set<Timer::ptr, Timer::Comparator>::iterator it =
            m_manager->m_timers.find(shared_from_this());
        MORDOR_ASSERT(it != m_manager->m_timers.end());
        m_manager->m_timers.erase(it);
        return true;
    }
    return false;
}

bool
Timer::refresh()
{
    boost::mutex::scoped_lock lock(m_manager->m_mutex);
    if (!m_dg)
        return false;
    std::set<Timer::ptr, Timer::Comparator>::iterator it =
        m_manager->m_timers.find(shared_from_this());
    MORDOR_ASSERT(it != m_manager->m_timers.end());
    m_manager->m_timers.erase(it);
    m_next = TimerManager::now() + m_us;
    m_manager->m_timers.insert(shared_from_this());
    lock.unlock();
    MORDOR_LOG_DEBUG(g_log) << this << " refresh";
    return true;
}

bool
Timer::reset(unsigned long long us, bool fromNow)
{
    boost::mutex::scoped_lock lock(m_manager->m_mutex);
    if (!m_dg)
        return false;
    // No change
    if (us == m_us && !fromNow)
        return true;
    std::set<Timer::ptr, Timer::Comparator>::iterator it =
        m_manager->m_timers.find(shared_from_this());
    MORDOR_ASSERT(it != m_manager->m_timers.end());
    m_manager->m_timers.erase(it);
    unsigned long long start;
    if (fromNow)
        start = TimerManager::now();
    else
        start = m_next - m_us;
    m_us = us;
    m_next = start + m_us;
    it = m_manager->m_timers.insert(shared_from_this()).first;
    bool atFront = (it == m_manager->m_timers.begin()) && !m_manager->m_tickled;
    if (atFront)
        m_manager->m_tickled = true;
    lock.unlock();
    MORDOR_LOG_DEBUG(g_log) << this << " reset to " << m_us;
    if (atFront)
        m_manager->onTimerInsertedAtFront();
    return true;
}

TimerManager::TimerManager()
: m_tickled(false),
  m_previousTime(0ull)
{}

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
    MORDOR_ASSERT(dg);
    Timer::ptr result(new Timer(us, dg, recurring, this));
    boost::mutex::scoped_lock lock(m_mutex);
    std::set<Timer::ptr, Timer::Comparator>::iterator it =
        m_timers.insert(result).first;
    bool atFront = (it == m_timers.begin()) && !m_tickled;
    if (atFront)
        m_tickled = true;
    lock.unlock();
    MORDOR_LOG_DEBUG(g_log) << result.get() << " registerTimer(" << us
        << ", " << recurring << "): " << atFront;
    if (atFront)
        onTimerInsertedAtFront();
    return result;
}

Timer::ptr
TimerManager::registerConditionTimer(unsigned long long us,
    boost::function<void ()> dg,
    boost::weak_ptr<void> weakCond,
    bool recurring)
{
    return registerTimer(us,
        boost::bind(stubOnTimer, weakCond, dg),
        recurring);
}

static void
stubOnTimer(
    boost::weak_ptr<void> weakCond, boost::function<void ()> dg)
{
    if (weakCond.lock()) {
        dg();
    }
    else {
        MORDOR_LOG_DEBUG(g_log) << " Conditionally skip in stubOnTimer!";
    }
}

unsigned long long
TimerManager::nextTimer()
{
    boost::mutex::scoped_lock lock(m_mutex);
    m_tickled = false;
    if (m_timers.empty()) {
        MORDOR_LOG_DEBUG(g_log) << this << " nextTimer(): ~0ull";
        return ~0ull;
    }
    const Timer::ptr &next = *m_timers.begin();
    unsigned long long nowUs = now();
    unsigned long long result;
    if (nowUs >= next->m_next)
        result = 0;
    else
        result = next->m_next - nowUs;
    MORDOR_LOG_DEBUG(g_log) << this << " nextTimer(): " << result;
    return result;
}

bool
TimerManager::detectClockRollover(unsigned long long nowUs)
{
    // If the time jumps backward, expire timers (rather than have them
    // expire in the distant future or not at all).
    // We check this way because now() will not roll from 0xffff... to zero
    // since the underlying hardware counter doesn't count microseconds.
    // Use a threshold value so we don't overreact to minor clock jitter.
    bool rollover = false;
    if (nowUs < m_previousTime && // check first in case the next line would underflow
        nowUs < m_previousTime - g_clockRolloverThreshold->val())
    {
        MORDOR_LOG_ERROR(g_log) << this << " clock has rolled back from "
            << m_previousTime << " to " << nowUs << "; expiring all timers";
        rollover = true;
    }
    m_previousTime = nowUs;
    return rollover;
}

std::vector<boost::function<void ()> >
TimerManager::processTimers()
{
    std::vector<Timer::ptr> expired;
    std::vector<boost::function<void ()> > result;
    unsigned long long nowUs = now();
    {
        boost::mutex::scoped_lock lock(m_mutex);
        if (m_timers.empty())
            return result;
        bool rollover = detectClockRollover(nowUs);
        if (!rollover && (*m_timers.begin())->m_next > nowUs)
            return result;
        Timer nowTimer(nowUs);
        Timer::ptr nowTimerPtr(&nowTimer, &nop<Timer *>);
        // Find all timers that are expired
        std::set<Timer::ptr, Timer::Comparator>::iterator it =
            rollover ? m_timers.end() : m_timers.lower_bound(nowTimerPtr);
        while (it != m_timers.end() && (*it)->m_next == nowUs ) ++it;
        // Copy to expired, remove from m_timers;
        expired.insert(expired.begin(), m_timers.begin(), it);
        m_timers.erase(m_timers.begin(), it);
        result.reserve(expired.size());
        // Look at expired timers and re-register recurring timers
        // (while under the same lock)
        for (std::vector<Timer::ptr>::iterator it2(expired.begin());
            it2 != expired.end();
            ++it2) {
            Timer::ptr &timer = *it2;
            MORDOR_ASSERT(timer->m_dg);
            result.push_back(timer->m_dg);
            if (timer->m_recurring) {
                MORDOR_LOG_TRACE(g_log) << timer << " expired and refreshed";
                timer->m_next = nowUs + timer->m_us;
                m_timers.insert(timer);
            } else {
                MORDOR_LOG_TRACE(g_log) << timer << " expired";
                timer->m_dg = NULL;
            }
        }
    }
    return result;
}

void
TimerManager::executeTimers()
{
    std::vector<boost::function<void ()> > expired = processTimers();
    // Run the callbacks for each expired timer (not under a lock)
    for (std::vector<boost::function<void ()> >::iterator it(expired.begin());
        it != expired.end();
        ++it) {
        (*it)();
    }
}

void
TimerManager::setClock(boost::function<unsigned long long()> dg)
{
    ms_clockDg = dg;
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
