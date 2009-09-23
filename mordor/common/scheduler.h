#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__
// Copyright (c) 2009 - Decho Corp.

#ifdef WIN32
#else
#include <stddef.h>
#endif

#include <list>
#include <string>
#include <vector>

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/thread.hpp>

#include "fiber.h"
#include "semaphore.h"

class ThreadPool2
{
public:
    void init(boost::function<void ()> proc);
    void start(size_t threads = 1);

    size_t size();
    void join_all();

private:
    boost::function<void ()> m_proc;
    boost::mutex m_mutex;
    std::list<boost::shared_ptr<boost::thread> > m_threads;
};

class Scheduler : public boost::noncopyable
{
public:
    Scheduler(int threads = 1, bool useCaller = true);
    virtual ~Scheduler();

    static Scheduler* getThis();

    void stop();
    bool stopping();

    void schedule(Fiber::ptr f, boost::thread::id thread = boost::thread::id());
    void schedule(boost::function<void ()> dg, boost::thread::id thread = boost::thread::id());
    void switchTo(boost::thread::id thread = boost::thread::id());
    void yieldTo();
    // Useful for single thread hijacking scheduler only
    // Calls yieldTo(), and yields back when there is no more work to be done
    void dispatch();

protected:
    void start();
    // Call from derived destructors
    void selfStop();

    virtual void idle() = 0;
    virtual void tickle() = 0;

private:
    void yieldTo(bool yieldToCallerOnTerminate);
    void run();

private:
    struct FiberAndThread {
        Fiber::ptr fiber;
        boost::function<void ()> dg;
        boost::thread::id thread;
    };
    static boost::thread_specific_ptr<Scheduler> t_scheduler;
    static boost::thread_specific_ptr<Fiber> t_fiber;
    boost::mutex m_mutex;
    std::list<FiberAndThread> m_fibers;
    boost::thread::id m_rootThread;
    Fiber::ptr m_rootFiber;
    ThreadPool2 m_threads;
    size_t m_threadCount;
    bool m_stopping;
    bool m_autoStop;
};

class WorkerPool : public Scheduler
{
public:
    WorkerPool(int threads = 1, bool useCaller = true);
    ~WorkerPool() { stop(); }

protected:
    void idle();
    void tickle();

private:
    Semaphore m_semaphore;
};

void
parallel_do(const std::vector<boost::function<void ()> > &dgs);

// Automatically returns to calling scheduler when goes out of scope
struct SchedulerSwitcher : public boost::noncopyable
{
public:
    SchedulerSwitcher(Scheduler *target = NULL);
    ~SchedulerSwitcher();

private:
    Scheduler *m_caller;
};

template<class T>
static
void
parallel_foreach_impl(boost::function<bool (T&)> dg, T *&t,
                      int &result,
                      Scheduler *scheduler, Fiber::ptr caller)
{
    Fiber::getThis()->autoThrowExceptions(false);
    try {
        result = dg(*t) ? 1 : 0;
        t = NULL;
        scheduler->schedule(caller);
    } catch (...) {
        result = 0;
        t = NULL;
        scheduler->schedule(caller);
        throw;
    }
}

template<class Iterator, class T>
bool
parallel_foreach(Iterator begin, Iterator end, boost::function<bool (T &)> dg,
    int parallelism = -1)
{
    if (parallelism == -1)
        parallelism = 4;
    Scheduler *scheduler = Scheduler::getThis();
    Fiber::ptr caller = Fiber::getThis();
    Iterator it = begin;

    std::vector<Fiber::ptr> fibers;
    std::vector<T *> current;
    // Not bool, because that's specialized, and it doesn't return just a
    // bool &, but instead some reference wrapper that the compiler hates
    std::vector<int> result;
    fibers.resize(parallelism);
    current.resize(parallelism);
    result.resize(parallelism);
    for (int i = 0; i < parallelism; ++i) {
        fibers[i] = Fiber::ptr(new Fiber(boost::bind(&parallel_foreach_impl<T>,
            dg, boost::ref(current[i]), boost::ref(result[i]), scheduler,
            caller)));
    }

    int curFiber = 0;
    while (it != end && curFiber < parallelism) {
        current[curFiber] = &*it;
        scheduler->schedule(fibers[curFiber]);
        ++curFiber;
        ++it;
    }
    if (curFiber < parallelism) {
        parallelism = curFiber;
        fibers.resize(parallelism);
        current.resize(parallelism);
        result.resize(parallelism);
    }

    while (it != end) {
        scheduler->yieldTo();
        // Figure out who just finished and scheduled us
        for (int i = 0; i < parallelism; ++i) {
            if (current[i] == NULL) {
                curFiber = i;
                break;
            }
        }
        if (!result[curFiber]) {
            --parallelism;
            break;
        }
        current[curFiber] = &*it;
        fibers[curFiber]->reset();
        scheduler->schedule(fibers[curFiber]);
        ++it;
    }

    // Wait for everyone to finish
    while (parallelism > 0) {
        scheduler->yieldTo();
        --parallelism;
    }

    // Pass the first exception along
    // TODO: group exceptions?
    for(std::vector<Fiber::ptr>::iterator it2 = fibers.begin();
        it2 != fibers.end();
        ++it2) {
        (*it2)->throwExceptions();
    }
    for(std::vector<int>::iterator it2 = result.begin();
        it2 != result.end();
        ++it2) {
        if (!*it2)
            return false;
    }
    return true;
}

#endif
