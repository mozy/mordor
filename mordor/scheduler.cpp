// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/pch.h"

#include "scheduler.h"

#include <boost/bind.hpp>

#include "assert.h"
#include "fiber.h"

namespace Mordor {

static Logger::ptr g_log = Log::lookup("mordor:scheduler");

void
ThreadPool::init(boost::function<void()> proc)
{
    m_proc = proc;
}

void
ThreadPool::start(size_t threads)
{
    MORDOR_ASSERT(m_proc);
    boost::mutex::scoped_lock lock(m_mutex);
    if (!m_threads.empty())
        return;
    for (size_t i = 0; i < threads; ++i)
        m_threads.push_back(
            boost::shared_ptr<boost::thread>(new boost::thread(m_proc)));
}

size_t
ThreadPool::size()
{
    boost::mutex::scoped_lock lock(m_mutex);
    return m_threads.size();
}

void
ThreadPool::join_all()
{
    boost::mutex::scoped_lock lock(m_mutex);
    for (std::list<boost::shared_ptr<boost::thread> >::const_iterator it(m_threads.begin());
        it != m_threads.end();
        ++it)
    {
        if ((*it)->get_id() != boost::this_thread::get_id()) {
            (*it)->join();
        }
    }
    m_threads.clear();
}

ThreadLocalStorage<Scheduler *> Scheduler::t_scheduler;
ThreadLocalStorage<Fiber *> Scheduler::t_fiber;
bool
ThreadPool::contains(boost::thread::id id)
{
    boost::mutex::scoped_lock lock(m_mutex);
    for (std::list<boost::shared_ptr<boost::thread> >::const_iterator it(m_threads.begin());
        it != m_threads.end();
        ++it)
    {
        if ((*it)->get_id() == id)
            return true;
    }
    return false;
}


Scheduler::Scheduler(int threads, bool useCaller, size_t batchSize)
    : m_stopping(true),
      m_autoStop(false),
      m_batchSize(batchSize)
{
    m_threads.init(boost::bind(&Scheduler::run, this));
    if (useCaller)
        MORDOR_ASSERT(threads >= 1);
    if (useCaller) {
        --threads;
        MORDOR_ASSERT(getThis() == NULL);
        t_scheduler = this;
        m_rootFiber.reset(new Fiber(boost::bind(&Scheduler::run, this)));
        t_scheduler = this;
        t_fiber = m_rootFiber.get();
        m_rootThread = boost::this_thread::get_id();
    }
    m_threadCount = threads;
}

Scheduler::~Scheduler()
{
    MORDOR_ASSERT(m_stopping);
    if (getThis() == this) {
        t_scheduler = NULL;
    }
}

Scheduler *
Scheduler::getThis()
{
    return t_scheduler.get();
}

void
Scheduler::start()
{
    MORDOR_LOG_VERBOSE(g_log) << this << " starting " << m_threadCount << " threads";
    MORDOR_ASSERT(m_stopping);
    m_stopping = false;
    m_threads.start(m_threadCount);
}

bool
Scheduler::hasWorkToDo()
{
    boost::mutex::scoped_lock lock(m_mutex);
    return !m_fibers.empty();
}

void
Scheduler::stop()
{
    // Already stopped
    if (m_rootFiber &&
        m_threads.size() == 0 &&
        (m_rootFiber->state() == Fiber::TERM || m_rootFiber->state() == Fiber::INIT)) {
        MORDOR_LOG_VERBOSE(g_log) << this << " stopped";
        m_stopping = true;
        // A derived class may inhibit stopping while it has things to do in
        // its idle loop, so we can't break early
        if (stopping()) {
            boost::mutex::scoped_lock lock(m_mutex);
            if (m_fibers.empty())
                return;
        }
    }

    bool exitOnThisFiber = false;
    if (m_rootThread != boost::thread::id()) {
        // A thread-hijacking scheduler must be stopped
        // from within itself to return control to the
        // original thread
        MORDOR_ASSERT(Scheduler::getThis() == this);
        if (Fiber::getThis() == m_callingFiber) {
            exitOnThisFiber = true;
            // First switch to the correct thread
            MORDOR_LOG_DEBUG(g_log) << this << " switching to root thread to stop";
            switchTo(m_rootThread);
        }
        if (!m_callingFiber)
            exitOnThisFiber = true;
    } else {
        // A spawned-threads only scheduler cannot be stopped from within
        // itself... who would get control?
        MORDOR_ASSERT(Scheduler::getThis() != this);
    }
    m_stopping = true;
    for (size_t i = 0; i < m_threads.size(); ++i) {
        tickle();
    }
    if (m_rootFiber)
        tickle();
    // Wait for all work to stop on this thread
    if (exitOnThisFiber) {
        bool moreWork = t_fiber->state() == Fiber::HOLD || !stopping();
        if (!moreWork) {
            boost::mutex::scoped_lock lock(m_mutex);
            moreWork = !m_fibers.empty();
        }
        while (moreWork) {
            // Give this thread's run fiber a chance to kill itself off
            MORDOR_LOG_DEBUG(g_log) << this << " yielding to this thread to stop";
            yieldTo(true);
            moreWork = t_fiber->state() == Fiber::HOLD || !stopping();
            if (!moreWork) {
                boost::mutex::scoped_lock lock(m_mutex);
                moreWork = !m_fibers.empty();
            }
        }
    }
    // Wait for other threads to stop
    if (exitOnThisFiber ||
        Scheduler::getThis() != this) {
        MORDOR_LOG_DEBUG(g_log) << this << " waiting for other threads to stop";
        m_threads.join_all();
    }
    MORDOR_LOG_VERBOSE(g_log) << this << " stopped";
}

bool
Scheduler::stopping()
{
    return m_stopping;
}

void
Scheduler::schedule(Fiber::ptr f, boost::thread::id thread)
{
    bool tickleMe;
    {
        boost::mutex::scoped_lock lock(m_mutex);
        tickleMe = scheduleNoLock(f, thread);
    }
    if (tickleMe && Scheduler::getThis() != this)
        tickle();
}

void
Scheduler::schedule(boost::function<void ()> dg, boost::thread::id thread)
{
    bool tickleMe;
    {
        boost::mutex::scoped_lock lock(m_mutex);
        tickleMe = scheduleNoLock(dg, thread);
    }
    if (tickleMe && Scheduler::getThis() != this)
        tickle();
}

bool
Scheduler::scheduleNoLock(Fiber::ptr f, boost::thread::id thread)
{
    MORDOR_LOG_DEBUG(g_log) << this << " scheduling " << f << " on thread "
        << thread;
    MORDOR_ASSERT(f);
    // Not thread-targeted, or this scheduler owns the targetted thread
    MORDOR_ASSERT(thread == boost::thread::id() || thread == m_rootThread ||
        m_threads.contains(thread));
    FiberAndThread ft = {f, NULL, thread };
    bool tickleMe = m_fibers.empty();
    m_fibers.push_back(ft);
    return tickleMe;
}

bool
Scheduler::scheduleNoLock(boost::function<void ()> dg, boost::thread::id thread)
{
    MORDOR_LOG_DEBUG(g_log) << this << " scheduling " << dg << " on thread "
        << thread;
    MORDOR_ASSERT(dg);
    // Not thread-targeted, or this scheduler owns the targetted thread
    MORDOR_ASSERT(thread == boost::thread::id() || thread == m_rootThread ||
        m_threads.contains(thread));
    FiberAndThread ft = {Fiber::ptr(), dg, thread };
    bool tickleMe = m_fibers.empty();
    m_fibers.push_back(ft);
    return tickleMe;
}

void
Scheduler::switchTo(boost::thread::id thread)
{
    MORDOR_ASSERT(Scheduler::getThis() != NULL);
    MORDOR_LOG_DEBUG(g_log) << this << " switching to thread " << thread;
    if (Scheduler::getThis() == this) {
        if (thread == boost::thread::id() ||
            thread == boost::this_thread::get_id()) {
            return;
        }
    }
    schedule(Fiber::getThis(), thread);
    Scheduler::yieldTo();
}

void
Scheduler::yieldTo()
{
    Scheduler *self = Scheduler::getThis();
    MORDOR_ASSERT(self);
    MORDOR_LOG_DEBUG(g_log) << self << " yielding to scheduler";
    MORDOR_ASSERT(t_fiber.get());
    if (self->m_rootThread == boost::this_thread::get_id() &&
        (t_fiber->state() == Fiber::INIT || t_fiber->state() == Fiber::TERM)) {
        self->m_callingFiber = Fiber::getThis();
        self->yieldTo(true);
    } else {
        self->yieldTo(false);
    }
}

void
Scheduler::yield()
{
    MORDOR_ASSERT(Scheduler::getThis());
    Scheduler::getThis()->schedule(Fiber::getThis());
    yieldTo();
}

void
Scheduler::dispatch()
{
    MORDOR_LOG_DEBUG(g_log) << this << " dispatching";
    MORDOR_ASSERT(m_rootThread == boost::this_thread::get_id() &&
        m_threads.size() == 0);
    m_stopping = true;
    m_autoStop = true;
    yieldTo();
    m_autoStop = false;
}

void
Scheduler::yieldTo(bool yieldToCallerOnTerminate)
{
    MORDOR_ASSERT(t_fiber.get());
    MORDOR_ASSERT(Scheduler::getThis() == this);
    if (yieldToCallerOnTerminate)
        MORDOR_ASSERT(m_rootThread == boost::this_thread::get_id());
    if (t_fiber->state() != Fiber::HOLD) {
        m_stopping = m_autoStop || m_stopping;
        t_fiber->reset();
    }
    t_fiber->yieldTo(yieldToCallerOnTerminate);
}

void
Scheduler::run()
{
    t_scheduler = this;
    if (boost::this_thread::get_id() != m_rootThread) {
        // Running in own thread
        t_fiber = Fiber::getThis().get();
    } else {
        // Hijacked a thread
        MORDOR_ASSERT(t_fiber.get() == Fiber::getThis().get());
    }
    Fiber::ptr idleFiber(new Fiber(boost::bind(&Scheduler::idle, this)));
    MORDOR_LOG_VERBOSE(g_log) << this << " starting thread with idle fiber " << idleFiber;
    Fiber::ptr dgFiber;
    // use a vector for O(1) .size()
    std::vector<FiberAndThread> batch(m_batchSize);
    bool cameFromIdle = true;
    while (true) {
        batch.clear();
        bool dontIdle = false;
        bool tickleMe = false;
        {
            boost::mutex::scoped_lock lock(m_mutex);
            std::list<FiberAndThread>::iterator it(m_fibers.begin());
            while (it != m_fibers.end()) {
                // If we've met our batch size, and we're not checking to see
                // if we need to tickle another thread, then break
                if ( (!cameFromIdle || tickleMe || threadCount() == 1) &&
                    batch.size() == m_batchSize)
                    break;

                if (it->thread != boost::thread::id() &&
                    it->thread != boost::this_thread::get_id()) {
                    MORDOR_LOG_DEBUG(g_log) << this
                        << " skipping item scheduled for thread "
                        << it->thread;

                    // Wake up another thread to hopefully service this
                    tickleMe = true;
                    dontIdle = true;
                    ++it;
                    continue;
                }
                MORDOR_ASSERT(it->fiber || it->dg);
                if (it->fiber && it->fiber->state() == Fiber::EXEC) {
                    MORDOR_LOG_DEBUG(g_log) << this
                        << " skipping executing fiber " << it->fiber;
                    ++it;
                    dontIdle = true;
                    continue;
                }
                // We were just checking if there is more work; there is, so
                // set the flag and don't actually take this piece of work
                if (batch.size() == m_batchSize) {
                    tickleMe = true;
                    break;
                }
                batch.push_back(*it);
                it = m_fibers.erase(it);
            }
        }
        cameFromIdle = false;
        if (tickleMe)
            tickle();
        MORDOR_LOG_DEBUG(g_log) << this
            << " got " << batch.size() << " fiber/dgs to process (max: "
            << m_batchSize << ")";
        if (!batch.empty()) {
            std::vector<FiberAndThread>::iterator it;
            for (it = batch.begin(); it != batch.end(); ++it) {
                Fiber::ptr f = it->fiber;
                boost::function<void ()> dg = it->dg;

                try {
                    if (f) {
                        if (f->state() != Fiber::TERM) {
                            MORDOR_LOG_DEBUG(g_log) << this << " running " << f;
                            f->yieldTo();
                        }
                    } else if (dg) {
                        if (!dgFiber)
                            dgFiber.reset(new Fiber(dg));
                        dgFiber->reset(dg);
                        MORDOR_LOG_DEBUG(g_log) << this << " running " << dg;
                        dgFiber->yieldTo();
                        if (dgFiber->state() != Fiber::TERM)
                            dgFiber.reset();
                        else
                            dgFiber->reset(NULL);
                    }
                } catch (...) {
                    MORDOR_LOG_FATAL(Log::root())
                        << boost::current_exception_diagnostic_information();
                    throw;
                }
            }
            continue;
        }
        if (dontIdle)
            continue;

        if (idleFiber->state() == Fiber::TERM) {
            MORDOR_LOG_DEBUG(g_log) << this << " idle fiber terminated";
            if (boost::this_thread::get_id() == m_rootThread)
                m_callingFiber.reset();
            return;
        }
        MORDOR_LOG_DEBUG(g_log) << this << " idling";
        idleFiber->call();
        cameFromIdle = true;
    }
}

SchedulerSwitcher::SchedulerSwitcher(Scheduler *target)
{
    m_caller = Scheduler::getThis();
    if (target)
        target->switchTo();
}

SchedulerSwitcher::~SchedulerSwitcher()
{
    if (m_caller)
        m_caller->switchTo();
}

}
