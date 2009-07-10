// Copyright (c) 2009 - Decho Corp.

#include "timer.h"

#include <algorithm>
#include <cassert>
#include <vector>

#include "mordor/common/exception.h"
#include "mordor/common/version.h"

#ifndef WINDOWS
#include <sys/time.h>
#include <time.h>
#endif

#ifdef WINDOWS
static unsigned long long queryFrequency()
{
    LARGE_INTEGER frequency;
    BOOL bRet = QueryPerformanceFrequency(&frequency);
    assert(bRet);
    return (unsigned long long)frequency.QuadPart;
}

unsigned long long g_frequency = queryFrequency();
#endif

// Return monotonically increasing count of microseconds.  The number returned
// isn't guaranteed to be relative to any particular start time, however,
// the difference between two successive calls to nowUs() should be
// equal to the time that elapsed between calls.  This should be true
// even if the system clock is changed.

static
unsigned long long nowUs()
{
#ifdef WINDOWS
    LARGE_INTEGER count;
    if (!QueryPerformanceCounter(&count))
        throwExceptionFromLastError();
    unsigned long long countUll = (unsigned long long)count.QuadPart;
    return countUll * g_frequency / 1000 / 1000;
#else
    // the invariant described above is not really true.  The
    // current implementation is using gettimeofday and should be changed to
    // use the timestamp counters, with frequency adjustment based on the
    // tsc_quotient provided by the kernel.
    //
    // Doing it that way has the added benefit that we can tell time using an
    // ultrafast method that takes about 100 cycles rather than the ~10,000
    // required to make the gettimeofday system call.

    struct timeval tv;

    gettimeofday(&tv, NULL);
#ifdef LINUX
    // To work around a kernel bug where gettimeofday periodically jumps 4398s
    // into the future, call gettimeofday() twice and return the lower of the
    // two. This doubles the performance suckiness of this call.
    //
    // See http://kerneltrap.org/mailarchive/linux-kernel/2007/8/23/163943
    //
    unsigned long long v1 = tv.tv_sec * 1000000ull + tv.tv_usec;

    gettimeofday(&tv, NULL);
    unsigned long long v2 = tv.tv_sec * 1000000ull + tv.tv_usec;

    return v1 < v2 ? v1 : v2;
#else
    return tv.tv_sec * 1000000ull + tv.tv_usec;
#endif

#endif
}

Timer::Timer(unsigned long long us, boost::function<void ()> dg, bool recurring,
             TimerManager *manager)
    : m_us(us),
      m_dg(dg),
      m_recurring(recurring),
      m_manager(manager)
{
    assert(m_dg);
    m_next = nowUs() + m_us;
}

Timer::Timer(unsigned long long next)
    : m_next(next)
{}

void
Timer::cancel()
{
    if (m_next != 0) {
        boost::mutex::scoped_lock lock(m_manager->m_mutex);
        std::set<Timer::ptr, Timer::Comparator>::iterator it =
            m_manager->m_timers.find(shared_from_this());
        assert(it != m_manager->m_timers.end());
        m_next = 0;
        m_manager->m_timers.erase(it);
    }
}

TimerManager::~TimerManager()
{
#ifndef NDEBUG
    boost::mutex::scoped_lock lock(m_mutex);
    assert(m_timers.empty());
#endif
}

Timer::ptr
TimerManager::registerTimer(unsigned long long us, boost::function<void ()> dg,
        bool recurring)
{
    Timer::ptr result(new Timer(us, dg, recurring, this));
    boost::mutex::scoped_lock lock(m_mutex);
    m_timers.insert(result);
    return result;
}

unsigned long long
TimerManager::nextTimer()
{
    boost::mutex::scoped_lock lock(m_mutex);
    if (m_timers.empty())
        return ~0ull;
    const Timer::ptr &next = *m_timers.begin();
    unsigned long long now = nowUs();
    if (now >= next->m_next)
        return 0;
    return next->m_next - now;
}

static
void delete_nothing(Timer *t)
{}

void
TimerManager::processTimers()
{
    std::vector<Timer::ptr> expired;
    unsigned long long now = nowUs();
    {
        boost::mutex::scoped_lock lock(m_mutex);
        if (m_timers.empty() || (*m_timers.begin())->m_next > now)
            return;
        Timer nowTimer(now);
        Timer::ptr nowTimerPtr(&nowTimer, &delete_nothing);
        // Find all timers that are expired
        std::set<Timer::ptr, Timer::Comparator>::iterator it =
            m_timers.lower_bound(nowTimerPtr);
        while (it != m_timers.end() && (*it)->m_next == now ) ++it;
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
                timer->m_next = now + timer->m_us;
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
