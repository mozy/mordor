// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/coroutine.h"
#include "mordor/test/test.h"

using namespace Mordor;
using namespace Mordor::Test;

static void countTo5(Coroutine<int> &self)
{
    self.yield(1);
    self.yield(2);
    self.yield(3);
    self.yield(4);
    self.yield(5);
}

MORDOR_UNITTEST(Coroutine, basic)
{
    Coroutine<int> coro(&countTo5);
    int sequence = 0;
    MORDOR_TEST_ASSERT_EQUAL(coro.state(), Fiber::INIT);
    int value;
    while (true) {
        value = coro.call();
        if (coro.state() == Fiber::TERM)
            break;
        MORDOR_TEST_ASSERT_EQUAL(value, ++sequence);
    }
    MORDOR_TEST_ASSERT_EQUAL(value, 0);
    MORDOR_TEST_ASSERT_EQUAL(sequence, 5);
}

static void echo(Coroutine<int, int> &self, int arg)
{
    while (arg <= 5) {
        arg = self.yield(arg);
    }
}

MORDOR_UNITTEST(Coroutine, basicWithArg)
{
    Coroutine<int, int> coro(&echo);
    MORDOR_TEST_ASSERT_EQUAL(coro.state(), Fiber::INIT);
    for (int i = 0; i <= 5; ++i) {
        MORDOR_TEST_ASSERT(coro.state() == Fiber::INIT || coro.state() == Fiber::HOLD);
        MORDOR_TEST_ASSERT_EQUAL(coro.call(i), i);
    }
}

static void countTo5Arg(Coroutine<void, int> &self, int arg)
{
    int sequence = 0;
    for (int i = 0; i < 5; ++i) {
        MORDOR_TEST_ASSERT_EQUAL(arg, sequence++);
        arg = self.yield();
    }
}

MORDOR_UNITTEST(Coroutine, voidWithArg)
{
    Coroutine<void, int> coro(&countTo5Arg);
    for (int i = 0; i < 5; ++i)
        coro.call(i);
}
