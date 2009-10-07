// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include <boost/bind.hpp>

#include <boost/exception.hpp>
#include "mordor/common/exception.h"
#include "mordor/common/scheduler.h"
#include "mordor/test/test.h"

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
    MORDOR_THROW_EXCEPTION(OperationAbortedException());
}

TEST_WITH_SUITE(Scheduler, switcherExceptions)
{
    Fiber::ptr mainFiber(new Fiber());
    WorkerPool poolA(1, true), poolB(1, false);

    TEST_ASSERT_EQUAL(Scheduler::getThis(), &poolA);
    TEST_ASSERT_NOT_EQUAL(Scheduler::getThis(), &poolB);
    
    TEST_ASSERT_EXCEPTION(runInContext(poolA, poolB), OperationAbortedException);

    TEST_ASSERT_EQUAL(Scheduler::getThis(), &poolA);
    TEST_ASSERT_NOT_EQUAL(Scheduler::getThis(), &poolB);
}

static void increment(int &total)
{
    ++total;
}

TEST_WITH_SUITE(Scheduler, parallelDo)
{
    Fiber::ptr mainFiber(new Fiber());
    WorkerPool pool;

    int total = 0;
    std::vector<boost::function<void ()> > dgs;
    dgs.push_back(boost::bind(&increment, boost::ref(total)));
    dgs.push_back(boost::bind(&increment, boost::ref(total)));

    parallel_do(dgs);
    TEST_ASSERT_EQUAL(total, 2);
}

static void exception()
{
    MORDOR_THROW_EXCEPTION(OperationAbortedException());
}

TEST_WITH_SUITE(Scheduler, parallelDoException)
{
    Fiber::ptr mainFiber(new Fiber());
    WorkerPool pool;

    std::vector<boost::function<void ()> > dgs;
    dgs.push_back(&exception);
    dgs.push_back(&exception);

    TEST_ASSERT_EXCEPTION(parallel_do(dgs), OperationAbortedException);
}

static bool checkEqual(int x, int &sequence)
{
    TEST_ASSERT_EQUAL(x, sequence);
    ++sequence;
    return true;
}

TEST_WITH_SUITE(Scheduler, parallelForEach)
{
    const int values[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    Fiber::ptr mainFiber(new Fiber());
    WorkerPool pool;

    int sequence = 1;
    parallel_foreach<const int *, const int>(&values[0], &values[10], boost::bind(
        &checkEqual, _1, boost::ref(sequence)), 4);
    TEST_ASSERT_EQUAL(sequence, 11);
}

TEST_WITH_SUITE(Scheduler, parallelForEachLessThanParallelism)
{
    const int values[] = { 1, 2 };
    Fiber::ptr mainFiber(new Fiber());
    WorkerPool pool;

    int sequence = 1;
    parallel_foreach<const int *, const int>(&values[0], &values[2], boost::bind(
        &checkEqual, _1, boost::ref(sequence)), 4);
    TEST_ASSERT_EQUAL(sequence, 3);
}

static bool checkEqualStop5(int x, int &sequence)
{
    TEST_ASSERT_EQUAL(x, sequence);
    ++sequence;
    return sequence <= 5;
}

TEST_WITH_SUITE(Scheduler, parallelForEachStopShort)
{
    const int values[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    Fiber::ptr mainFiber(new Fiber());
    WorkerPool pool;

    int sequence = 1;
    parallel_foreach<const int *, const int>(&values[0], &values[10], boost::bind(
        &checkEqualStop5, _1, boost::ref(sequence)), 4);
    // 5 was told to stop, 6, 7, and 8 were already scheduled
    TEST_ASSERT_EQUAL(sequence, 9);
}

static bool checkEqualExceptionOn5(int x, int &sequence)
{
    TEST_ASSERT_EQUAL(x, sequence);
    ++sequence;
    if (sequence == 6)
        MORDOR_THROW_EXCEPTION(OperationAbortedException());
    return true;
}

TEST_WITH_SUITE(Scheduler, parallelForEachException)
{
    const int values[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    Fiber::ptr mainFiber(new Fiber());
    WorkerPool pool;

    int sequence = 1;
    try {
        parallel_foreach<const int *, const int>(&values[0], &values[10], boost::bind(
            &checkEqualExceptionOn5, _1, boost::ref(sequence)), 4);
        TEST_ASSERT(false);
    } catch (OperationAbortedException)
    {}
    // 5 was told to stop (exception), 6, 7, and 8 were already scheduled
    TEST_ASSERT_EQUAL(sequence, 9);
}

/*
TEST_WITH_SUITE(Scheduler, uncaughtExceptionHijack)
{
    Fiber::ptr mainFiber(new Fiber());
    WorkerPool pool;

    pool.schedule(Fiber::ptr(new Fiber(&exception)));
    
    pool.dispatch();
    TEST_ASSERT_EXCEPTION(pool.dispatch(), OperationAbortedException);
}
*/
