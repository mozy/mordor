// Copyright (c) 2009 - Mozy, Inc.

#include <boost/bind.hpp>

#include "mordor/future.h"
#include "mordor/test/test.h"
#include "mordor/workerpool.h"

using namespace Mordor;

static void setTrue(bool &signalled)
{
    signalled = true;
}

static void setResult(int &result, int value)
{
    result = value;
}

template <class T>
void signal(T &future)
{
    future.signal();
}

template <>
void signal<Future<int> >(Future<int> &future)
{
    future.result() = 1;
    future.signal();
}

static void setTrueScheduler(bool &signalled, Scheduler *scheduler)
{
    signalled = true;
    MORDOR_TEST_ASSERT_EQUAL(scheduler, Scheduler::getThis());
}

static void setResultScheduler(int &result, int value, Scheduler *scheduler)
{
    result = value;
    MORDOR_TEST_ASSERT_EQUAL(scheduler, Scheduler::getThis());
}

MORDOR_UNITTEST(Future, synchronousYield)
{
    Future<> future;
    future.signal();
    future.wait();
}

MORDOR_UNITTEST(Future, asynchronousYield)
{
    WorkerPool pool;
    Future<> future;
    pool.schedule(boost::bind(&signal<Future<> >, boost::ref(future)));
    future.wait();
}

MORDOR_UNITTEST(Future, synchronousDg)
{
    bool signalled = false;
    Future<> future(boost::bind(&setTrue, boost::ref(signalled)));
    future.signal();
    MORDOR_ASSERT(signalled);
}

MORDOR_UNITTEST(Future, asynchronousDg)
{
    bool signalled = false;
    Future<> future(boost::bind(&setTrue, boost::ref(signalled)));
    MORDOR_ASSERT(!signalled);
    future.signal();
    MORDOR_ASSERT(signalled);
}

MORDOR_UNITTEST(Future, synchronousDgOtherScheduler)
{
    WorkerPool pool(1, false);
    bool signalled = false;
    Future<> future(boost::bind(&setTrueScheduler, boost::ref(signalled), &pool), &pool);
    future.signal();
    pool.stop();
    MORDOR_ASSERT(signalled);
}

MORDOR_UNITTEST(Future, asynchronousDgOtherScheduler)
{
    WorkerPool pool(1, false);
    bool signalled = false;
    Future<> future(boost::bind(&setTrueScheduler, boost::ref(signalled), &pool), &pool);
    MORDOR_ASSERT(!signalled);
    future.signal();
    pool.stop();
    MORDOR_ASSERT(signalled);
}

MORDOR_UNITTEST(Future, synchronousYieldInt)
{
    Future<int> future;
    future.signal();
    future.wait();
}

MORDOR_UNITTEST(Future, asynchronousYieldInt)
{
    WorkerPool pool;
    Future<int> future;
    MORDOR_TEST_ASSERT_EQUAL(future.result(), 0);
    pool.schedule(boost::bind(&signal<Future<int> >, boost::ref(future)));
    MORDOR_TEST_ASSERT_EQUAL(future.wait(), 1);
}

MORDOR_UNITTEST(Future, synchronousDgInt)
{
    int result = 0;
    Future<int> future(boost::bind(&setResult, boost::ref(result), _1));
    future.result() = 1;
    future.signal();
    MORDOR_TEST_ASSERT_EQUAL(result, 1);
}

MORDOR_UNITTEST(Future, asynchronousDgInt)
{
    int result = 0;
    Future<int> future(boost::bind(&setResult, boost::ref(result), _1));
    MORDOR_TEST_ASSERT_EQUAL(result, 0);
    future.result() = 1;
    future.signal();
    MORDOR_TEST_ASSERT_EQUAL(result, 1);
}

MORDOR_UNITTEST(Future, synchronousDgIntOtherScheduler)
{
    WorkerPool pool(1, false);
    int result = 0;
    Future<int> future(boost::bind(&setResultScheduler, boost::ref(result), _1, &pool), &pool);
    future.result() = 1;
    future.signal();
    pool.stop();
    MORDOR_TEST_ASSERT_EQUAL(result, 1);
}

