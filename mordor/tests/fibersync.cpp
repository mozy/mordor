// Copyright (c) 2009 - Mozy, Inc.

#include <boost/bind.hpp>

#include "mordor/atomic.h"
#include "mordor/fiber.h"
#include "mordor/fibersynchronization.h"
#include "mordor/iomanager.h"
#include "mordor/sleep.h"
#include "mordor/test/test.h"
#include "mordor/workerpool.h"

using namespace Mordor;

template<typename M> void test_mutex_basic()
{
    WorkerPool pool;
    M mutex;

    typename M::ScopedLock lock(mutex);
}

MORDOR_UNITTEST(FiberMutex, basic)
{
    test_mutex_basic<FiberMutex>();
}

template<typename M> inline void contentionFiber(int fiberNo, M &mutex, int &sequence)
{
    MORDOR_TEST_ASSERT_EQUAL(++sequence, fiberNo);
    typename M::ScopedLock lock(mutex);
    MORDOR_TEST_ASSERT_EQUAL(++sequence, fiberNo + 3 + 1);
}

template<typename M> void test_mutex_contention()
{
    WorkerPool pool;
    M mutex;
    int sequence = 0;
    Fiber::ptr fiber1(new Fiber(NULL)), fiber2(new Fiber(NULL)),
        fiber3(new Fiber(NULL));
    fiber1->reset(boost::bind(&contentionFiber<M>, 1, boost::ref(mutex),
        boost::ref(sequence)));
    fiber2->reset(boost::bind(&contentionFiber<M>, 2, boost::ref(mutex),
        boost::ref(sequence)));
    fiber3->reset(boost::bind(&contentionFiber<M>, 3, boost::ref(mutex),
        boost::ref(sequence)));

    {
        typename M::ScopedLock lock(mutex);
        pool.schedule(fiber1);
        pool.schedule(fiber2);
        pool.schedule(fiber3);
        pool.dispatch();
        MORDOR_TEST_ASSERT_EQUAL(++sequence, 4);
    }
    pool.dispatch();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 8);
}

MORDOR_UNITTEST(FiberMutex, contention)
{
    test_mutex_contention<FiberMutex>();
}

#ifndef NDEBUG
MORDOR_UNITTEST(FiberMutex, notRecursive)
{
    WorkerPool pool;
    FiberMutex mutex;

    FiberMutex::ScopedLock lock(mutex);
    MORDOR_TEST_ASSERT_ASSERTED(mutex.lock());
}
#endif

template<typename M> inline void lockIt(M &mutex)
{
    typename M::ScopedLock lock(mutex);
}

template<typename M> void test_mutex_unlockUnique()
{
    WorkerPool pool;
    M mutex;

    typename M::ScopedLock lock(mutex);
    MORDOR_TEST_ASSERT(!lock.unlockIfNotUnique());
    pool.schedule(boost::bind(&lockIt<M>, boost::ref(mutex)));
    Scheduler::yield();
    MORDOR_TEST_ASSERT(lock.unlockIfNotUnique());
    pool.dispatch();
}

MORDOR_UNITTEST(FiberMutex, unlockUnique)
{
    test_mutex_unlockUnique<FiberMutex>();
}

template<typename M> inline void lockAndHold(IOManager &ioManager, M &mutex, Atomic<int> &counter)
{
    --counter;
    typename M::ScopedLock lock(mutex);
    while(counter > 0)
        Mordor::sleep(ioManager, 50000); // sleep 50ms
}

template<typename M> void test_mutex_performance()
{
    IOManager ioManager(2, true);
    M mutex;
#ifdef X86_64
#ifndef NDEBUG_PERF
    int repeatness = 10000;
#else
    int repeatness = 50000;
#endif
#else
    // on a 32bit system, a process can only have a 4GB virtual address
    // each fiber wound take 1MB virtual address, this gives at most
    // 4096 fibers can be alive simultaneously.
    int repeatness = 1000;
#endif
    Atomic<int> counter = repeatness;
    unsigned long long before = TimerManager::now();
    for (int i=0; i<repeatness; ++i) {
        ioManager.schedule(boost::bind(lockAndHold<M>,
                                boost::ref(ioManager),
                                boost::ref(mutex),
                                boost::ref(counter)));
    }
    ioManager.stop();
    unsigned long long elapse = TimerManager::now() - before;
    MORDOR_LOG_INFO(Mordor::Log::root()) << "elapse: " << elapse;
}

