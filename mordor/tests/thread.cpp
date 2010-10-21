// Copyright (c) 2010 - Mozy, Inc.

#include <boost/bind.hpp>

#include "mordor/test/test.h"
#include "mordor/thread.h"

using namespace Mordor;

static void threadFunc(tid_t &myTid)
{
    myTid = gettid();
}

// Tests to make sure the tid as reported by Thread object and inside the
// thread itself are the same (and have a valid value)
MORDOR_UNITTEST(Thread, correctTID)
{
    tid_t tid = emptytid();
    Thread t(boost::bind(&threadFunc, boost::ref(tid)));
    t.join();
    MORDOR_TEST_ASSERT_NOT_EQUAL(tid, emptytid());
    MORDOR_TEST_ASSERT_EQUAL(tid, t.tid());
}
