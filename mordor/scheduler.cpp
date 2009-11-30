// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include "scheduler.h"

#include <boost/bind.hpp>

#include "assert.h"
#include "atomic.h"
#include "log.h"

namespace Mordor {

static Logger::ptr g_log = Log::lookup("mordor:scheduler");
static Logger::ptr g_workerLog = Log::lookup("mordor:workerpool");

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
    for (size_t i = 0; i < threads; ++i) {
        m_threads.push_back(boost::shared_ptr<boost::thread>(new boost::thread(m_proc)));
    }
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
        if (stopping())
            return;
    }

    if (m_rootThread != boost::thread::id()) {
        // A thread-hijacking scheduler must be stopped
        // from within itself to return control to the
        // original thread
        MORDOR_ASSERT(Scheduler::getThis() == this);
        // First switch to the correct thread
        MORDOR_LOG_DEBUG(g_log) << this << " switching to root thread to stop";
        switchTo(m_rootThread);
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
    if (Scheduler::getThis() == this) {
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
    if (m_rootThread == boost::this_thread::get_id() ||
        Scheduler::getThis() != this) {
        MORDOR_LOG_DEBUG(g_log) << this << " waiting for other threads to stop";
        m_threads.join_all();
    } else {
        MORDOR_NOTREACHED();
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
    boost::mutex::scoped_lock lock(m_mutex);
    scheduleNoLock(f, thread);
}

void
Scheduler::schedule(boost::function<void ()> dg, boost::thread::id thread)
{
    boost::mutex::scoped_lock lock(m_mutex);
    scheduleNoLock(dg, thread);
}

void
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
    if (tickleMe && Scheduler::getThis() != this)
        tickle();
}

void
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
    if (tickleMe && Scheduler::getThis() != this)
        tickle();
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
    Scheduler::getThis()->yieldTo();
}

void
Scheduler::yieldTo()
{
    MORDOR_LOG_DEBUG(g_log) << this << " yielding to scheduler";
    MORDOR_ASSERT(t_fiber.get());
    if (m_rootThread == boost::this_thread::get_id() &&
        (t_fiber->state() == Fiber::INIT || t_fiber->state() == Fiber::TERM)) {
        yieldTo(true);
    } else {
        yieldTo(false);
    }
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
    while (true) {
        batch.clear();
        bool dontIdle = false;
        {
            boost::mutex::scoped_lock lock(m_mutex);
            for (std::list<FiberAndThread>::iterator it(m_fibers.begin());
                 it != m_fibers.end() && batch.size() < m_batchSize; ) {
                if (it->thread != boost::thread::id() &&
                    it->thread != boost::this_thread::get_id()) {
                    MORDOR_LOG_DEBUG(g_log) << this
                        << " skipping item scheduled for thread "
                        << it->thread;

                    // Wake up another thread to hopefully service this
                    tickle();
                    dontIdle = true;
                    ++it;
                    continue;
                }
                MORDOR_ASSERT(it->fiber || it->dg);
                if (it->fiber && it->fiber->state() == Fiber::EXEC) {
                    ++it;
                    MORDOR_LOG_DEBUG(g_log) << this
                        << " skipping executing fiber " << it->fiber;
                    dontIdle = true;
                    continue;
                }
                batch.push_back(*it);
                it = m_fibers.erase(it);
            }
        }
        MORDOR_LOG_DEBUG(g_log) << this
            << " got " << batch.size() << " fiber/dgs to process (max: "
            << m_batchSize << ")";
        if (!batch.empty()) {
            std::vector<FiberAndThread>::iterator it;
            for (it = batch.begin(); it != batch.end(); ++it) {
                Fiber::ptr f = it->fiber;
                boost::function<void ()> dg = it->dg;

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
                }
            }
            continue;
        }
        if (dontIdle)
            continue;

        if (idleFiber->state() == Fiber::TERM) {
            MORDOR_LOG_DEBUG(g_log) << this << " idle fiber terminated";
            return;
        }
        MORDOR_LOG_DEBUG(g_log) << this << " idling";
        idleFiber->call();
    }
}

WorkerPool::WorkerPool(int threads, bool useCaller, size_t batchSize)
    : Scheduler(threads, useCaller, batchSize)
{
    start();
}

void
WorkerPool::idle()
{
    while (true) {
        if (stopping()) {
            return;
        }
        m_semaphore.wait();
        Fiber::yield();
    }
}