MORDOR_UNITTEST(FiberMutex, mutexPerformance)
{
    test_mutex_performance<FiberMutex>();
}

MORDOR_UNITTEST(RecursiveFiberMutex, basic)
{
    test_mutex_basic<RecursiveFiberMutex>();
}

MORDOR_UNITTEST(RecursiveFiberMutex, recursive_basic)
{
    WorkerPool pool;
    RecursiveFiberMutex mutex;

    RecursiveFiberMutex::ScopedLock lock0(mutex);
    {
        RecursiveFiberMutex::ScopedLock lock1(mutex);
        {
            RecursiveFiberMutex::ScopedLock lock2(mutex);
        }
    }
}

MORDOR_UNITTEST(RecursiveFiberMutex, contention)
{
    test_mutex_contention<RecursiveFiberMutex>();
}

MORDOR_UNITTEST(RecursiveFiberMutex, mutexPerformance)
{
    test_mutex_performance<RecursiveFiberMutex>();
}

MORDOR_UNITTEST(RecursiveFiberMutex, unlockUnique)
{
    test_mutex_unlockUnique<RecursiveFiberMutex>();
}

static void signalMe(FiberCondition &condition, int &sequence)
{
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 2);
    condition.signal();
}

MORDOR_UNITTEST(FiberCondition, signal)
{
    int sequence = 0;
    WorkerPool pool;
    FiberMutex mutex;
    FiberCondition condition(mutex);

    FiberMutex::ScopedLock lock(mutex);
    pool.schedule(boost::bind(&signalMe, boost::ref(condition),
        boost::ref(sequence)));
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 1);
    condition.wait();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 3);
}

static void waitOnMe(FiberCondition &condition, FiberMutex &mutex,
                     int &sequence, int expected)
{
    MORDOR_TEST_ASSERT_EQUAL(++sequence, expected * 2);
    FiberMutex::ScopedLock lock(mutex);
    MORDOR_TEST_ASSERT_EQUAL(++sequence, expected * 2 + 1);
    condition.wait();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, expected + 8);
}

MORDOR_UNITTEST(FiberCondition, broadcast)
{
    int sequence = 0;
    WorkerPool pool;
    FiberMutex mutex;
    FiberCondition condition(mutex);

    pool.schedule(boost::bind(&waitOnMe, boost::ref(condition),
        boost::ref(mutex), boost::ref(sequence), 1));
    pool.schedule(boost::bind(&waitOnMe, boost::ref(condition),
        boost::ref(mutex), boost::ref(sequence), 2));
    pool.schedule(boost::bind(&waitOnMe, boost::ref(condition),
        boost::ref(mutex), boost::ref(sequence), 3));
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 1);
    pool.dispatch();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 8);
    condition.broadcast();
    pool.dispatch();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 12);
}

static void signalMe2(FiberEvent &event, int &sequence)
{
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 2);
    event.set();
}

MORDOR_UNITTEST(FiberEvent, autoResetSetWithoutExistingWaiters)
{
    int sequence = 0;
    WorkerPool pool;
    FiberEvent event;

    pool.schedule(boost::bind(&signalMe2, boost::ref(event),
        boost::ref(sequence)));
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 1);
    event.wait();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 3);
    event.set();
    // no fiber waiting at this moment, but the event is signaled
    event.wait();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 4);
}

static void signalMe3(TimerManager &manager, FiberEvent &event,
                      int &sequence, unsigned long long awhile)
{
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 2);
    event.set();
    Mordor::sleep(manager, awhile);
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 4);
    // a fiber is already waiting for a while
    event.set();
}

MORDOR_UNITTEST(FiberEvent, autoResetSetWithExistingWaiters)
{
    int sequence = 0;
    IOManager manager; // for a timer-enabled scheduler
    FiberEvent event;
    static const unsigned long long awhile = 50000ULL; // sleep for 50ms

    manager.schedule(boost::bind(&signalMe3, boost::ref(manager),
        boost::ref(event), boost::ref(sequence), awhile));
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 1);
    event.wait();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 3);
    Test::TakesAtLeast _(awhile);
    // the first set() call should not leave the event signaled, so this fiber
    // should be blocked until signalMe3() allows it to move on.
    event.wait();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 5);
}

