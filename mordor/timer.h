#ifndef __MORDOR_TIMER_H__
#define __MORDOR_TIMER_H__
// Copyright (c) 2009 - Decho Corp.

#include <set>

#include <boost/enable_shared_from_this.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>

namespace Mordor {

class TimerManager;

class Timer : public boost::noncopyable, public boost::enable_shared_from_this<Timer>
{
    friend class TimerManager;
public:
    typedef boost::shared_ptr<Timer> ptr;

private:
    Timer(unsigned long long us, boost::function<void ()> dg,
        bool recurring, TimerManager *manager);
    // Constructor for dummy object
    Timer(unsigned long long next);

public:
    void cancel();

private:
    unsigned long long m_next;
    unsigned long long m_us;
    boost::function<void ()> m_dg;
    bool m_recurring;
    TimerManager *m_manager;

private:
    struct Comparator
    {
        bool operator()(const Timer::ptr &lhs, const Timer::ptr &rhs) const;
    };

};

class TimerManager : public boost::noncopyable
{
    friend class Timer;
public:
    virtual ~TimerManager();

    virtual Timer::ptr registerTimer(unsigned long long us,
        boost::function<void ()> dg, bool recurring = false);

    // How *long* until the next timer expires; ~0ull if no timers
    unsigned long long nextTimer();
    void processTimers();

    // Return monotonically increasing count of microseconds.  The number returned
    // isn't guaranteed to be relative to any particular start time, however,
    // the difference between two successive calls to now() should be
    // equal to the time that elapsed between calls.  This should be true
    // even if the system clock is changed.
    static unsigned long long now();

protected:
    Timer::ptr registerTimer(unsigned long long us, boost::function<void ()> dg,
        bool recurring, bool &atFront);

private:
    std::set<Timer::ptr, Timer::Comparator> m_timers;
    boost::mutex m_mutex;
};

}

#endif
