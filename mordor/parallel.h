#ifndef __MORDOR_PARALLEL_H__
#define __MORDOR_PARALLEL_H__
// Copyright (c) 2009 - Mozy, Inc.

#include <vector>

#include <boost/noncopyable.hpp>
#include <boost/function.hpp>

#include "fiber.h"
#include "scheduler.h"

namespace Mordor {

/// @defgroup parallel_do
/// @brief Execute multiple functors in parallel
///
/// Execute multiple functors in parallel by scheduling them all on the current
/// Scheduler.  Concurrency is achieved either because the Scheduler is running
/// on multiple threads, or because the functors will yield to the Scheduler
/// during execution, instead of blocking.
///
/// If there is no Scheduler associated with the current thread, the functors
/// are simply executed sequentially.
///
/// If any of the functors throw an uncaught exception, the first uncaught
/// exception is rethrown to the caller.

/// @ingroup parallel_do
/// @param dgs The functors to execute
void
parallel_do(const std::vector<boost::function<void ()> > &dgs);
/// @ingroup parallel_do
/// @param dgs The functors to execute
/// @param fibers The Fibers to use to execute the functors
/// @pre dgs.size() <= fibers.size()
void
parallel_do(const std::vector<boost::function<void ()> > &dgs,
            std::vector<Fiber::ptr> &fibers);

template<class T>
static
void
parallel_foreach_impl(boost::function<bool (T&)> dg, T *&t,
                      int &result, boost::exception_ptr &exception,
                      Scheduler *scheduler, Fiber::ptr caller)
{
    try {
        result = dg(*t) ? 1 : 0;
    } catch (boost::exception &ex) {
        result = 0;
        removeTopFrames(ex);
        exception = boost::current_exception();
    } catch (...) {
        result = 0;
        exception = boost::current_exception();
    }
    t = NULL;
    scheduler->schedule(caller);
}

/// Execute a functor for multiple objects in parallel

/// @ingroup parallel_do
/// Execute a functor for multiple objects in parallel by scheduling up to
/// parallelism at a time on the current Scheduler.  Concurrency is achived
/// either because the Scheduler is running on multiple threads, or because the
/// the functor yields to the Scheduler during execution, instead of blocking.
/// @tparam Iterator The type of the iterator for the collection
/// @tparam T The type returned by dereferencing the Iterator, and then passed
/// to the functor
/// @param begin The beginning of the collection
/// @param end The end of the collection
/// @param dg The functor to be passed each object in the collection
/// @param parallelism How many objects to Schedule in parallel
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

    if (parallelism == 1) {
        while (it != end) {
            if (!dg(*it))
                return false;
            ++it;
        }
        return true;
    }

    std::vector<Fiber::ptr> fibers;
    std::vector<T *> current;
    // Not bool, because that's specialized, and it doesn't return just a
    // bool &, but instead some reference wrapper that the compiler hates
    std::vector<int> result;
    std::vector<boost::exception_ptr> exceptions;
    fibers.resize(parallelism);
    current.resize(parallelism);
    result.resize(parallelism);
    exceptions.resize(parallelism);
    for (int i = 0; i < parallelism; ++i) {
        fibers[i] = Fiber::ptr(new Fiber(boost::bind(&parallel_foreach_impl<T>,
            dg, boost::ref(current[i]), boost::ref(result[i]),
            boost::ref(exceptions[i]), scheduler, caller)));
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
        Scheduler::yieldTo();
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
        Scheduler::yieldTo();
        --parallelism;
    }

    // Pass the first exception along
    // TODO: group exceptions?
    for(std::vector<boost::exception_ptr>::iterator it2 = exceptions.begin();
        it2 != exceptions.end();
        ++it2) {
        if (*it2)
            Mordor::rethrow_exception(*it2);
    }
    for(std::vector<int>::iterator it2 = result.begin();
        it2 != result.end();
        ++it2) {
        if (!*it2)
            return false;
    }
    return true;
}

}

#endif
