// Copyright (c) 2009 - Decho Corp.

#include "scheduler.h"

#include <cassert>

#include <boost/bind.hpp>

ThreadPool::ThreadPool(boost::function<void()> proc, int threads)
: m_proc(proc), m_size(threads)
{
}

void
ThreadPool::start()
{
    for (size_t i = 0; i < m_size; ++i) {
        boost::thread(m_proc);
    }
}

size_t
ThreadPool::size()
{
    return m_size;
}

boost::thread_specific_ptr<Scheduler> Scheduler::t_scheduler(NULL);
boost::thread_specific_ptr<Fiber> Scheduler::t_fiber(NULL);

Scheduler::Scheduler(int threads, bool useCaller)
    : m_threads(boost::bind(&Scheduler::run, this),
                useCaller ? threads - 1 : threads)
{
    if (useCaller)
        assert(threads >= 1);
    if (useCaller) {
        --threads;
        assert(getThis() == NULL);
        t_scheduler.reset(this);
        t_fiber.reset(new Fiber(boost::bind(&Scheduler::run, this), 65536));
    }
    m_threads.start();
}

Scheduler *
Scheduler::getThis()
{
    return t_scheduler.get();
}

void
Scheduler::stop()
{
    m_stopping = true;
    // XXX: This is incorrect for useCaller = true threads
    for (size_t i = 0; i < m_threads.size(); ++i) {
        tickle();
    }
}

bool
Scheduler::stopping()
{
    return m_stopping;
}

void
Scheduler::schedule(Fiber::ptr f)
{
    assert(f);
    boost::mutex::scoped_lock lock(m_mutex);
    m_fibers.push_back(f);
    if (m_fibers.size() == 1)
        tickle();
}

void
Scheduler::switchTo()
{
    assert(Scheduler::getThis() != NULL);
    if (Scheduler::getThis() == this) {
        return;
    }
    schedule(Fiber::getThis());
    Scheduler::getThis()->yieldTo();
}

void
Scheduler::yieldTo()
{
    assert(t_fiber.get());
    assert(Scheduler::getThis() == this);
    t_fiber->yieldTo(false);
}

void
Scheduler::run()
{
    t_scheduler.reset(this);
    Fiber::ptr f = Fiber::getThis();
    if (!f) {
        f.reset(new Fiber());
    }
    t_fiber.reset(Fiber::getThis().get());
    Fiber idleFiber(boost::bind(&Scheduler::idle, this), 65536 * 4);
    while (true) {
        Fiber::ptr f;
        {
            boost::mutex::scoped_lock lock(m_mutex);
            while (!f) {
                if (m_fibers.empty())
                    break;
                std::list<Fiber::ptr>::const_iterator it;
                for (it = m_fibers.begin(); it != m_fibers.end(); ++it) {
                    f = *it;
                    if (f->state() == Fiber::EXEC) {
                        f.reset();
                        continue;
                    }
                    m_fibers.erase(it);
                    break;
                }
            }
        }
        if (f) {
            if (f->state() != Fiber::TERM) {
                f->yieldTo();
            }
            continue;
        }
        if (idleFiber.state() == Fiber::TERM) {
            return;
        }
        idleFiber.call();
    }
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
