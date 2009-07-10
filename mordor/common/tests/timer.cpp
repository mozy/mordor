// Copyright (c) 2009 - Decho Corp.

#include <boost/bind.hpp>

#include "mordor/common/timer.h"
#include "mordor/test/test.h"

void
singleTimer(int &sequence, int &expected)
{
    ++sequence;
    TEST_ASSERT_EQUAL(sequence, expected);
}

TEST_WITH_SUITE(Timer, single)
{
    int sequence = 0;
    TimerManager manager;
    TEST_ASSERT_EQUAL(manager.nextTimer(), ~0ull);
    manager.registerTimer(0, boost::bind(&singleTimer, boost::ref(sequence), 1));
    TEST_ASSERT_EQUAL(manager.nextTimer(), 0u);
    TEST_ASSERT_EQUAL(sequence, 0);
    manager.processTimers();
    ++sequence;
    TEST_ASSERT_EQUAL(sequence, 2);
    TEST_ASSERT_EQUAL(manager.nextTimer(), ~0ull);
}

TEST_WITH_SUITE(Timer, multiple)
{
    int sequence = 0;
    TimerManager manager;
    TEST_ASSERT_EQUAL(manager.nextTimer(), ~0ull);
    manager.registerTimer(0, boost::bind(&singleTimer, boost::ref(sequence),
        boost::ref(sequence)));
    manager.registerTimer(0, boost::bind(&singleTimer, boost::ref(sequence),
        boost::ref(sequence)));
    TEST_ASSERT_EQUAL(manager.nextTimer(), 0u);
    TEST_ASSERT_EQUAL(sequence, 0);
    manager.processTimers();
    ++sequence;
    TEST_ASSERT_EQUAL(sequence, 3);
    TEST_ASSERT_EQUAL(manager.nextTimer(), ~0ull);
}

TEST_WITH_SUITE(Timer, cancel)
{
    int sequence = 0;
    TimerManager manager;
    TEST_ASSERT_EQUAL(manager.nextTimer(), ~0ull);
    Timer::ptr timer =
        manager.registerTimer(0, boost::bind(&singleTimer, boost::ref(sequence), 1));
    TEST_ASSERT_EQUAL(manager.nextTimer(), 0u);
    timer->cancel();
    TEST_ASSERT_EQUAL(manager.nextTimer(), ~0ull);
    manager.processTimers();
    TEST_ASSERT_EQUAL(sequence, 0);
}

TEST_WITH_SUITE(Timer, recurring)
{
    int sequence = 0;
    int expected;
    TimerManager manager;
    TEST_ASSERT_EQUAL(manager.nextTimer(), ~0ull);
    Timer::ptr timer =
        manager.registerTimer(0, boost::bind(&singleTimer, boost::ref(sequence),
        boost::ref(expected)), true);
    TEST_ASSERT_EQUAL(manager.nextTimer(), 0u);
    TEST_ASSERT_EQUAL(sequence, 0);
    expected = 1;
    manager.processTimers();
    ++sequence;
    TEST_ASSERT_EQUAL(sequence, 2);
    TEST_ASSERT_EQUAL(manager.nextTimer(), 0u);
    expected = 3;
    manager.processTimers();
    ++sequence;
    TEST_ASSERT_EQUAL(sequence, 4);
    timer->cancel();
    TEST_ASSERT_EQUAL(manager.nextTimer(), ~0ull);
}

TEST_WITH_SUITE(Timer, later)
{
    int sequence = 0;
    TimerManager manager;
    TEST_ASSERT_EQUAL(manager.nextTimer(), ~0ull);
    Timer::ptr timer = manager.registerTimer(1000 * 1000 * 1000,
        boost::bind(&singleTimer, boost::ref(sequence), 1));
    TEST_ASSERT_ABOUT_EQUAL(manager.nextTimer(),
        1000 * 1000 * 1000u, 100 * 1000 * 1000u);
    TEST_ASSERT_EQUAL(sequence, 0);
    manager.processTimers();
    ++sequence;
    TEST_ASSERT_EQUAL(sequence, 1);
    timer->cancel();
    TEST_ASSERT_EQUAL(manager.nextTimer(), ~0ull);
}
