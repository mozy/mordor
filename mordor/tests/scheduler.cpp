// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/pch.h"

#include <boost/bind.hpp>

#include <boost/exception.hpp>
#include "mordor/exception.h"
#include "mordor/scheduler.h"
#include "mordor/test/test.h"

using namespace Mordor;
using namespace Mordor::Test;

MORDOR_SUITE_INVARIANT(Scheduler)
{
    MORDOR_TEST_ASSERT(!Scheduler::getThis());
}

static void doNothing()
{
}

// Stop can be called multiple times without consequence
MORDOR_UNITTEST(Scheduler, idempotentStopHijack)
{
    WorkerPool pool;
    pool.stop();
    pool.stop();
}

MORDOR_UNITTEST(Scheduler, idempotentStopHybrid)
{
    WorkerPool pool(2);
    pool.stop();
    pool.stop();
}

MORDOR_UNITTEST(Scheduler, idempotentStopSpawn)
{
    WorkerPool pool(1, false);
    pool.stop();
    pool.stop();
}

// When hijacking the calling thread, you don't need to explicitly start
// or stop the scheduler; it starts on the first yieldTo, and stops on
// destruction
MORDOR_UNITTEST(Scheduler, hijackBasic)
{
    Fiber::ptr doNothingFiber(new Fiber(&doNothing));
    WorkerPool pool;
    MORDOR_TEST_ASSERT_EQUAL(Scheduler::getThis(), &pool);
    pool.schedule(doNothingFiber);
    MORDOR_TEST_ASSERT_EQUAL(doNothingFiber->state(), Fiber::INIT);
    pool.dispatch();
    MORDOR_TEST_ASSERT_EQUAL(doNothingFiber->state(), Fiber::TERM);
}

// Similar to above, but after the scheduler has stopped, yielding
// to it again should implicitly restart it
MORDOR_UNITTEST(Scheduler, hijackMultipleDispatch)
{
    Fiber::ptr doNothingFiber(new Fiber(&doNothing));
    WorkerPool pool;
    MORDOR_TEST_ASSERT_EQUAL(Scheduler::getThis(), &pool);
    pool.schedule(doNothingFiber);
    MORDOR_TEST_ASSERT_EQUAL(doNothingFiber->state(), Fiber::INIT);
    pool.dispatch();
    MORDOR_TEST_ASSERT_EQUAL(doNothingFiber->state(), Fiber::TERM);
    doNothingFiber->reset();
    pool.schedule(doNothingFiber);
    MORDOR_TEST_ASSERT_EQUAL(doNothingFiber->state(), Fiber::INIT);
    pool.dispatch();
    MORDOR_TEST_ASSERT_EQUAL(doNothingFiber->state(), Fiber::TERM);
}

// TODO: could improve this test by having two fibers that
// synchronize and MORDOR_ASSERT( that they are on different threads
MORDOR_UNITTEST(Scheduler, hybridBasic)
{
    Fiber::ptr doNothingFiber(new Fiber(&doNothing));
    WorkerPool pool(2);
    MORDOR_TEST_ASSERT_EQUAL(Scheduler::getThis(), &pool);
    MORDOR_TEST_ASSERT_EQUAL(doNothingFiber->state(), Fiber::INIT);
    pool.schedule(doNothingFiber);
    pool.schedule(Fiber::getThis());
    pool.yieldTo();
    pool.stop();
    MORDOR_TEST_ASSERT_EQUAL(doNothingFiber->state(), Fiber::TERM);
}

void
otherThreadProc(Scheduler *scheduler, bool &done)
{
    MORDOR_TEST_ASSERT_EQUAL(Scheduler::getThis(), scheduler);
    done = true;
}

MORDOR_UNITTEST(Scheduler, spawnBasic)
{
    bool done = false;
    WorkerPool pool(1, false);
    Fiber::ptr f(new Fiber(
        boost::bind(&otherThreadProc, &pool, boost::ref(done))));
    MORDOR_TEST_ASSERT(!Scheduler::getThis());
    MORDOR_TEST_ASSERT_EQUAL(f->state(), Fiber::INIT);
    MORDOR_TEST_ASSERT(!done);
    pool.schedule(f);
    volatile bool &doneVolatile = done;
    while (!doneVolatile);
    pool.stop();
    MORDOR_TEST_ASSERT_EQUAL(f->state(), Fiber::TERM);
}

MORDOR_UNITTEST(Scheduler, switchToStress)
{
    WorkerPool poolA(1, true), poolB(1, false);

    // Ensure we return to poolA
    SchedulerSwitcher switcher;
    for (int i = 0; i < 1000; ++i) {
        if (i % 2) {
            poolA.switchTo();
            MORDOR_TEST_ASSERT_EQUAL(Scheduler::getThis(), &poolA);
        } else {
            poolB.switchTo();
            MORDOR_TEST_ASSERT_EQUAL(Scheduler::getThis(), &poolB);
        }
    }
}

