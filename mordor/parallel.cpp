// Copyright (c) 2009 - Mozy, Inc.

#include "parallel.h"

#include <boost/scoped_ptr.hpp>

#include "assert.h"
#include "atomic.h"
#include "fibersynchronization.h"

namespace Mordor {

static Logger::ptr g_log = Log::lookup("mordor:parallel");

static
void
parallel_do_impl(boost::function<void ()> dg, size_t &completed,
    size_t total, boost::exception_ptr &exception, Scheduler *scheduler,
    Fiber::ptr caller, FiberSemaphore *sem)
{
    if (sem)
        sem->wait();
    try {
        dg();
    } catch (boost::exception &ex) {
        removeTopFrames(ex);
        exception = boost::current_exception();
    } catch (...) {
        exception = boost::current_exception();
    }
    if (sem)
        sem->notify();
    if (atomicIncrement(completed) == total)
        scheduler->schedule(caller);
}

void
parallel_do(const std::vector<boost::function<void ()> > &dgs,
    int parallelism)
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

    MORDOR_ASSERT(parallelism != 0);
    boost::scoped_ptr<FiberSemaphore> sem;
    if (parallelism != -1)
        sem.reset(new FiberSemaphore(parallelism));

    std::vector<Fiber::ptr> fibers;
    std::vector<boost::exception_ptr> exceptions;
    fibers.reserve(dgs.size());
    exceptions.resize(dgs.size());
    for(size_t i = 0; i < dgs.size(); ++i) {
        Fiber::ptr f(new Fiber(boost::bind(&parallel_do_impl, dgs[i],
            boost::ref(completed), dgs.size(), boost::ref(exceptions[i]),
            scheduler, caller, sem.get())));
        fibers.push_back(f);
        scheduler->schedule(f);
    }

    Scheduler::yieldTo();
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
            std::vector<Fiber::ptr> &fibers,
            int parallelism)
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

    boost::scoped_ptr<FiberSemaphore> sem;
    MORDOR_ASSERT(parallelism != 0);
    if (parallelism != -1)
        sem.reset(new FiberSemaphore(parallelism));

    std::vector<boost::exception_ptr> exceptions;
    exceptions.resize(dgs.size());
    for(size_t i = 0; i < dgs.size(); ++i) {
        fibers[i]->reset(boost::bind(&parallel_do_impl, dgs[i],
            boost::ref(completed), dgs.size(), boost::ref(exceptions[i]),
            scheduler, caller, sem.get()));
        scheduler->schedule(fibers[i]);
    }
    Scheduler::yieldTo();
    // Make sure all fibers have actually exited, to avoid the caller
    // immediately calling Fiber::reset, and the Fiber hasn't actually exited
    // because it is running in a different thread.
    for (size_t i = 0; i < dgs.size(); ++i)
        while (fibers[i]->state() == Fiber::EXEC) Scheduler::yield();

    // Pass the first exception along
    // TODO: group exceptions?
    for (size_t i = 0; i < dgs.size(); ++i) {
        if (exceptions[i])
            Mordor::rethrow_exception(exceptions[i]);
    }
}

namespace Detail {

Logger::ptr getLogger()
{
    return g_log;
}

}}
