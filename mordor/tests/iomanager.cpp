// Copyright (c) 2009 - Mozy, Inc.

#include <boost/bind.hpp>

#include "mordor/future.h"
#include "mordor/iomanager.h"
#include "mordor/test/test.h"

using namespace Mordor;
using namespace Mordor::Test;

namespace
{
    class EmptyTimeClass
    {
    public:
        typedef boost::shared_ptr<EmptyTimeClass> ptr;
    public:
        EmptyTimeClass()
            : m_timedOut(false)
            , m_future(NULL)
        {}

        void onTimer() {
            m_timedOut = true;
            if (m_future)
                m_future->signal();
        }

        void setFuture(Future<> *future) { m_future = future; }
        bool timedOut() const { return m_timedOut; }

    private:
        bool m_timedOut;
        Future<> * m_future;
    };

    void testTimerExpired(IOManager &manager)
    {
        Future<> future;
        EmptyTimeClass::ptr tester(new EmptyTimeClass());
        tester->setFuture(&future);
        MORDOR_TEST_ASSERT(tester.unique());
        manager.registerTimer(1000, boost::bind(&EmptyTimeClass::onTimer, tester));
        MORDOR_TEST_ASSERT(!tester.unique());
        // wait until timed out
        future.wait();
        MORDOR_TEST_ASSERT(tester->timedOut());
        MORDOR_TEST_ASSERT(tester.unique());
    }

    void testTimerNoExpire(IOManager &manager)
    {
        EmptyTimeClass::ptr tester(new EmptyTimeClass());
        MORDOR_TEST_ASSERT(tester.unique());
        Timer::ptr timer = manager.registerTimer(30000000,
            boost::bind(&EmptyTimeClass::onTimer, tester));
        MORDOR_TEST_ASSERT(!tester.unique());
        timer->cancel();
        MORDOR_TEST_ASSERT(!tester->timedOut());
        MORDOR_TEST_ASSERT(tester.unique());
    }
}

static void
singleTimer(int &sequence, int &expected)
{
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, expected);
}

MORDOR_UNITTEST(IOManager, singleTimer)
{
    int sequence = 0;
    IOManager manager;
    manager.registerTimer(0, boost::bind(&singleTimer, boost::ref(sequence), 1));
    MORDOR_TEST_ASSERT_EQUAL(sequence, 0);
    manager.dispatch();
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, 2);
}

MORDOR_UNITTEST(IOManager, laterTimer)
{
    int sequence = 0;
    IOManager manager;
    manager.registerTimer(100000, boost::bind(&singleTimer, boost::ref(sequence), 1));
    MORDOR_TEST_ASSERT_EQUAL(sequence, 0);
    manager.dispatch();
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, 2);
}

MORDOR_UNITTEST(IOManager, timerRefCountNoExpired)
{
    IOManager manager;
    manager.schedule(boost::bind(testTimerNoExpire, boost::ref(manager)));
    manager.dispatch();
}

MORDOR_UNITTEST(IOManager, timerRefCountExpired)
{
    IOManager manager;
    manager.schedule(boost::bind(testTimerExpired, boost::ref(manager)));
    manager.dispatch();
}
