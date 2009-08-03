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

// Stop can be called multiple times without consequence
TEST_WITH_SUITE(Scheduler, idempotentStopHijack)
{
    Fiber::ptr mainFiber(new Fiber());
    WorkerPool pool;
    pool.stop();
    pool.stop();
}

TEST_WITH_SUITE(Scheduler, idempotentStopHybrid)
{
    Fiber::ptr mainFiber(new Fiber());
    WorkerPool pool(2);
    pool.stop();
    pool.stop();
}

TEST_WITH_SUITE(Scheduler, idempotentStopSpawn)
{
    Fiber::ptr mainFiber(new Fiber());
    WorkerPool pool(1, false);
    pool.stop();
    pool.stop();
}

// When hijacking the calling thread, you don't need to explicitly start
// or stop the scheduler; it starts on the first yieldTo, and stops on
// destruction
TEST_WITH_SUITE(Scheduler, hijackBasic)
{
    Fiber::ptr mainFiber(new Fiber());
    Fiber::ptr doNothingFiber(new Fiber(&doNothing));
    WorkerPool pool;
    TEST_ASSERT_EQUAL(Scheduler::getThis(), &pool);
    pool.schedule(doNothingFiber);
    TEST_ASSERT_EQUAL(doNothingFiber->state(), Fiber::INIT);
    pool.dispatch();
    TEST_ASSERT_EQUAL(doNothingFiber->state(), Fiber::TERM);
}

// Similar to above, but after the scheduler has stopped, yielding
// to it again should implicitly restart it
TEST_WITH_SUITE(Scheduler, hijackMultipleDispatch)
{
    Fiber::ptr mainFiber(new Fiber());
    Fiber::ptr doNothingFiber(new Fiber(&doNothing));
    WorkerPool pool;
    TEST_ASSERT_EQUAL(Scheduler::getThis(), &pool);
    pool.schedule(doNothingFiber);
    TEST_ASSERT_EQUAL(doNothingFiber->state(), Fiber::INIT);
    pool.dispatch();
    TEST_ASSERT_EQUAL(doNothingFiber->state(), Fiber::TERM);
    doNothingFiber->reset();
    pool.schedule(doNothingFiber);
    TEST_ASSERT_EQUAL(doNothingFiber->state(), Fiber::INIT);
    pool.dispatch();
    TEST_ASSERT_EQUAL(doNothingFiber->state(), Fiber::TERM);
}

// TODO: could improve this test by having two fibers that
// synchronize and ASSERT( that they are on different threads
TEST_WITH_SUITE(Scheduler, hybridBasic)
{
    Fiber::ptr mainFiber(new Fiber());
    Fiber::ptr doNothingFiber(new Fiber(&doNothing));
    WorkerPool pool(2);
    TEST_ASSERT_EQUAL(Scheduler::getThis(), &pool);
    TEST_ASSERT_EQUAL(doNothingFiber->state(), Fiber::INIT);
    pool.schedule(doNothingFiber);
    pool.schedule(Fiber::getThis());
    pool.yieldTo();
    pool.stop();
    TEST_ASSERT_EQUAL(doNothingFiber->state(), Fiber::TERM);
}

void
otherThreadProc(Scheduler *scheduler, bool &done)
{
    TEST_ASSERT_EQUAL(Scheduler::getThis(), scheduler);
    done = true;
}

TEST_WITH_SUITE(Scheduler, spawnBasic)
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

TEST_WITH_SUITE(Scheduler, switchToStress)
{
    Fiber::ptr mainFiber(new Fiber());
    WorkerPool poolA(1, true), poolB(1, false);

    // Ensure we return to poolA
    SchedulerSwitcher switcher;
    for (int i = 0; i < 10000; ++i) {
        if (i % 2) {
            poolA.switchTo();
            TEST_ASSERT_EQUAL(Scheduler::getThis(), &poolA);
        } else {
            poolB.switchTo();
            TEST_ASSERT_EQUAL(Scheduler::getThis(), &poolB);
        }
    }
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
    WorkerPool poolA(1, true), poolB(1, false);

    TEST_ASSERT_EQUAL(Scheduler::getThis(), &poolA);
    TEST_ASSERT_NOT_EQUAL(Scheduler::getThis(), &poolB);
    
    TEST_ASSERT_EXCEPTION(runInContext(poolA, poolB), std::runtime_error);

    TEST_ASSERT_EQUAL(Scheduler::getThis(), &poolA);
    TEST_ASSERT_NOT_EQUAL(Scheduler::getThis(), &poolB);
}
