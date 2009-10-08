// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include "mordor/common/coroutine.h"
#include "mordor/test/test.h"

using namespace Mordor;
using namespace Mordor::Test;

static int countTo5(Coroutine<int>::ptr self)
{
    self->yield(1);
    self->yield(2);
    self->yield(3);
    self->yield(4);
    return 5;
}

MORDOR_UNITTEST(Coroutine, basic)
{
    Fiber::ptr mainFiber(new Fiber());
    Coroutine<int>::ptr coro(new Coroutine<int>(&countTo5));
    int sequence = 0;
    MORDOR_TEST_ASSERT_EQUAL(coro->state(), Fiber::INIT);
    while (coro->state() != Fiber::TERM) {
        MORDOR_TEST_ASSERT_EQUAL(coro->call(), ++sequence);
    }
    MORDOR_TEST_ASSERT_EQUAL(sequence, 5);
}

static int echo(Coroutine<int, int>::ptr self, int arg)
{
    while (arg < 5) {
        arg = self->yield(arg);
    }
    return arg;
}

MORDOR_UNITTEST(Coroutine, basicWithArg)
{
    Fiber::ptr mainFiber(new Fiber());
    Coroutine<int, int>::ptr coro(new Coroutine<int, int>(boost::function<int (Coroutine<int, int>::ptr, int)>(&echo)));
    MORDOR_TEST_ASSERT_EQUAL(coro->state(), Fiber::INIT);
    for (int i = 0; i <= 5; ++i) {
        MORDOR_TEST_ASSERT(coro->state() == Fiber::INIT || coro->state() == Fiber::HOLD);
        MORDOR_TEST_ASSERT_EQUAL(coro->call(i), i);
    }
    MORDOR_TEST_ASSERT_EQUAL(coro->state(), Fiber::TERM);
}
/*
static int producer(Coroutine<int, int>::ptr self, int arg,
                    Coroutine<int, int>::weak_ptr weak_consumer)
{
    Coroutine<int, int>::ptr consumer(weak_consumer);
    for (int i = 0; i < 5; ++i)
        self->yieldTo(consumer, i);
    return 0;
}

static int consumer(Coroutine<int, int>::ptr self, int arg,
                    Coroutine<int, int>::weak_ptr weak_producer)
{
    Coroutine<int, int>::ptr producer(weak_producer);
    int sequence = 0;
    while (arg > 0) {
        MORDOR_TEST_ASSERT_EQUAL(sequence++, arg);
        arg = self->yieldTo(producer, 0);
    }
    return 0;
}

MORDOR_UNITTEST(Coroutine, producerConsumer)
{
    Fiber::ptr mainFiber(new Fiber());
    Coroutine<int, int>::ptr producerCoro(new Coroutine<int, int>());
    Coroutine<int, int>::ptr consumerCoro(new Coroutine<int, int>());

    producerCoro->reset(boost::bind(&producer, _1, _2, Coroutine<int, int>::weak_ptr(consumerCoro)));
    consumerCoro->reset(boost::bind(&consumer, _1, _2, Coroutine<int, int>::weak_ptr(producerCoro)));

    producerCoro->call(0);
    consumerCoro->call(-1);
}
*/

