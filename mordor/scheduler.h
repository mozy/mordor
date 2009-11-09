#ifndef __MORDOR_SCHEDULER_H__
#define __MORDOR_SCHEDULER_H__
// Copyright (c) 2009 - Decho Corp.

#ifdef WIN32
#else
#include <stddef.h>
#endif

#include <list>
#include <string>
#include <vector>

#include <boost/bind.hpp>
#include <boost/exception.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/thread.hpp>

#include "fiber.h"
#include "semaphore.h"
#include "thread_local_storage.h"

namespace Mordor {

class ThreadPool
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

/// Cooperative user-mode thread (Fiber) Scheduler

/// A Scheduler is used to cooperatively schedule fibers on threads,
/// implementing an M-on-N threading model. A Scheduler can either "hijack"
/// the thread it was created on (by passing useCaller = true in the
/// constructor), spawn multiple threads of its own, or a hybrid of the two.
///
/// Hijacking and Schedulers begin processing fibers when either
/// yieldTo() or dispatch() is called. The Scheduler will stop itself when
/// there are no more Fibers scheduled, and return from yieldTo() or
/// dispatch(). Hybrid and spawned Schedulers must be explicitly stopped via
/// stop(). stop() will return only after there are no more Fibers scheduled.
class Scheduler : public boost::noncopyable
{
public:
    /// Default constructor

    /// By default, a single-threaded hijacking Scheduler is constructed.
    /// If threads > 1 && useCaller == true, a hybrid Scheduler is constructed.
    /// If useCaller == false, this Scheduler will not be associated with
    /// the currently executing thread.
    /// @param threads How many threads this Scheduler should be comprised of
    /// @param useCaller If this Scheduler should "hijack" the currently
    /// executing thread
    /// @param batchSize Number of operations to pull off the scheduler queue
    /// on every iteration
    /// @pre if (useCaller == true) Scheduler::getThis() == NULL
    Scheduler(int threads = 1, bool useCaller = true, size_t batchSize = 1);
    /// Destroys the scheduler, implicitly calling stop()
    virtual ~Scheduler();

    /// @return The Scheduler controlling the currently executing thread
    static Scheduler* getThis();

    /// Explicitly stop the scheduler
    
    /// This must be called for hybrid and spawned Schedulers.  It can be
    /// called multiple times.
    void stop();

    /// Schedule a Fiber to be executed on the Scheduler

    /// @param f The Fiber to schedule
    /// @param thread Optionally provide a specific thread for the Fiber to run
    /// on
    void schedule(Fiber::ptr f, boost::thread::id thread = boost::thread::id());
    /// Schedule a generic functor to be executed on the Scheduler

    /// The functor will be executed on a new Fiber.
    /// @param dg The functor to schedule
    /// @param thread Optionally provide a specific thread for the functor to
    /// run on
    void schedule(boost::function<void ()> dg, boost::thread::id thread = boost::thread::id());

    /// Schedule multiple items to be executed at once
    
    /// @param begin The first item to schedule
    /// @param end One past the last item to schedule
    template <class InputIterator>
    void schedule(InputIterator begin, InputIterator end)
    {
        boost::mutex::scoped_lock lock(m_mutex);
        while (begin != end) {
            scheduleNoLock(*begin);
            ++begin;
        }
    }

    /// Change the currently executing Fiber to be running on this Scheduler
    
    /// This function can be used to change which Scheduler/thread the
    /// currently executing Fiber is executing on.  This switch is done by
    /// rescheduling this Fiber on this Scheduler, and yielding to the current
    /// Scheduler.
    /// @param thread Optionally provide a specific thread for this Fiber to
    /// run on
    /// @post Scheduler::getThis() == this
    void switchTo(boost::thread::id thread = boost::thread::id());

    /// Yield to the Scheduler to allow other Fibers to execute on this thread

    /// The Scheduler will not re-schedule this Fiber automatically.
    ///
    /// In a hijacking Scheduler, any scheduled work will begin running if
    /// someone yields to the Scheduler.
    /// @pre Scheduler::getThis() == this
    void yieldTo();

    /// Force a hijacking Scheduler to process scheduled work

    /// Calls yieldTo(), and yields back to the currently executing Fiber
    /// when there is no more work to be done
    /// @pre this is a hijacking Scheduler
    void dispatch();

protected:
    /// Derived classes should call start() in their constructor.
    void start();
    /// Derived classes can query stopping() to see if the Scheduler is trying
    /// to stop, and should return from the idle Fiber as soon as possible.
    ///
    /// Also, this function should be implemented if the derived class has
    /// any additional work to do in the idle Fiber that the Scheduler is not
    /// aware of.
    virtual bool stopping();

    /// The function called (in its own Fiber) when there is no work scheduled
    /// on the Scheduler.  The Scheduler is not considered stopped until the
    /// idle Fiber has terminated.
    ///
    /// Implementors should Fiber::yield() when it believes there is work
    /// scheduled on the Scheduler.
    virtual void idle() = 0;
    /// The Scheduler wants to force the idle fiber to Fiber::yield(), because
    /// new work has been scheduled.
    virtual void tickle() = 0;

private:
    void yieldTo(bool yieldToCallerOnTerminate);
    void run();

