// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/pch.h"

#include "parallel.h"

#include "assert.h"
#include "atomic.h"

namespace Mordor {

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
    Scheduler::yieldTo();
    // Pass the first exception along
    // TODO: group exceptions?
    for (size_t i = 0; i < dgs.size(); ++i) {
        if (exceptions[i])
            Mordor::rethrow_exception(exceptions[i]);
    }
}

}