MORDOR_UNITTEST(Future, asynchronousDgIntOtherScheduler)
{
    WorkerPool pool(1, false);
    int result = 0;
    Future<int> future(boost::bind(&setResultScheduler, boost::ref(result), _1, &pool), &pool);
    MORDOR_TEST_ASSERT_EQUAL(result, 0);
    future.result() = 1;
    future.signal();
    pool.stop();
    MORDOR_TEST_ASSERT_EQUAL(result, 1);
}

MORDOR_UNITTEST(Future, waitAllSynchronous)
{
    Future<> future[2];
    future[0].signal(); future[1].signal();
    waitAll(future, future + 2);
}

MORDOR_UNITTEST(Future, waitAllHalfSynchronous1)
{
    WorkerPool pool;
    Future<> future[2];
    future[0].signal();
    pool.schedule(boost::bind(&signal<Future<> >, boost::ref(future[1])));
    waitAll(future, future + 2);
}

MORDOR_UNITTEST(Future, waitAllHalfSynchronous2)
{
    WorkerPool pool;
    Future<> future[2];
    future[1].signal();
    pool.schedule(boost::bind(&signal<Future<> >, boost::ref(future[0])));
    waitAll(future, future + 2);
}

MORDOR_UNITTEST(Future, waitAllAsynchronous1)
{
    WorkerPool pool;
    Future<> future[2];
    pool.schedule(boost::bind(&signal<Future<> >, boost::ref(future[0])));
    pool.schedule(boost::bind(&signal<Future<> >, boost::ref(future[1])));
    waitAll(future, future + 2);
}

MORDOR_UNITTEST(Future, waitAllAsynchronous2)
{
    WorkerPool pool;
    Future<> future[2];
    pool.schedule(boost::bind(&signal<Future<> >, boost::ref(future[1])));
    pool.schedule(boost::bind(&signal<Future<> >, boost::ref(future[0])));
    waitAll(future, future + 2);
}

MORDOR_UNITTEST(Future, waitAnySynchronous1)
{
    Future<> future[2];
    future[0].signal();
    MORDOR_TEST_ASSERT_EQUAL(waitAny(future, future + 2), 0u);
    future[1].signal();
    waitAll(future, future + 2);
}

MORDOR_UNITTEST(Future, waitAnySynchronous2)
{
    Future<> future[2];
    future[1].signal();
    MORDOR_TEST_ASSERT_EQUAL(waitAny(future, future + 2), 1u);
    future[0].signal();
    waitAll(future, future + 2);
}

MORDOR_UNITTEST(Future, waitAnySynchronousBoth)
{
    Future<> future[2];
    future[0].signal(); future[1].signal();
    MORDOR_TEST_ASSERT_EQUAL(waitAny(future, future + 2), 0u);
    waitAll(future, future + 2);
    MORDOR_TEST_ASSERT_EQUAL(waitAny(future, future + 2), 0u);
}

MORDOR_UNITTEST(Future, waitAnyAsynchronous1)
{
    WorkerPool pool;
    Future<> future[2];
    pool.schedule(boost::bind(&signal<Future<> >, boost::ref(future[0])));
    MORDOR_TEST_ASSERT_EQUAL(waitAny(future, future + 2), 0u);
}

MORDOR_UNITTEST(Future, waitAnyAsynchronous2)
{
    WorkerPool pool;
    Future<> future[2];
    pool.schedule(boost::bind(&signal<Future<> >, boost::ref(future[1])));
    MORDOR_TEST_ASSERT_EQUAL(waitAny(future, future + 2), 1u);
}

MORDOR_UNITTEST(Future, waitAnyAsynchronousBoth)
{
    WorkerPool pool;
    Future<> future[2];
    pool.schedule(boost::bind(&signal<Future<> >, boost::ref(future[0])));
    pool.schedule(boost::bind(&signal<Future<> >, boost::ref(future[1])));
    MORDOR_TEST_ASSERT_EQUAL(waitAny(future, future + 2), 0u);
}
