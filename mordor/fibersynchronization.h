#ifndef __MORDOR_FIBERSYNCHRONIZATION_H__
#define __MORDOR_FIBERSYNCHRONIZATION_H__
// Copyright (c) 2009 - Mozy, Inc.

#include <list>

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>

namespace Mordor {

class Fiber;
class Scheduler;

/// Scheduler based Mutex for Fibers

/// Mutex for use by Fibers that yields to a Scheduler instead of blocking
/// if the mutex cannot be immediately acquired.  It also provides the
/// additional guarantee that it is strictly FIFO, instead of random which
/// Fiber will acquire the mutex next after it is released.
struct FiberMutex : boost::noncopyable
{
    friend struct FiberCondition;
public:
    /// Type that will lock the mutex on construction, and unlock on
    /// destruction
    struct ScopedLock
    {
    public:
        ScopedLock(FiberMutex &mutex)
            : m_mutex(mutex)
        {
            m_mutex.lock();
            m_locked = true;
        }
        ~ScopedLock()
        { unlock(); }

        void lock()
        {
            if (!m_locked) {
                m_mutex.lock();
                m_locked = true;
            }
        }

        void unlock()
        {
            if (m_locked) {
                m_mutex.unlock();
                m_locked = false;
            }
        }

        bool unlockIfNotUnique()
        {
            if (m_locked) {
                if (m_mutex.unlockIfNotUnique()) {
                    m_locked = false;
                    return true;
                } else {
                    return false;
                }
            }
            return true;
        }

    private:
        FiberMutex &m_mutex;
        bool m_locked;
    };

public:
    ~FiberMutex();

    /// @brief Locks the mutex
    /// Note that it is possible for this Fiber to switch threads after this
    /// method, though it is guaranteed to still be on the same Scheduler
    /// @pre Scheduler::getThis() != NULL
    /// @pre Fiber::getThis() does not own this mutex
    /// @post Fiber::getThis() owns this mutex
    void lock();
    /// @brief Unlocks the mutex
    /// @pre Fiber::getThis() owns this mutex
    void unlock();

    /// Unlocks the mutex if there are other Fibers waiting for the mutex.
    /// This is useful if there is extra work should be done if there is no one
    /// else waiting (such as flushing a buffer).
    /// @return If the mutex was unlocked
    bool unlockIfNotUnique();

private:
    void unlockNoLock();

private:
    boost::mutex m_mutex;
    boost::shared_ptr<Fiber> m_owner;
    std::list<std::pair<Scheduler *, boost::shared_ptr<Fiber> > > m_waiters;
};

/// Scheduler based condition variable for Fibers

/// Condition for use by Fibers that yields to a Scheduler instead of blocking.
/// It also provides the additional guarantee that it is strictly FIFO,
/// instead of random which waiting Fiber will be released when the condition
/// is signalled.
struct FiberCondition : boost::noncopyable
{
public:
    /// @param mutex The mutex to associate with the Condition
    FiberCondition(FiberMutex &mutex)
        : m_fiberMutex(mutex)
    {}
    ~FiberCondition();

    /// @brief Wait for the Condition to be signalled
    /// @details
    /// Atomically unlock mutex, and wait for the Condition to be signalled.
    /// Once released, the mutex is locked again.
    /// @pre Scheduler::getThis() != NULL
    /// @pre Fiber::getThis() owns mutex
    /// @post Fiber::getThis() owns mutex
    void wait();
    /// Release a single Fiber from wait()
    void signal();
    /// Release all waiting Fibers
    void broadcast();

private:
    boost::mutex m_mutex;
    FiberMutex &m_fiberMutex;
    std::list<std::pair<Scheduler *, boost::shared_ptr<Fiber> > > m_waiters;
};

/// Scheduler based event variable for Fibers

/// Event for use by Fibers that yields to a Scheduler instead of blocking.
/// It also provides the additional guarantee that it is strictly FIFO,
/// instead of random which waiting Fiber will be released when the event
/// is signalled.
struct FiberEvent : boost::noncopyable
{
public:
    /// @param autoReset If the Event should automatically reset itself
    /// whenever a Fiber is released
    FiberEvent(bool autoReset = true)
        : m_signalled(false),
          m_autoReset(autoReset)
    {}
    ~FiberEvent();

    /// @brief Wait for the Event to become set
    /// @pre Scheduler::getThis() != NULL
    void wait();
    /// Set the Event
    void set();
    /// Reset the Event
    void reset();

private:
    boost::mutex m_mutex;
    bool m_signalled, m_autoReset;
    std::list<std::pair<Scheduler *, boost::shared_ptr<Fiber> > > m_waiters;
};

}

#endif
