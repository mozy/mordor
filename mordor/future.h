#ifndef __MORDOR_FUTURE_H__
#define __MORDOR_FUTURE_H__
// Copyright (c) 2009 - Decho Corporation

#include <bitset>

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>

#include "assert.h"
#include "atomic.h"
#include "fiber.h"
#include "scheduler.h"

namespace Mordor {

struct Void;

template <class T = Void>
class Future : boost::noncopyable
{
    template <class Iterator>
    friend void waitAll(Iterator start, Iterator end);
    template <class Iterator>
    friend size_t waitAny(Iterator start, Iterator end);
public:
    Future(boost::function<void (const T &)> dg = NULL, Scheduler *scheduler = NULL)
        : m_fiber(0),
          m_scheduler(scheduler),
          m_dg(dg),
          m_t()
    {
        MORDOR_ASSERT(dg || !scheduler);
        if (m_dg)
            m_fiber = 0x2;
    }

    /// Suspend this Fiber, and wait for the future to become signalled
    T &wait()
    {
        MORDOR_ASSERT(!m_scheduler);
        MORDOR_ASSERT(!m_dg);
        m_scheduler = Scheduler::getThis();
        intptr_t newValue = (intptr_t)Fiber::getThis().get();
        MORDOR_ASSERT(!(newValue & 0x1));
        intptr_t currentValue = atomicCompareAndSwap(m_fiber, newValue, (intptr_t)0);
        // Not signalled yet
        if (currentValue == 0) {
            Scheduler::yieldTo();
            // Make sure we got signalled
            MORDOR_ASSERT(m_fiber & 0x1);
        } else if (currentValue == 0x1) {
            // Already signalled; just return
        } else {
            MORDOR_NOTREACHED();
        }
        return m_t;
    }

    /// For signallers to set the result; once signalled should not be modified
    T& result() { MORDOR_ASSERT(!(m_fiber & 0x1)); return m_t; }

    /// Signal this Future
    void signal()
    {
        intptr_t newValue = m_fiber, oldValue;
        if (newValue == 0x2) {
            MORDOR_ASSERT(m_dg);
            if (m_scheduler)
                m_scheduler->schedule(boost::bind(m_dg, boost::cref(m_t)));
            else
                m_dg(m_t);
            return;
        }
        do {
            oldValue = newValue;
            newValue = oldValue | 0x1;
        } while ( (newValue = atomicCompareAndSwap(m_fiber, newValue, oldValue)) != oldValue);
        newValue &= ~0x1;
        // Somebody was previously waiting
        if (newValue) {
            MORDOR_ASSERT(m_scheduler);
            MORDOR_ASSERT(!m_dg);
            m_scheduler->schedule(((Fiber *)newValue)->shared_from_this());
        }
    }

    void reset()
    {
        m_fiber = 0;
        if (!m_dg)
            m_scheduler = NULL;
    }

private:
    // We're going to stuff a couple of things into m_fiber, and do some bit
    // manipulation, so it's going to be easier to declare it as intptr_t
    // m_fiber = NULL if not signalled, and not waiting
    // m_fiber & 0x1 if signalled
    // m_fiber & ~0x1 if waiting
    // Note that m_fiber will *not* point to a fiber if m_dg is valid
    intptr_t m_fiber;
    Scheduler *m_scheduler;
    boost::function<void (const T &)> m_dg;
    T m_t;
};

template <>
class Future<Void> : boost::noncopyable
{
    template <class Iterator>
    friend void waitAll(Iterator start, Iterator end);
    template <class Iterator>
    friend size_t waitAny(Iterator start, Iterator end);
public:
    Future(boost::function<void ()> dg = NULL, Scheduler *scheduler = NULL)
        : m_fiber(0),
          m_scheduler(scheduler),
          m_dg(dg)
    {
        MORDOR_ASSERT(m_dg || !scheduler);
        if (m_dg)
            m_fiber = 0x2;
    }

