// Copyright (c) 2009 - Decho Corp.

#include "scheduler.h"

#include <boost/bind.hpp>

#include "assert.h"
#include "atomic.h"

void
ThreadPool2::init(boost::function<void()> proc)
{
    m_proc = proc;
}

void
ThreadPool2::start(size_t threads)
{
    ASSERT(m_proc);
    boost::mutex::scoped_lock lock(m_mutex);
    for (size_t i = 0; i < threads; ++i) {
        m_threads.push_back(boost::shared_ptr<boost::thread>(new boost::thread(m_proc)));
    }
}

size_t
ThreadPool2::size()
{
    boost::mutex::scoped_lock lock(m_mutex);
    return m_threads.size();
}

void
ThreadPool2::join_all()
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

static void delete_nothing_scheduler(Scheduler* f) {}
static void delete_nothing(Fiber* f) {}

boost::thread_specific_ptr<Scheduler> Scheduler::t_scheduler(&delete_nothing_scheduler);
boost::thread_specific_ptr<Fiber> Scheduler::t_fiber(&delete_nothing);

Scheduler::Scheduler(int threads, bool useCaller)
    : m_stopping(true),
      m_autoStop(false)
{
    m_threads.init(boost::bind(&Scheduler::run, this));
    if (useCaller)
        ASSERT(threads >= 1);
    if (useCaller) {
        --threads;
        ASSERT(getThis() == NULL);
        t_scheduler.reset(this);
        m_rootFiber.reset(new Fiber(boost::bind(&Scheduler::run, this), 65536));
        t_scheduler.reset(this);
        t_fiber.reset(m_rootFiber.get());
        m_rootThread = boost::this_thread::get_id();
    }
    m_threadCount = threads;
}

Scheduler::~Scheduler()
{
    ASSERT(m_stopping);
    if (getThis() == this) {
        t_scheduler.reset(NULL);
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
    ASSERT(m_stopping);
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
        m_stopping = true;
        return;
    }

    if (m_rootThread != boost::thread::id()) {
        // A thread-hijacking scheduler must be stopped
        // from within itself to return control to the
        // original thread
        ASSERT(Scheduler::getThis() == this);
        // First switch to the correct thread
        switchTo(m_rootThread);
    } else {
        // A spawned-threads only scheduler cannot be stopped from within
        // itself... who would get control?
        ASSERT(Scheduler::getThis() != this);
    }
    m_stopping = true;
    for (size_t i = 0; i < m_threads.size(); ++i) {
        tickle();
    }
    if (m_rootFiber)
        tickle();
    if (Scheduler::getThis() == this) {
        // Give this thread's run fiber a chance to kill itself off
        yieldTo(true);
    }
    if (m_rootThread == boost::this_thread::get_id() ||
        Scheduler::getThis() != this) {
        m_threads.join_all();
    } else {
        ASSERT(false);
    }
}

bool
Scheduler::stopping()
{
    return m_stopping;
}

void
Scheduler::schedule(Fiber::ptr f, boost::thread::id thread)
{
    ASSERT(f);
    boost::mutex::scoped_lock lock(m_mutex);
    FiberAndThread ft = {f, NULL, thread };
    m_fibers.push_back(ft);
    if (m_fibers.size() == 1)
        tickle();
}

void
Scheduler::schedule(boost::function<void ()> dg, boost::thread::id thread)
{
    ASSERT(dg);
    boost::mutex::scoped_lock lock(m_mutex);
    FiberAndThread ft = {Fiber::ptr(), dg, thread };
    m_fibers.push_back(ft);
    if (m_fibers.size() == 1)
        tickle();
}

void
Scheduler::switchTo(boost::thread::id thread)
{
    ASSERT(Scheduler::getThis() != NULL);
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
    ASSERT(t_fiber.get());
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
    ASSERT(m_rootThread == boost::this_thread::get_id() &&
        m_threads.size() == 0);
    m_stopping = true;
    m_autoStop = true;
    yieldTo();
    m_autoStop = false;
}

