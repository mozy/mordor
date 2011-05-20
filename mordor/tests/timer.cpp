// Copyright (c) 2009 - Mozy, Inc.

#include <boost/bind.hpp>

#include "mordor/timer.h"
#include "mordor/test/test.h"

using namespace Mordor;
using namespace Mordor::Test;

static void
singleTimer(int &sequence, int &expected)
{
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, expected);
}

MORDOR_UNITTEST(Timer, single)
{
    int sequence = 0;
    TimerManager manager;
    MORDOR_TEST_ASSERT_EQUAL(manager.nextTimer(), ~0ull);
    manager.registerTimer(0, boost::bind(&singleTimer, boost::ref(sequence), 1));
    MORDOR_TEST_ASSERT_EQUAL(manager.nextTimer(), 0u);
    MORDOR_TEST_ASSERT_EQUAL(sequence, 0);
    manager.executeTimers();
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, 2);
    MORDOR_TEST_ASSERT_EQUAL(manager.nextTimer(), ~0ull);
}

MORDOR_UNITTEST(Timer, multiple)
{
    int sequence = 0;
    TimerManager manager;
    MORDOR_TEST_ASSERT_EQUAL(manager.nextTimer(), ~0ull);
    manager.registerTimer(0, boost::bind(&singleTimer, boost::ref(sequence),
        boost::ref(sequence)));
    manager.registerTimer(0, boost::bind(&singleTimer, boost::ref(sequence),
        boost::ref(sequence)));
    MORDOR_TEST_ASSERT_EQUAL(manager.nextTimer(), 0u);
    MORDOR_TEST_ASSERT_EQUAL(sequence, 0);
    manager.executeTimers();
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, 3);
    MORDOR_TEST_ASSERT_EQUAL(manager.nextTimer(), ~0ull);
}

MORDOR_UNITTEST(Timer, cancel)
{
    int sequence = 0;
    TimerManager manager;
    MORDOR_TEST_ASSERT_EQUAL(manager.nextTimer(), ~0ull);
    Timer::ptr timer =
        manager.registerTimer(0, boost::bind(&singleTimer, boost::ref(sequence), 1));
    MORDOR_TEST_ASSERT_EQUAL(manager.nextTimer(), 0u);
    timer->cancel();
    MORDOR_TEST_ASSERT_EQUAL(manager.nextTimer(), ~0ull);
    manager.executeTimers();
    MORDOR_TEST_ASSERT_EQUAL(sequence, 0);
}

MORDOR_UNITTEST(Timer, idempotentCancel)
{
    int sequence = 0;
    TimerManager manager;
    MORDOR_TEST_ASSERT_EQUAL(manager.nextTimer(), ~0ull);
    Timer::ptr timer =
        manager.registerTimer(0, boost::bind(&singleTimer, boost::ref(sequence), 1));
    MORDOR_TEST_ASSERT_EQUAL(manager.nextTimer(), 0u);
    timer->cancel();
    timer->cancel();
    MORDOR_TEST_ASSERT_EQUAL(manager.nextTimer(), ~0ull);
    manager.executeTimers();
    MORDOR_TEST_ASSERT_EQUAL(sequence, 0);
}

MORDOR_UNITTEST(Timer, idempotentCancelAfterSuccess)
{
    int sequence = 0;
    TimerManager manager;
    MORDOR_TEST_ASSERT_EQUAL(manager.nextTimer(), ~0ull);
    Timer::ptr timer =
        manager.registerTimer(0, boost::bind(&singleTimer, boost::ref(sequence), 1));
    MORDOR_TEST_ASSERT_EQUAL(manager.nextTimer(), 0u);
    MORDOR_TEST_ASSERT_EQUAL(sequence, 0);
    manager.executeTimers();
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, 2);
    MORDOR_TEST_ASSERT_EQUAL(manager.nextTimer(), ~0ull);
    timer->cancel();
    timer->cancel();
}


MORDOR_UNITTEST(Timer, recurring)
{
    int sequence = 0;
    int expected;
    TimerManager manager;
    MORDOR_TEST_ASSERT_EQUAL(manager.nextTimer(), ~0ull);
    Timer::ptr timer =
        manager.registerTimer(0, boost::bind(&singleTimer, boost::ref(sequence),
        boost::ref(expected)), true);
    MORDOR_TEST_ASSERT_EQUAL(manager.nextTimer(), 0u);
    MORDOR_TEST_ASSERT_EQUAL(sequence, 0);
    expected = 1;
    manager.executeTimers();
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, 2);
    MORDOR_TEST_ASSERT_EQUAL(manager.nextTimer(), 0u);
    expected = 3;
    manager.executeTimers();
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, 4);
    timer->cancel();
    MORDOR_TEST_ASSERT_EQUAL(manager.nextTimer(), ~0ull);
}

MORDOR_UNITTEST(Timer, later)
{
    int sequence = 0;
    TimerManager manager;
    MORDOR_TEST_ASSERT_EQUAL(manager.nextTimer(), ~0ull);
    Timer::ptr timer = manager.registerTimer(1000 * 1000 * 1000,
        boost::bind(&singleTimer, boost::ref(sequence), 1));
    MORDOR_TEST_ASSERT_ABOUT_EQUAL(manager.nextTimer(),
        1000 * 1000 * 1000u, 100 * 1000 * 1000u);
    MORDOR_TEST_ASSERT_EQUAL(sequence, 0);
    manager.executeTimers();
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);
    timer->cancel();
    MORDOR_TEST_ASSERT_EQUAL(manager.nextTimer(), ~0ull);
}

static unsigned long long fakeClock(unsigned long long &clock)
{
    return clock;
}

MORDOR_UNITTEST(Timer, rollover)
{
    // two minutes before the apocalypse
    static unsigned long long clock = 0ULL - 120000000;
    TimerManager::setClock(boost::bind(&fakeClock, boost::ref(clock)));
    
    int sequence = 0;
    TimerManager manager;

    // sanity check - test passage of time
    Timer::ptr timer1 = manager.registerTimer(60000000,
        boost::bind(&singleTimer, boost::ref(sequence), boost::ref(sequence)));
    MORDOR_TEST_ASSERT_EQUAL(manager.nextTimer(), 60000000ULL);
    clock += 30000000;
    manager.executeTimers();
    MORDOR_TEST_ASSERT_EQUAL(sequence, 0);  // timer hasn't fired yet
    MORDOR_TEST_ASSERT_EQUAL(manager.nextTimer(), 30000000ULL); // timer is still 30 seconds out
    
    // now create a few more timers for good measure
    Timer::ptr timer2 = manager.registerTimer(15000000,     // pre-rollover
        boost::bind(&singleTimer, boost::ref(sequence), boost::ref(sequence)));
    MORDOR_TEST_ASSERT_EQUAL(manager.nextTimer(), 15000000ULL);
    Timer::ptr timer3 = manager.registerTimer(180000000,    // post-rollover
        boost::bind(&singleTimer, boost::ref(sequence), boost::ref(sequence)));
    // nextTimer() would return 0 now, because timer3 appears to be in the past

    clock += 120000000; // overflow!!
    manager.executeTimers();
    MORDOR_TEST_ASSERT_EQUAL(sequence, 3);  // all timers should have fired
    MORDOR_TEST_ASSERT_EQUAL(manager.nextTimer(), ~0ULL);   // no timers left

    TimerManager::setClock();
}
