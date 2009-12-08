// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include "mordor/fiber.h"
#include "mordor/scheduler.h"
#include "mordor/test/test.h"

using namespace Mordor;

static void basic(FiberLocalStorage<int> &fls)
{
    MORDOR_TEST_ASSERT_EQUAL(fls.get(), 0);
    fls = 2;
    MORDOR_TEST_ASSERT_EQUAL(fls.get(), 2);
    Fiber::yield();
    MORDOR_TEST_ASSERT_EQUAL(fls.get(), 2);
    fls = 4;
    MORDOR_TEST_ASSERT_EQUAL(fls.get(), 4);
    Fiber::yield();
    MORDOR_TEST_ASSERT_EQUAL(fls.get(), 4);
    fls = 6;
    MORDOR_TEST_ASSERT_EQUAL(fls.get(), 6);
}

static void thread(FiberLocalStorage<int> &fls,
                   Fiber::ptr fiber)
{
    MORDOR_TEST_ASSERT_EQUAL(fls.get(), 0);
    fls = 3;
    MORDOR_TEST_ASSERT_EQUAL(fls.get(), 3);
    fiber->call();
    MORDOR_TEST_ASSERT_EQUAL(fls.get(), 3);
    fls = 5;
    MORDOR_TEST_ASSERT_EQUAL(fls.get(), 5);
}

MORDOR_UNITTEST(FLS, basic)
{
    FiberLocalStorage<int> fls;
    MORDOR_TEST_ASSERT_EQUAL(fls.get(), 0);
    fls = 1;
    MORDOR_TEST_ASSERT_EQUAL(fls.get(), 1);

    Fiber::ptr fiber(new Fiber(boost::bind(&basic, boost::ref(fls))));
    fiber->call();
    MORDOR_TEST_ASSERT_EQUAL(fls.get(), 1);

    boost::thread thread1(boost::bind(&thread, boost::ref(fls), fiber));
    thread1.join();
    MORDOR_TEST_ASSERT_EQUAL(fls.get(), 1);
    fiber->call();
    MORDOR_TEST_ASSERT_EQUAL(fls.get(), 1);
}
