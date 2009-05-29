#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__
// Copyright (c) 2009 - Decho Corp.

#ifdef WIN32
#else
#include <stddef.h>
#endif

#include <cassert>
#include <list>
#include <string>
#include <vector>

#include <boost/thread.hpp>

#include "fiber.h"
#include "semaphore.h"

class ThreadPool
{
public:
    ThreadPool(boost::function<void ()> proc, int threads = 1);

    void start();

    size_t size();

private:
    boost::function<void ()> m_proc;
    size_t m_size;
};

class Scheduler
{
public:
    Scheduler(int threads = 1, bool useCaller = true);

    static Scheduler* getThis();

    void stop();
    bool stopping();

    void schedule(Fiber::ptr f);
    void switchTo();
    void yieldTo();

protected:
    virtual void idle() = 0;
    virtual void tickle() = 0;

private:
    void run();

private:
    static boost::thread_specific_ptr<Scheduler> t_scheduler;
    static boost::thread_specific_ptr<Fiber> t_fiber;
    boost::mutex m_mutex;
    std::list<Fiber::ptr> m_fibers;
    Fiber::ptr m_rootFiber;
    ThreadPool m_threads;
    bool m_stopping;
};

class WorkerPool : public Scheduler
{
public:
    WorkerPool(int threads = 1, bool useCaller = true);

protected:
    void idle();
    void tickle();

private:
    Semaphore m_semaphore;
};

void
parallel_do(const std::vector<boost::function<void ()> > &dgs);

/*
template<class T>
static
void
parallel_foreach_impl(boost::function<int (T&)> dg, T& t, size_t &running,
    Scheduler *scheduler, Fiber::ptr caller)
{
    dg(t);
    // This could be improeved; currently it wait for parallelism fibers
    // to complete, then schedules parallelism more; it would be better if
    // we could schedule another as soon as one completes, but it's difficult
    // to *not* schedule antoher if we're already done
    if (atomicDecrement(running) == 0) {
        scheduler->schedule(caller);
    }
}

template<class C, class T>
void
parallel_foreach(C &collection, boost::function<int (T&)> dg,
    int parallelism = -1)
{
    if (parallelism == -1)
        parallelism = 4;
    size_t running;
    Scheduler *scheduler = Scheduler::getThis();
    Fiber::ptr caller = Fiber::getThis();
    C::const_iterator it;

    for (it = collection.begin(); it != collection.end(); ++it) {
        Fiber::ptr f(new Fiber(boost::bind(parallel_foreach_impl, dg,
            boost::ref(*it), boost::ref(running), scheduler, caller),
            8192));
        bool yield = (atomicIncrement(running) >= parallelism);
        scheduler->schedule(f);

        if (yield) {
            scheduler->yieldTo();
        }
    }
    if (running > 0) {
        scheduler->yieldTo();
    }    
}*/

#endif
