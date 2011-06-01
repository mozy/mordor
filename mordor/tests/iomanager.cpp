// Copyright (c) 2009 - Mozy, Inc.

#include <boost/bind.hpp>

#include "mordor/iomanager.h"
#include "mordor/sleep.h"
#include "mordor/test/test.h"

using namespace Mordor;
using namespace Mordor::Test;

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

namespace {
class TickleAccessibleIOManager : public IOManager
{
public:
    void tickle() { IOManager::tickle(); }
};
}

MORDOR_UNITTEST(IOManager, lotsOfTickles)
{
    TickleAccessibleIOManager ioManager;
    // Need at least 64K iterations for Linux, but not more than 16K at once
    // for OS X
    for (int i = 0; i < 9; ++i) {
        for (int j = 0; j < 16000; ++j)
            ioManager.tickle();
        // Let the tickles drain
        sleep(ioManager, 250000ull);
    }
}