void
Scheduler::yieldTo(bool yieldToCallerOnTerminate)
{
    ASSERT(t_fiber.get());
    ASSERT(Scheduler::getThis() == this);
    if (yieldToCallerOnTerminate)
        ASSERT(m_rootThread == boost::this_thread::get_id());
    if (t_fiber->state() != Fiber::HOLD) {
        m_stopping = m_autoStop || m_stopping;
        t_fiber->reset();
    }
    t_fiber->yieldTo(yieldToCallerOnTerminate);
}

void
Scheduler::run()
{
    t_scheduler.reset(this);
    Fiber::ptr threadfiber = Fiber::getThis();
    if (!threadfiber) {
        // Running in own thread
        threadfiber.reset(new Fiber());
        t_fiber.reset(Fiber::getThis().get());
    } else {
        // Hijacked a thread
        ASSERT(t_fiber.get() == Fiber::getThis().get());
        threadfiber.reset();
    }
    Fiber::ptr idleFiber(new Fiber(boost::bind(&Scheduler::idle, this), 65536 * 4));
    while (true) {
        Fiber::ptr f;
        boost::function<void ()> dg;
        bool loop = false;
        {
            boost::mutex::scoped_lock lock(m_mutex);
            while (!f && !dg) {
                if (m_fibers.empty())
                    break;
                std::list<FiberAndThread>::iterator it;
                for (it = m_fibers.begin(); it != m_fibers.end(); ++it) {
                    if (it->thread != boost::thread::id() &&
                        it->thread != boost::this_thread::get_id()) {
                        // Wake up another thread to hopefully service this
                        tickle();
                        loop = true;
                        break;
                    }
                    ASSERT(it->fiber || it->dg);
                    if (it->fiber) {
                        f = it->fiber;
                        if (f->state() == Fiber::EXEC) {
                            f.reset();
                            continue;
                        }
                    } else {
                        dg = it->dg;
                    }
                    m_fibers.erase(it);
                    break;
                }
                if (loop) {
                    break;
                }
            }
        }
        // We're looping because we're not allowed to go back to idle until
        // the correct thread services a a thread-targetted fiber request;
        // but we need to break out of the above loop to release the lock
        if (loop) {
            continue;
        }
        if (f) {
            if (f->state() != Fiber::TERM) {
                f->yieldTo();
            }
            continue;
        } else if (dg) {
            dg();
            continue;
        }
        if (idleFiber->state() == Fiber::TERM) {
            return;
        }
        idleFiber->call();
    }
}

WorkerPool::WorkerPool(int threads, bool useCaller)
    : Scheduler(threads, useCaller)
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
    m_semaphore.notify();
}


SchedulerSwitcher::SchedulerSwitcher(Scheduler *target)
{
    ASSERT(Scheduler::getThis());
    m_caller = Scheduler::getThis();
    if (target)
        target->switchTo();
}

SchedulerSwitcher::~SchedulerSwitcher()
{
    m_caller->switchTo();
}

static
void
parallel_do_impl(boost::function<void ()> dg, size_t &completed,
    size_t total, Scheduler *scheduler, Fiber::ptr caller)
{
    dg();
    if (atomicIncrement(completed) == total) {
        scheduler->schedule(caller);
    }
}

void
parallel_do(const std::vector<boost::function<void ()> > &dgs)
{
    size_t completed = 0;
    Scheduler *scheduler = Scheduler::getThis();
    Fiber::ptr caller = Fiber::getThis();
    std::vector<boost::function<void ()> >::const_iterator it;

    if (scheduler == NULL) {
        for(it = dgs.begin(); it != dgs.end(); ++it) {
            (*it)();
        }
        return;
    }

    for(it = dgs.begin(); it != dgs.end(); ++it) {
        Fiber::ptr f(new Fiber(boost::bind(&parallel_do_impl, *it,
            boost::ref(completed), dgs.size(), scheduler, caller),
            16384));
        scheduler->schedule(f);
    }
    scheduler->yieldTo();
}