void
WorkerPool::tickle()
{
    MORDOR_LOG_DEBUG(g_workerLog) << this << " tickling";
    m_semaphore.notify();
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

static
void
parallel_do_impl(boost::function<void ()> dg, size_t &completed,
    size_t total, boost::exception_ptr &exception, Scheduler *scheduler,
    Fiber::ptr caller)
{
    try {
        dg();
    } catch (boost::exception &ex) {
        removeTopFrames(ex);
        exception = boost::current_exception();
    } catch (...) {
        exception = boost::current_exception();
    }
    if (atomicIncrement(completed) == total)
        scheduler->schedule(caller);
}

void
parallel_do(const std::vector<boost::function<void ()> > &dgs)
{
    size_t completed = 0;
    Scheduler *scheduler = Scheduler::getThis();
    Fiber::ptr caller = Fiber::getThis();
    std::vector<boost::function<void ()> >::const_iterator it;

    if (scheduler == NULL || dgs.size() <= 1) {
        for(it = dgs.begin(); it != dgs.end(); ++it) {
            (*it)();
        }
        return;
    }

    std::vector<Fiber::ptr> fibers;
    std::vector<boost::exception_ptr> exceptions;
    fibers.resize(dgs.size());
    exceptions.resize(dgs.size());
    for(size_t i = 0; i < dgs.size(); ++i) {
        Fiber::ptr f(new Fiber(boost::bind(&parallel_do_impl, dgs[i],
            boost::ref(completed), dgs.size(), boost::ref(exceptions[i]),
            scheduler, caller)));
        fibers.push_back(f);
        scheduler->schedule(f);
    }

    scheduler->yieldTo();
    // Pass the first exception along
    // TODO: group exceptions?
    for(std::vector<boost::exception_ptr>::iterator it2 = exceptions.begin();
        it2 != exceptions.end();
        ++it2) {
        if (*it2)
            Mordor::rethrow_exception(*it2);
    }
}

void
parallel_do(const std::vector<boost::function<void ()> > &dgs,
            std::vector<Fiber::ptr> &fibers)
{
    MORDOR_ASSERT(fibers.size() >= dgs.size());
    size_t completed = 0;
    Scheduler *scheduler = Scheduler::getThis();
    Fiber::ptr caller = Fiber::getThis();
    std::vector<boost::function<void ()> >::const_iterator it;

    if (scheduler == NULL || dgs.size() <= 1) {
        for(it = dgs.begin(); it != dgs.end(); ++it) {
            (*it)();
        }
        return;
    }

    std::vector<boost::exception_ptr> exceptions;
    exceptions.resize(dgs.size());
    for(size_t i = 0; i < dgs.size(); ++i) {
        fibers[i]->reset(boost::bind(&parallel_do_impl, dgs[i],
            boost::ref(completed), dgs.size(), boost::ref(exceptions[i]),
            scheduler, caller));
        scheduler->schedule(fibers[i]);
    }
    scheduler->yieldTo();
    // Pass the first exception along
    // TODO: group exceptions?
    for(size_t i = 0; i < dgs.size(); ++i) {
        if (exceptions[i])
            Mordor::rethrow_exception(exceptions[i]);
    }
}

FiberMutex::~FiberMutex()
{
#ifdef DEBUG
    boost::mutex::scoped_lock scopeLock(m_mutex);
    MORDOR_ASSERT(!m_owner);
    MORDOR_ASSERT(m_waiters.empty());
#endif
}

void
FiberMutex::lock()
{
    MORDOR_ASSERT(Scheduler::getThis());
    {
        boost::mutex::scoped_lock scopeLock(m_mutex);
        MORDOR_ASSERT(m_owner != Fiber::getThis());
        MORDOR_ASSERT(std::find(m_waiters.begin(), m_waiters.end(),
            std::make_pair(Scheduler::getThis(), Fiber::getThis()))
            == m_waiters.end());
        if (!m_owner) {
            m_owner = Fiber::getThis();
            return;
        }
        m_waiters.push_back(std::make_pair(Scheduler::getThis(), Fiber::getThis()));
    }
    Scheduler::getThis()->yieldTo();
#ifdef DEBUG
    boost::mutex::scoped_lock scopeLock(m_mutex);
    MORDOR_ASSERT(m_owner == Fiber::getThis());
    MORDOR_ASSERT(std::find(m_waiters.begin(), m_waiters.end(),
            std::make_pair(Scheduler::getThis(), Fiber::getThis()))
            == m_waiters.end());
#endif
}

void
FiberMutex::unlock()
{
    boost::mutex::scoped_lock lock(m_mutex);
    unlockNoLock();
}

void
FiberMutex::unlockNoLock()
{
    MORDOR_ASSERT(m_owner == Fiber::getThis());
    m_owner.reset();
    if (!m_waiters.empty()) {
        std::pair<Scheduler *, Fiber::ptr> next = m_waiters.front();
        m_waiters.pop_front();
        m_owner = next.second;
        next.first->schedule(next.second);
    }
}

FiberCondition::~FiberCondition()
{
#ifdef DEBUG
    boost::mutex::scoped_lock lock(m_mutex);
    MORDOR_ASSERT(m_waiters.empty());
#endif
}

void
FiberCondition::wait()
{
    MORDOR_ASSERT(Scheduler::getThis());
    {
        boost::mutex::scoped_lock lock(m_mutex);
        boost::mutex::scoped_lock lock2(m_fiberMutex.m_mutex);
        MORDOR_ASSERT(m_fiberMutex.m_owner == Fiber::getThis());
        m_waiters.push_back(std::make_pair(Scheduler::getThis(),
            Fiber::getThis()));
        m_fiberMutex.unlockNoLock();
    }
    Scheduler::getThis()->yieldTo();
#ifdef DEBUG
    boost::mutex::scoped_lock lock2(m_fiberMutex.m_mutex);
    MORDOR_ASSERT(m_fiberMutex.m_owner == Fiber::getThis());
#endif
}

void
FiberCondition::signal()
{
    std::pair<Scheduler *, Fiber::ptr> next;
    {
        boost::mutex::scoped_lock lock(m_mutex);
        if (m_waiters.empty())
            return;
        next = m_waiters.front();
        m_waiters.pop_front();
    }
    boost::mutex::scoped_lock lock2(m_fiberMutex.m_mutex);
    MORDOR_ASSERT(m_fiberMutex.m_owner != next.second);
    MORDOR_ASSERT(std::find(m_fiberMutex.m_waiters.begin(),
        m_fiberMutex.m_waiters.end(), next)
        == m_fiberMutex.m_waiters.end());
    if (!m_fiberMutex.m_owner) {
        m_fiberMutex.m_owner = next.second;
        next.first->schedule(next.second);
    } else {
        m_fiberMutex.m_waiters.push_back(next);
    }
}

void
FiberCondition::broadcast()
{
    boost::mutex::scoped_lock lock(m_mutex);
    if (m_waiters.empty())
        return;
    boost::mutex::scoped_lock lock2(m_fiberMutex.m_mutex);

    std::list<std::pair<Scheduler *, Fiber::ptr> >::iterator it;
    for (it = m_waiters.begin();
        it != m_waiters.end();
        ++it) {
        std::pair<Scheduler *, Fiber::ptr> &next = *it;
        MORDOR_ASSERT(m_fiberMutex.m_owner != next.second);
        MORDOR_ASSERT(std::find(m_fiberMutex.m_waiters.begin(),
            m_fiberMutex.m_waiters.end(), next)
            == m_fiberMutex.m_waiters.end());
        if (!m_fiberMutex.m_owner) {
            m_fiberMutex.m_owner = next.second;
            next.first->schedule(next.second);
        } else {
            m_fiberMutex.m_waiters.push_back(next);
        }
    }
    m_waiters.clear();
}


FiberEvent::~FiberEvent()
{
#ifdef DEBUG
    boost::mutex::scoped_lock lock(m_mutex);
    MORDOR_ASSERT(m_waiters.empty());
#endif
}

void
FiberEvent::wait()
{
    {
        boost::mutex::scoped_lock lock(m_mutex);
        if (m_signalled) {
            if (m_autoReset)
                m_signalled = false;
            return;
        }
        m_waiters.push_back(std::make_pair(Scheduler::getThis(),
            Fiber::getThis()));
    }
    Scheduler::getThis()->yieldTo();
}

void
FiberEvent::set()
{
    boost::mutex::scoped_lock lock(m_mutex);
    if (!m_autoReset) {
        m_signalled = true;

        std::list<std::pair<Scheduler *, Fiber::ptr> >::iterator it;
        for (it = m_waiters.begin();
            it != m_waiters.end();
            ++it) {
            it->first->schedule(it->second);
        }
        m_waiters.clear();
        return;
    }
    if (m_waiters.empty()) {
        m_signalled = true;
        return;
    }
    m_waiters.front().first->schedule(m_waiters.front().second);
    m_waiters.pop_front();
}

void
FiberEvent::reset()
{
    boost::mutex::scoped_lock lock(m_mutex);
    m_signalled = false;
}

}
