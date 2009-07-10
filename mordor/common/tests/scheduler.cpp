// Copyright (c) 2009 - Decho Corp.

#include "mordor/test/test.h"

#include <boost/bind.hpp>

#include "mordor/common/scheduler.h"

SUITE_INVARIANT(Scheduler)
{
    TEST_ASSERT(!Fiber::getThis());
    TEST_ASSERT(!Scheduler::getThis());
}

static void doNothing()
{
}

// When hijacking the calling thread, you don't need to explicitly start
// or stop the scheduler; it starts on construction, and stops when
// there are no more fibers waiting to be scheduled (returning to the
// fiber that first yielded to it)
TEST_WITH_SUITE(Scheduler, hijackThread)
{
    Fiber::ptr mainFiber(new Fiber());
    Fiber::ptr doNothingFiber(new Fiber(&doNothing));
    WorkerPool pool;
    TEST_ASSERT_EQUAL(Scheduler::getThis(), &pool);
    pool.schedule(doNothingFiber);
    TEST_ASSERT_EQUAL(doNothingFiber->state(), Fiber::INIT);
    pool.yieldTo();
    TEST_ASSERT_EQUAL(doNothingFiber->state(), Fiber::TERM);
}

// Similar to above, but after the scheduler has stopped, yielding
// to it again should implicitly restart it
TEST_WITH_SUITE(Scheduler, hijackThreadAutoReset)
{
    Fiber::ptr mainFiber(new Fiber());
    Fiber::ptr doNothingFiber(new Fiber(&doNothing));
    WorkerPool pool;
    TEST_ASSERT_EQUAL(Scheduler::getThis(), &pool);
    pool.schedule(doNothingFiber);
    TEST_ASSERT_EQUAL(doNothingFiber->state(), Fiber::INIT);
    pool.yieldTo();
    TEST_ASSERT_EQUAL(doNothingFiber->state(), Fiber::TERM);
    doNothingFiber->reset();
    pool.schedule(doNothingFiber);
    TEST_ASSERT_EQUAL(doNothingFiber->state(), Fiber::INIT);
    pool.yieldTo();
    TEST_ASSERT_EQUAL(doNothingFiber->state(), Fiber::TERM);
}

// TODO: could improve this test by having two fibers that
// synchronize and assert that they are on different threads
TEST_WITH_SUITE(Scheduler, hijackThreadPlusMore)
{
    Fiber::ptr mainFiber(new Fiber());
    Fiber::ptr doNothingFiber(new Fiber(&doNothing));
    WorkerPool pool(2);
    TEST_ASSERT_EQUAL(Scheduler::getThis(), &pool);
    pool.schedule(doNothingFiber);
    TEST_ASSERT_EQUAL(doNothingFiber->state(), Fiber::INIT);
    pool.schedule(Fiber::getThis());
    pool.yieldTo();
    TEST_ASSERT_EQUAL(doNothingFiber->state(), Fiber::TERM);
    pool.stop();
}

void
otherThreadProc(Scheduler *scheduler, bool &done)
{
    TEST_ASSERT_EQUAL(Scheduler::getThis(), scheduler);
    done = true;
}

TEST_WITH_SUITE(Scheduler, otherThread)
{
    bool done = false;
    Fiber::ptr mainFiber(new Fiber());
    WorkerPool pool(1, false);
    Fiber::ptr f(new Fiber(
        boost::bind(&otherThreadProc, &pool, boost::ref(done))));
    TEST_ASSERT(!Scheduler::getThis());
    TEST_ASSERT_EQUAL(f->state(), Fiber::INIT);
    TEST_ASSERT(!done);
    pool.schedule(f);
    volatile bool &doneVolatile = done;
    while (!doneVolatile);
    pool.stop();
    TEST_ASSERT_EQUAL(f->state(), Fiber::TERM);
}

void
runInContext(Scheduler &poolA, Scheduler &poolB)
{
    SchedulerSwitcher switcher(&poolB);
    TEST_ASSERT_EQUAL(Scheduler::getThis(), &poolB);
    TEST_ASSERT_NOT_EQUAL(Scheduler::getThis(), &poolA);
    throw std::runtime_error("pass through context switch");
}

TEST_WITH_SUITE(Scheduler, switcherExceptions)
{
    Fiber::ptr mainFiber(new Fiber());
    WorkerPool poolA(1, true, false), poolB(1, false);

    TEST_ASSERT_EQUAL(Scheduler::getThis(), &poolA);
    TEST_ASSERT_NOT_EQUAL(Scheduler::getThis(), &poolB);
    
    TEST_ASSERT_EXCEPTION(runInContext(poolA, poolB), std::runtime_error);

    TEST_ASSERT_EQUAL(Scheduler::getThis(), &poolA);
    TEST_ASSERT_NOT_EQUAL(Scheduler::getThis(), &poolB);
    poolB.stop();
    poolA.stop();
}
