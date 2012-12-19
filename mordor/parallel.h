#ifndef __MORDOR_PARALLEL_H__
#define __MORDOR_PARALLEL_H__
// Copyright (c) 2009 - Mozy, Inc.

#include <vector>

#include <boost/bind.hpp>
#include <boost/noncopyable.hpp>
#include <boost/function.hpp>

#include "atomic.h"
#include "fiber.h"
#include "log.h"
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

namespace Detail {

template<class Iterator, class Functor>
static
void
parallel_foreach_impl(Iterator &begin, Iterator &end, Functor &functor,
                      boost::mutex &mutex, boost::exception_ptr &exception,
                      Scheduler *scheduler, Fiber::ptr caller, int &count)
{
    while (true) {
        try {
            Iterator it;
            {
                boost::mutex::scoped_lock lock(mutex);
                if (begin == end || exception)
                    break;
                it = begin++;
            }
            functor(*it);
        } catch (boost::exception &ex) {
            removeTopFrames(ex);
            boost::mutex::scoped_lock lock(mutex);
            exception = boost::current_exception();
            break;
        } catch (...) {
            boost::mutex::scoped_lock lock(mutex);
            exception = boost::current_exception();
            break;
        }
    }
    // Don't want to own the mutex here, because another thread could pick up
    // caller immediately, and return from parallel_for before this thread has
    // a chance to unlock it
    if (atomicDecrement(count) == 0)
        scheduler->schedule(caller);
}

Logger::ptr getLogger();

}

/// Execute a functor for multiple objects in parallel

/// @ingroup parallel_do
/// Execute a functor for multiple objects in parallel by scheduling up to
/// parallelism at a time on the current Scheduler.  Concurrency is achieved
/// either because the Scheduler is running on multiple threads, or because the
/// the functor yields to the Scheduler during execution, instead of blocking.
/// @tparam Iterator The type of the iterator for the collection
/// @tparam T The type returned by dereferencing the Iterator, and then passed
/// to the functor
/// @param begin The beginning of the collection
/// @param end The end of the collection
/// @param dg The functor to be passed each object in the collection
/// @param parallelism How many objects to Schedule in parallel
template<class Iterator, class Functor>
void
parallel_foreach(Iterator begin, Iterator end, Functor functor,
    int parallelism = -1)
{
    if (parallelism == -1)
        parallelism = 4;
    Scheduler *scheduler = Scheduler::getThis();

    if (parallelism == 1 || !scheduler) {
        MORDOR_LOG_DEBUG(Detail::getLogger())
            << " running parallel_for sequentially";
        while (begin != end)
            functor(*begin++);
        return;
    }

    boost::mutex mutex;
    boost::exception_ptr exception;
    MORDOR_LOG_DEBUG(Detail::getLogger()) << " running parallel_for with "
        << parallelism << " fibers";
    int count = parallelism;
    for (int i = 0; i < parallelism; ++i) {
        scheduler->schedule(boost::bind(
            &Detail::parallel_foreach_impl<Iterator, Functor>,
            boost::ref(begin), boost::ref(end), boost::ref(functor),
            boost::ref(mutex), boost::ref(exception), scheduler,
            Fiber::getThis(), boost::ref(count)));
    }
    Scheduler::yieldTo();

    if (exception)
        Mordor::rethrow_exception(exception);
}

}

#endif