    void scheduleNoLock(Fiber::ptr f, boost::thread::id thread = boost::thread::id());
    void scheduleNoLock(boost::function<void ()> dg, boost::thread::id thread = boost::thread::id());

private:
    struct FiberAndThread {
        Fiber::ptr fiber;
        boost::function<void ()> dg;
        boost::thread::id thread;
    };
    static ThreadLocalStorage<Scheduler> t_scheduler;
    static ThreadLocalStorage<Fiber> t_fiber;
    boost::mutex m_mutex;
    std::list<FiberAndThread> m_fibers;
    boost::thread::id m_rootThread;
    Fiber::ptr m_rootFiber;
    ThreadPool m_threads;
    size_t m_threadCount;
    bool m_stopping;
    bool m_autoStop;
    size_t m_batchSize;
};

/// Generic Scheduler

/// A WorkerPool is a generic Scheduler that does nothing when there is no work
/// to be done.
class WorkerPool : public Scheduler
{
public:
    WorkerPool(int threads = 1, bool useCaller = true, size_t batchSize = 1);
    ~WorkerPool() { stop(); }

protected:
    /// The idle Fiber for a WorkerPool simply loops waiting on a Semaphore,
    /// and yields whenever that Semaphore is signalled, returning if
    /// stopping() is true.
    void idle();
    /// Signals the semaphore so that the idle Fiber will yield.
    void tickle();

private:
    Semaphore m_semaphore;
};

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

/// Automatic Scheduler switcher

/// Automatically returns to Scheduler::getThis() when goes out of scope
/// (by calling Scheduler::switchTo())
struct SchedulerSwitcher : public boost::noncopyable
{
public:
    /// Captures Scheduler::getThis(), and optionally calls target->switchTo()
    /// if target != NULL
    SchedulerSwitcher(Scheduler *target = NULL);
    /// Calls switchTo() on the Scheduler captured in the constructor
    /// @post Scheduler::getThis() == the Scheduler captured in the constructor
    ~SchedulerSwitcher();

private:
    Scheduler *m_caller;
};

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

/// Scheduler based Mutex for Fibers

/// Mutex for use by Fibers that yields to a Scheduler instead of blocking
/// if the mutex cannot be immediately acquired.  It also provides the
/// additional guarantee that it is strictly FIFO, instead of random which
/// Fiber will acquire the mutex next after it is released.
struct FiberMutex : boost::noncopyable
{
    friend struct FiberCondition;
public:
    /// Type that will lock the mutex on construction, and unlock on
    /// destruction
    struct ScopedLock
    {
    public:
        ScopedLock(FiberMutex &mutex)
            : m_mutex(&mutex)
        {
            m_mutex->lock();
        }
        ~ScopedLock()
        { release(); }

        void release()
        {
            if (m_mutex) {
                m_mutex->release();
                m_mutex = NULL;
            }
        }

    private:
        FiberMutex *m_mutex;
    };

public:
    ~FiberMutex();

    /// @brief Locks the mutex
    /// Note that it is possible for this Fiber to switch threads after this
    /// method, though it is guaranteed to still be on the same Scheduler
    /// @pre Scheduler::getThis() != NULL
    /// @pre Fiber::getThis() does not own this mutex
    /// @post Fiber::getThis() owns this mutex
    void lock();
    /// @brief Releases the mutex
    /// @pre Fiber::getThis() owns this mutex
    void release();

private:
    void releaseNoLock();

private:
    boost::mutex m_mutex;
    Fiber::ptr m_owner;
    std::list<std::pair<Scheduler *, Fiber::ptr> > m_waiters;
};

/// Scheduler based condition variable for Fibers

/// Condition for use by Fibers that yields to a Scheduler instead of blocking.
/// It also provides the additional guarantee that it is strictly FIFO,
/// instead of random which waiting Fiber will be released when the condition
/// is signalled.
struct FiberCondition : boost::noncopyable
{
public:
    /// @param mutex The mutex to associate with the Condition
    FiberCondition(FiberMutex &mutex)
        : m_fiberMutex(mutex)
    {}
    ~FiberCondition();

    /// @brief Wait for the Condition to be signalled
    /// @details
    /// Atomically unlock mutex, and wait for the Condition to be signalled.
    /// Once released, the mutex is locked again.
    /// @pre Scheduler::getThis() != NULL
    /// @pre Fiber::getThis() owns mutex
    /// @post Fiber::getThis() owns mutex
    void wait();
    /// Release a single Fiber from wait()
    void signal();
    /// Release all waiting Fibers
    void broadcast();

private:
    boost::mutex m_mutex;
    FiberMutex &m_fiberMutex;
    std::list<std::pair<Scheduler *, Fiber::ptr> > m_waiters;
};

/// Scheduler based event variable for Fibers

/// Event for use by Fibers that yields to a Scheduler instead of blocking.
/// It also provides the additional guarantee that it is strictly FIFO,
/// instead of random which waiting Fiber will be released when the event
/// is signalled.
struct FiberEvent : boost::noncopyable
{
public:
    /// @param autoReset If the Event should automatically reset itself
    /// whenever a Fiber is released
    FiberEvent(bool autoReset = true)
        : m_signalled(false),
          m_autoReset(autoReset)
    {}
    ~FiberEvent();

    /// @brief Wait for the Event to become set
    /// @pre Scheduler::getThis() != NULL
    void wait();
    /// Set the Event
    void set();
    /// Reset the Event
    void reset();

private:
    boost::mutex m_mutex;
    bool m_signalled, m_autoReset;
    std::list<std::pair<Scheduler *, Fiber::ptr> > m_waiters;
};

}

#endif