MORDOR_UNITTEST(FiberEvent, manualReset)
{
    int sequence = 0;
    WorkerPool pool;
    FiberEvent event(false);

    pool.schedule(boost::bind(&signalMe2, boost::ref(event),
        boost::ref(sequence)));
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 1);
    event.wait();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 3);
    // It's manual reset; you can wait as many times as you want until it's
    // reset
    event.wait();
    event.wait();
}

static void waitOnMe2(FiberEvent &event, int &sequence, int expected)
{
    MORDOR_TEST_ASSERT_EQUAL(++sequence, expected + 1);
    event.wait();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, expected + 5);
}

MORDOR_UNITTEST(FiberEvent, manualResetMultiple)
{
    int sequence = 0;
    WorkerPool pool;
    FiberEvent event(false);

    pool.schedule(boost::bind(&waitOnMe2, boost::ref(event),
        boost::ref(sequence), 1));
    pool.schedule(boost::bind(&waitOnMe2, boost::ref(event),
        boost::ref(sequence), 2));
    pool.schedule(boost::bind(&waitOnMe2, boost::ref(event),
        boost::ref(sequence), 3));
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 1);
    pool.dispatch();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 5);
    event.set();
    pool.dispatch();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 9);
    // It's manual reset; you can wait as many times as you want until it's
    // reset
    event.wait();
    event.wait();
}

class EventOwner
{
public:
    EventOwner()
        : m_event(false)
        , m_destroying(false)
    {}

    ~EventOwner()
    {

        // 1 thread case - we can't awake from yielding in the wait() call
        // until the fiber for the scheduled setEvent call is complete
        //
        // Multi-thread case - We can wake up because event is signalled, but
        //other thread might still be inside m_event.set() call,
        //with m_event mutex still locked.  Destroying that mutex while
        //set() is being called can cause crash
        m_event.wait();
        m_destroying = true;

        // Note: in debug mode the FiberEvent will get blocked waiting
        // for the lock help in FiberEvent::set, but NDEBUG build will not
    }

    void setEvent()
    {
        m_event.set();
        if (m_destroying) {
            MORDOR_NOTREACHED();
        }
    }

    FiberEvent m_event;
    bool m_destroying;
};


MORDOR_UNITTEST(FiberEvent, destroyAfterSet)
{
    // Demo risk of using an FiberEvent in multi-threaded enviroment
#if 0
    {
        //Not safe - owner destruction can start while pool2 is still
        //executing setEvent().  Even though we wait on event the
        //destructor is allowed to proceed before setEvent() has finished.
        WorkerPool pool;
        WorkerPool pool2(1,false);
        EventOwner owner;
        pool2.schedule(boost::bind(&EventOwner::setEvent, &owner));
    }
#endif

    {
        // Safe multi-threaded scenario - pool2 is stopped before event owner is destroyed
        // which ensures that scheduled event is complete
        WorkerPool pool;
        WorkerPool pool2(1,false);
        EventOwner owner;
        pool2.schedule(boost::bind(&EventOwner::setEvent, &owner));
        pool2.stop();
    }

    {
        // Safe multi-threaded scenario - variables are declared in correct order so
        // that pool2 is stopped before event owner is destroyed
        WorkerPool pool;
        EventOwner owner;
        WorkerPool pool2(1,false);
        pool2.schedule(boost::bind(&EventOwner::setEvent, &owner));
    }

    {
        // Safe single threaded scenario - pool stops itself before
        // owner is destroyed
        WorkerPool pool;
        EventOwner owner;
        pool.schedule(boost::bind(&EventOwner::setEvent, &owner));
        pool.stop();
    }

    {
        // Safe single threaded scenario - pool destruction automatically
        // blocks until setEvent is complete, then owner is destroyed
        EventOwner owner;
        WorkerPool pool;
        pool.schedule(boost::bind(&EventOwner::setEvent, &owner));
    }

    {
        // This is the only case that the event is actually needed and useful!
        // Because only one fiber executes at a time on the single thread
        WorkerPool pool;
        EventOwner owner;
        pool.schedule(boost::bind(&EventOwner::setEvent, &owner));
    }


}