void
runInContext(Scheduler &poolA, Scheduler &poolB)
{
    SchedulerSwitcher switcher(&poolB);
    MORDOR_TEST_ASSERT_EQUAL(Scheduler::getThis(), &poolB);
    MORDOR_TEST_ASSERT_NOT_EQUAL(Scheduler::getThis(), &poolA);
    MORDOR_THROW_EXCEPTION(OperationAbortedException());
}

MORDOR_UNITTEST(Scheduler, switcherExceptions)
{
    WorkerPool poolA(1, true), poolB(1, false);

    MORDOR_TEST_ASSERT_EQUAL(Scheduler::getThis(), &poolA);
    MORDOR_TEST_ASSERT_NOT_EQUAL(Scheduler::getThis(), &poolB);
    
    MORDOR_TEST_ASSERT_EXCEPTION(runInContext(poolA, poolB), OperationAbortedException);

    MORDOR_TEST_ASSERT_EQUAL(Scheduler::getThis(), &poolA);
    MORDOR_TEST_ASSERT_NOT_EQUAL(Scheduler::getThis(), &poolB);
}

static void increment(int &total)
{
    ++total;
}

MORDOR_UNITTEST(Scheduler, parallelDo)
{
    WorkerPool pool;

    int total = 0;
    std::vector<boost::function<void ()> > dgs;
    dgs.push_back(boost::bind(&increment, boost::ref(total)));
    dgs.push_back(boost::bind(&increment, boost::ref(total)));

    parallel_do(dgs);
    MORDOR_TEST_ASSERT_EQUAL(total, 2);
}

static void exception()
{
    MORDOR_THROW_EXCEPTION(OperationAbortedException());
}

MORDOR_UNITTEST(Scheduler, parallelDoException)
{
    WorkerPool pool;

    std::vector<boost::function<void ()> > dgs;
    dgs.push_back(&exception);
    dgs.push_back(&exception);

    MORDOR_TEST_ASSERT_EXCEPTION(parallel_do(dgs), OperationAbortedException);
}

static bool checkEqual(int x, int &sequence)
{
    MORDOR_TEST_ASSERT_EQUAL(x, sequence);
    ++sequence;
    return true;
}

MORDOR_UNITTEST(Scheduler, parallelForEach)
{
    const int values[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    WorkerPool pool;

    int sequence = 1;
    parallel_foreach<const int *, const int>(&values[0], &values[10], boost::bind(
        &checkEqual, _1, boost::ref(sequence)), 4);
    MORDOR_TEST_ASSERT_EQUAL(sequence, 11);
}

MORDOR_UNITTEST(Scheduler, parallelForEachLessThanParallelism)
{
    const int values[] = { 1, 2 };
    WorkerPool pool;

    int sequence = 1;
    parallel_foreach<const int *, const int>(&values[0], &values[2], boost::bind(
        &checkEqual, _1, boost::ref(sequence)), 4);
    MORDOR_TEST_ASSERT_EQUAL(sequence, 3);
}

static bool checkEqualStop5(int x, int &sequence)
{
    MORDOR_TEST_ASSERT_EQUAL(x, sequence);
    ++sequence;
    return sequence <= 5;
}

MORDOR_UNITTEST(Scheduler, parallelForEachStopShort)
{
    const int values[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    WorkerPool pool;

    int sequence = 1;
    parallel_foreach<const int *, const int>(&values[0], &values[10], boost::bind(
        &checkEqualStop5, _1, boost::ref(sequence)), 4);
    // 5 was told to stop, 6, 7, and 8 were already scheduled
    MORDOR_TEST_ASSERT_EQUAL(sequence, 9);
}

static bool checkEqualExceptionOn5(int x, int &sequence)
{
    MORDOR_TEST_ASSERT_EQUAL(x, sequence);
    ++sequence;
    if (sequence == 6)
        MORDOR_THROW_EXCEPTION(OperationAbortedException());
    return true;
}

MORDOR_UNITTEST(Scheduler, parallelForEachException)
{
    const int values[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    WorkerPool pool;

    int sequence = 1;
    try {
        parallel_foreach<const int *, const int>(&values[0], &values[10], boost::bind(
            &checkEqualExceptionOn5, _1, boost::ref(sequence)), 4);
        MORDOR_TEST_ASSERT(false);
    } catch (OperationAbortedException)
    {}
    // 5 was told to stop (exception), 6, 7, and 8 were already scheduled
    MORDOR_TEST_ASSERT_EQUAL(sequence, 9);
}

#ifdef DEBUG
MORDOR_UNITTEST(Scheduler, scheduleForThreadNotOnScheduler)
{
    Fiber::ptr doNothingFiber(new Fiber(&doNothing));
    WorkerPool pool(1, false);
    MORDOR_TEST_ASSERT_ASSERTED(pool.schedule(doNothingFiber, boost::this_thread::get_id()));
    pool.stop();
}
#endif