    /// Suspend this Fiber, and wait for the future to become signalled
    void wait()
    {
        MORDOR_ASSERT(!m_scheduler);
        MORDOR_ASSERT(!m_dg);
        m_scheduler = Scheduler::getThis();
        intptr_t newValue = (intptr_t)Fiber::getThis().get();
        MORDOR_ASSERT(!(newValue & 0x1));
        intptr_t currentValue = atomicCompareAndSwap(m_fiber, newValue, (intptr_t)0);
        // Not signalled yet
        if (currentValue == 0) {
            Scheduler::yieldTo();
            // Make sure we got signalled
            MORDOR_ASSERT(m_fiber & 0x1);
        } else if (currentValue == 0x1) {
            // Already signalled; just return
        } else {
            MORDOR_NOTREACHED();
        }
    }

    /// Signal this Future
    void signal()
    {
        intptr_t newValue = m_fiber, oldValue;
        if (newValue == 0x2) {
            MORDOR_ASSERT(m_dg);
            if (m_scheduler)
                m_scheduler->schedule(m_dg);
            else
                m_dg();
            return;
        }
        do {
            oldValue = newValue;
            newValue = oldValue | 0x1;
        } while ( (newValue = atomicCompareAndSwap(m_fiber, newValue, oldValue)) != oldValue);
        newValue &= ~0x1;
        // Somebody was previously waiting
        if (newValue) {
            MORDOR_ASSERT(m_scheduler);
            MORDOR_ASSERT(!m_dg);
            m_scheduler->schedule(((Fiber *)newValue)->shared_from_this());
        }
    }

    void reset()
    {
        m_fiber = 0;
        if (!m_dg)
            m_scheduler = NULL;
    }

private:
    /// @return If the future was already signalled
    bool startWait()
    {
        MORDOR_ASSERT(!m_scheduler);
        MORDOR_ASSERT(!m_dg);
        m_scheduler = Scheduler::getThis();
        intptr_t newValue = (intptr_t)Fiber::getThis().get();
        MORDOR_ASSERT(!(newValue & 0x1));
        intptr_t currentValue = atomicCompareAndSwap(m_fiber, newValue, (intptr_t)0);
        if (currentValue == 0) {
            return false;
        } else if (currentValue == 0x1) {
            return true;
        } else {
            MORDOR_NOTREACHED();
        }
    }

    /// @return If the future was already signalled
    bool cancelWait()
    {
        MORDOR_ASSERT(!m_dg);
        intptr_t oldValue = m_fiber;
        if (oldValue & 1)
            return true;
        oldValue = atomicCompareAndSwap(m_fiber, (intptr_t)0, oldValue);
        if (oldValue & 1)
            return true;
        m_scheduler = NULL;
        return false;
    }

private:
    // We're going to stuff a couple of things into m_fiber, and do some bit
    // manipulation, so it's going to be easier to declare it as intptr_t
    // m_fiber = NULL if not signalled, and not waiting
    // m_fiber & 0x1 if signalled
    // m_fiber & ~0x1 if waiting
    // Note that m_fiber will *not* point to a fiber if m_dg is valid
    intptr_t m_fiber;
    Scheduler *m_scheduler;
    boost::function<void ()> m_dg;
};

template <class Iterator>
void waitAll(Iterator first, Iterator last)
{
    MORDOR_ASSERT(first != last);
    size_t yieldsNeeded = 1;
    for (; first != last; ++first) {
        if (!first->startWait())
            ++yieldsNeeded;
    }
    while (--yieldsNeeded) Scheduler::yieldTo();
}

template <class Iterator>
size_t waitAny(Iterator first, Iterator last)
{
    MORDOR_ASSERT(first != last);
    size_t result = ~0u;
    size_t index = 0u;
    // Optimize first one
    if (first->startWait())
        return 0;
    Iterator it = first;
    ++it;
    while (it != last) {
        ++index;
        if (it->startWait()) {
            --it;
            result = index;
            break;
        }
        ++it;
    }
    size_t yieldsNeeded = 1;
    if (it == last) {
        --yieldsNeeded;
        Scheduler::yieldTo();
        --it;
    }
    while (it != first) {
        if (it->cancelWait()) {
            result = index;
            ++yieldsNeeded;
        }
        --it;
        --index;
    }
    if (it->cancelWait()) {
        result = 0;
        ++yieldsNeeded;
    }
    MORDOR_ASSERT(result != ~0u);
    while (--yieldsNeeded) Scheduler::yieldTo();
    return result;
}

}

#endif
