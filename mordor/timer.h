#ifndef __MORDOR_TIMER_H__
#define __MORDOR_TIMER_H__
// Copyright (c) 2009 - Mozy, Inc.

#include <set>
#include <vector>

#include <boost/enable_shared_from_this.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/weak_ptr.hpp>

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
    /// @return If the timer was successfully cancelled before it fired
    /// (if non-recurring)
    bool cancel();

    /// Refresh the timer from now
    /// @return If it was refreshed before firing
    bool refresh();
    /// Reset the timer to the new delay
    /// @param us The delay
    /// @param fromNow If us should be relative to now, or the original
    /// starting point
    /// @return If it was reset before firing
    bool reset(unsigned long long us, bool fromNow);

private:
    bool m_recurring;
    unsigned long long m_next;
    unsigned long long m_us;
    boost::function<void ()> m_dg;
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
    TimerManager();
    virtual ~TimerManager();

    virtual Timer::ptr registerTimer(unsigned long long us,
        boost::function<void ()> dg, bool recurring = false);

    /// Conditionally execute the dg callback function only when weakCond is
    /// still in valid status, which means, the original object managed by the
    /// shared_ptr is not destroyed
    /// NOTE: this interface can't be called in class constructor while passing
    ///  a shared_ptr/weak_ptr of itself.
    Timer::ptr registerConditionTimer(unsigned long long us,
        boost::function<void ()> dg,
        boost::weak_ptr<void> weakCond,
        bool recurring = false);

    /// @return How long until the next timer expires; ~0ull if no timers
    unsigned long long nextTimer();
    void executeTimers();

    /// @return Monotonically increasing count of microseconds.  The number
    /// returned isn't guaranteed to be relative to any particular start time,
    /// however, the difference between two successive calls to now() is
    /// equal to the time that elapsed between calls.  This is true even if the
    /// system clock is changed.
    static unsigned long long now();

    /// replace the built-in clock
    /// @param dg replacement function whose value will be returned by now()
    /// omit the parameter to return to the default clock.
    /// NOTE: as now() is static, this affects *all* TimerManager instances
    static void setClock(boost::function<unsigned long long()> dg = NULL);

protected:
    virtual void onTimerInsertedAtFront() {}
    std::vector<boost::function<void ()> > processTimers();

private:
    static boost::function<unsigned long long ()> ms_clockDg;
    bool detectClockRollover(unsigned long long nowUs);
    std::set<Timer::ptr, Timer::Comparator> m_timers;
//    boost::mutex m_mutex;
    bool m_tickled;
    unsigned long long m_previousTime;
};

}

#endif
