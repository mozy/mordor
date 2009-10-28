// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include "mordor/scheduler.h"
#include "mordor/test/test.h"

using namespace Mordor;

MORDOR_UNITTEST(FiberMutex, basic)
{
    Fiber::ptr mainFiber(new Fiber());
    WorkerPool pool;
    FiberMutex mutex;

    FiberMutex::ScopedLock lock(mutex);
}


static void contentionFiber(int fiberNo, FiberMutex &mutex, int &sequence)
{
    MORDOR_TEST_ASSERT_EQUAL(++sequence, fiberNo);
    FiberMutex::ScopedLock lock(mutex);
    MORDOR_TEST_ASSERT_EQUAL(++sequence, fiberNo + 3 + 1);
}

MORDOR_UNITTEST(FiberMutex, contention)
{
    Fiber::ptr mainFiber(new Fiber());
    WorkerPool pool;
    FiberMutex mutex;
    int sequence = 0;
    Fiber::ptr fiber1(new Fiber(NULL)), fiber2(new Fiber(NULL)),
        fiber3(new Fiber(NULL));
    fiber1->reset(boost::bind(&contentionFiber, 1, boost::ref(mutex),
        boost::ref(sequence)));
    fiber2->reset(boost::bind(&contentionFiber, 2, boost::ref(mutex),
        boost::ref(sequence)));
    fiber3->reset(boost::bind(&contentionFiber, 3, boost::ref(mutex),
        boost::ref(sequence)));

    {
        FiberMutex::ScopedLock lock(mutex);
        pool.schedule(fiber1);
        pool.schedule(fiber2);
        pool.schedule(fiber3);
        pool.dispatch();
        MORDOR_TEST_ASSERT_EQUAL(++sequence, 4);
    }
    pool.dispatch();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 8);
}

#ifdef DEBUG
MORDOR_UNITTEST(FiberMutex, notRecursive)
{
    Fiber::ptr mainFiber(new Fiber());
    WorkerPool pool;
    FiberMutex mutex;

    FiberMutex::ScopedLock lock(mutex);
    MORDOR_TEST_ASSERT_ASSERTED(mutex.lock());
}
#endif
