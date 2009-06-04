// Copyright (c) 2009 - Decho Corp.

#include "scheduler.h"

#include <cassert>

#include <boost/bind.hpp>

#include "atomic.h"

ThreadPool::ThreadPool(boost::function<void()> proc)
: m_proc(proc)
{
}

void
ThreadPool::start(size_t threads)
{
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
}

static void delete_nothing_scheduler(Scheduler* f) {}
static void delete_nothing(Fiber* f) {}

boost::thread_specific_ptr<Scheduler> Scheduler::t_scheduler(&delete_nothing_scheduler);
boost::thread_specific_ptr<Fiber> Scheduler::t_fiber(&delete_nothing);

Scheduler::Scheduler(int threads, bool useCaller)
    : m_threads(boost::bind(&Scheduler::run, this)),
      m_stopping(false)
{
    if (useCaller)
        assert(threads >= 1);
    if (useCaller) {
        --threads;
        assert(getThis() == NULL);
        t_scheduler.reset(this);
        m_rootFiber.reset(new Fiber(boost::bind(&Scheduler::run, this), 65536));
        m_rootThread = boost::this_thread::get_id();
        t_fiber.reset(m_rootFiber.get());
    }
    m_threads.start(threads);
}

Scheduler *
Scheduler::getThis()
{
    return t_scheduler.get();
}

void
Scheduler::stop()
{
    if (m_rootThread == boost::thread::id()) {
        assert(Scheduler::getThis() != this);
    } else {
        assert(Scheduler::getThis() == this);
    }
    if (m_rootThread != boost::thread::id()) {
        // First switch to the correct thread
        switchTo(m_rootThread);
    }
    m_stopping = true;
    for (size_t i = 0; i < m_threads.size(); ++i) {
        tickle();
    }
    if (m_rootFiber)
        tickle();
    if (Scheduler::getThis() == this) {
        yieldTo(true);
    }
    m_threads.join_all();
}

bool
Scheduler::stopping()
{
    return m_stopping;
}

void
Scheduler::schedule(Fiber::ptr f, boost::thread::id thread)
{
    assert(f);
    boost::mutex::scoped_lock lock(m_mutex);
    FiberAndThread ft = {f, thread };
    m_fibers.push_back(ft);
    if (m_fibers.size() == 1)
        tickle();
}

void
Scheduler::switchTo(boost::thread::id thread)
{
    assert(Scheduler::getThis() != NULL);
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
    yieldTo(false);
}

void
Scheduler::yieldTo(bool yieldToCallerOnTerminate)
{
    assert(t_fiber.get());
    assert(Scheduler::getThis() == this);
    t_fiber->yieldTo(yieldToCallerOnTerminate);
}

void
Scheduler::run()
{
    t_scheduler.reset(this);
    Fiber::ptr rootfiber = Fiber::getThis();
    if (!rootfiber) {
        rootfiber.reset(new Fiber());
    } else {
        rootfiber.reset();
    }
    t_fiber.reset(Fiber::getThis().get());
    Fiber::ptr idleFiber(new Fiber(boost::bind(&Scheduler::idle, this), 65536 * 4));
    while (true) {
        Fiber::ptr f;
        bool loop = false;
        {
            boost::mutex::scoped_lock lock(m_mutex);
            while (!f) {
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
                    f = it->fiber;
                    if (f->state() == Fiber::EXEC) {
                        f.reset();
                        continue;
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
        }
        if (idleFiber->state() == Fiber::TERM) {
            return;
        }
        idleFiber->call();
    }
}

WorkerPool::WorkerPool(int threads, bool useCaller)
    : Scheduler(threads, useCaller)
{}

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
