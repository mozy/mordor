// Copyright (c) 2013 - Cody Cutrer

#include <boost/bind.hpp>

#include "mordor/rate_limiter.h"
#include "mordor/iomanager.h"
#include "mordor/sleep.h"
#include "mordor/test/test.h"

using namespace Mordor;
using namespace Mordor::Test;

static ConfigVar<size_t>::ptr g_countLimit = Config::lookup(
    "ratelimiter.count", (size_t)0u, "Config var used by unit test");
static ConfigVar<unsigned long long>::ptr g_timeLimit = Config::lookup(
    "ratelimiter.time", 0ull, "Config var used by unit test");

MORDOR_UNITTEST(RateLimiter, countLimits)
{
    IOManager ioManager;
    RateLimiter<int> limiter(ioManager, g_countLimit, g_timeLimit);
    // max of 3, tenth of a second
    g_countLimit->val(3);
    g_timeLimit->val(100000ull);
    MORDOR_TEST_ASSERT(limiter.allowed(1));
    MORDOR_TEST_ASSERT(limiter.allowed(1));
    MORDOR_TEST_ASSERT(limiter.allowed(1));
    MORDOR_TEST_ASSERT(!limiter.allowed(1));
    MORDOR_TEST_ASSERT(!limiter.allowed(1));
    // sleep .2s; should allow three again
    Mordor::sleep(ioManager, 200000ull);
    MORDOR_TEST_ASSERT(limiter.allowed(1));
    MORDOR_TEST_ASSERT(limiter.allowed(1));
    MORDOR_TEST_ASSERT(limiter.allowed(1));
    MORDOR_TEST_ASSERT(!limiter.allowed(1));
}

MORDOR_UNITTEST(RateLimiter, countLimitsSlidingTime)
{
    IOManager ioManager;
    RateLimiter<int> limiter(ioManager, g_countLimit, g_timeLimit);
    // max of 3, half a second
    g_countLimit->val(3);
    g_timeLimit->val(500000ull);
    MORDOR_TEST_ASSERT(limiter.allowed(1));
    Mordor::sleep(ioManager, 250000ull);
    MORDOR_TEST_ASSERT(limiter.allowed(1));
    MORDOR_TEST_ASSERT(limiter.allowed(1));
    MORDOR_TEST_ASSERT(!limiter.allowed(1));
    // sleep .3s; should only allow 1 more as the first slid off
    Mordor::sleep(ioManager, 350000ull);
    MORDOR_TEST_ASSERT(limiter.allowed(1));
    MORDOR_TEST_ASSERT(!limiter.allowed(1));
}

MORDOR_UNITTEST(RateLimiter, reset)
{
    IOManager ioManager;
    RateLimiter<int> limiter(ioManager, g_countLimit, g_timeLimit);
    // max of 3, 5 seconds
    g_countLimit->val(3);
    g_timeLimit->val(5000000ull);
    MORDOR_TEST_ASSERT(limiter.allowed(1));
    MORDOR_TEST_ASSERT(limiter.allowed(1));
    MORDOR_TEST_ASSERT(limiter.allowed(1));
    MORDOR_TEST_ASSERT(!limiter.allowed(1));
    // Resetting (i.e. successful login) should allow a full new batch
    limiter.reset(1);
    MORDOR_TEST_ASSERT(limiter.allowed(1));
    MORDOR_TEST_ASSERT(limiter.allowed(1));
    MORDOR_TEST_ASSERT(limiter.allowed(1));
    MORDOR_TEST_ASSERT(!limiter.allowed(1));
}

MORDOR_UNITTEST(RateLimiter, uniqueKeys)
{
    IOManager ioManager;
    RateLimiter<int> limiter(ioManager, g_countLimit, g_timeLimit);
    // max of 1, 5 seconds
    g_countLimit->val(1);
    g_timeLimit->val(5000000ull);
    MORDOR_TEST_ASSERT(limiter.allowed(1));
    MORDOR_TEST_ASSERT(!limiter.allowed(1));
    MORDOR_TEST_ASSERT(limiter.allowed(2));
    MORDOR_TEST_ASSERT(!limiter.allowed(2));
    limiter.reset(1);
    MORDOR_TEST_ASSERT(!limiter.allowed(2));
    MORDOR_TEST_ASSERT(limiter.allowed(1));
    MORDOR_TEST_ASSERT(!limiter.allowed(1));
}
