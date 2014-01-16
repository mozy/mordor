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

/// Type that will lock the mutex on construction, and unlock on
/// destruction
template<class Mutex> struct ScopedLockImpl
{
public:
    ScopedLockImpl(Mutex &mutex)
        : m_mutex(mutex)
    {
        m_mutex.lock();
        m_locked = true;
    }
    ~ScopedLockImpl()
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
        if (!m_locked) {
            return true;
        }
        if (m_mutex.unlockIfNotUnique()) {
            m_locked = false;
            return true;
        } else {
            return false;
        }
    }

private:
    Mutex &m_mutex;
    bool m_locked;
};

/// Mutex for use by Fibers that yields to a Scheduler instead of blocking
/// if the mutex cannot be immediately acquired.  It also provides the
/// additional guarantee that it is strictly FIFO, instead of random which
/// Fiber will acquire the mutex next after it is released.
struct FiberMutex : boost::noncopyable
{
    friend struct FiberCondition;
public:
    typedef ScopedLockImpl<FiberMutex> ScopedLock;

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

struct RecursiveFiberMutex : boost::noncopyable
{
public:
    typedef ScopedLockImpl<RecursiveFiberMutex> ScopedLock;
public:
    RecursiveFiberMutex() : m_recursion(0) {}
    ~RecursiveFiberMutex();
    /// @brief Locks the mutex
    /// Note that it is possible for this Fiber to switch threads after this
    /// method, though it is guaranteed to still be on the same Scheduler
    /// @pre Scheduler::getThis() != NULL
    /// @post Fiber::getThis() owns this mutex
    void lock();
    /// @brief Unlocks the mutex
    /// @pre Fiber::getThis() owns this mutex
    void unlock();
    /// Unlocks the mutex if there are other Fibers waiting for the mutex.
    /// This is useful if there is extra work should be done if there is no one
    /// else waiting (such as flushing a buffer).
    /// @return If the mutex was unlocked
    /// @note the mutex is not completely released if current fiber is holding
    ///       it muliple times at the mean time
    bool unlockIfNotUnique();

private:
    void unlockNoLock();

private:
    boost::mutex m_mutex;
    boost::shared_ptr<Fiber> m_owner;
    std::list<std::pair<Scheduler *, boost::shared_ptr<Fiber> > > m_waiters;
    unsigned m_recursion;
};

/// Scheduler based Semaphore for Fibers

/// Semaphore for use by Fibers that yields to a Scheduler instead of blocking
/// if the mutex cannot be immediately acquired.  It also provides the
/// additional guarantee that it is strictly FIFO, instead of random which
/// Fiber will acquire the semaphore next after it is released.
struct FiberSemaphore : boost::noncopyable
{
public:
    FiberSemaphore(size_t initialConcurrency = 0);
    ~FiberSemaphore();

    /// @brief Waits for the semaphore
    /// Decreases the amount of concurrency.  If concurrency is already at
    /// zero, it will wait for someone else to notify the semaphore
    /// @note It is possible for this Fiber to switch threads after this
    /// method, though it is guaranteed to still be on the same Scheduler
    /// @pre Scheduler::getThis() != NULL
    void wait();
    /// @brief Increases the level of concurrency
    void notify();

private:
    boost::mutex m_mutex;
    std::list<std::pair<Scheduler *, boost::shared_ptr<Fiber> > > m_waiters;
    size_t m_concurrency;
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
