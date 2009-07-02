// Copyright (c) 2009 - Decho Corp.

#include <boost/bind.hpp>

#include "mordor/common/fiber.h"
#include "mordor/test/test.h"

SUITE_INVARIANT(Fibers)
{
    TEST_ASSERT(!Fiber::getThis());
}

static void
fiberProc1(Fiber::ptr mainFiber, Fiber::weak_ptr weakself, int &sequence)
{
    Fiber::ptr self(weakself);
    ++sequence;
    TEST_ASSERT_EQUAL(sequence, 1);
    TEST_ASSERT(Fiber::getThis() == self);
    TEST_ASSERT(mainFiber != self);
    TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    TEST_ASSERT(self->state() == Fiber::EXEC);
    Fiber::yield();
    ++sequence;
    TEST_ASSERT_EQUAL(sequence, 3);
    TEST_ASSERT(Fiber::getThis() == self);
    TEST_ASSERT(mainFiber != self);
    TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    TEST_ASSERT(self->state() == Fiber::EXEC);
}

TEST_WITH_SUITE(Fibers, call)
{
    int sequence = 0;
    Fiber::ptr mainFiber(new Fiber());
    Fiber::ptr a(new Fiber(NULL, 65536 * 6));
    a->reset(boost::bind(&fiberProc1, mainFiber, Fiber::weak_ptr(a), boost::ref(sequence)));
    TEST_ASSERT(Fiber::getThis() == mainFiber);
    TEST_ASSERT(a != mainFiber);
    TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    TEST_ASSERT(a->state() == Fiber::HOLD);
    a->call();
    ++sequence;
    TEST_ASSERT_EQUAL(sequence, 2);
    TEST_ASSERT(Fiber::getThis() == mainFiber);
    TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    TEST_ASSERT(a->state() == Fiber::HOLD);
    a->call();
    ++sequence;
    TEST_ASSERT_EQUAL(sequence, 4);
    TEST_ASSERT(Fiber::getThis() == mainFiber);
    TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    TEST_ASSERT(a->state() == Fiber::TERM);
}

static void
fiberProc2a(Fiber::ptr mainFiber, Fiber::weak_ptr weakself,
            Fiber::weak_ptr weakother, int &sequence)
{
    Fiber::ptr self(weakself), other(weakother);
    ++sequence;
    TEST_ASSERT_EQUAL(sequence, 1);
    TEST_ASSERT(Fiber::getThis() == self);
    TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    TEST_ASSERT(self->state() == Fiber::EXEC);
    TEST_ASSERT(other->state() == Fiber::HOLD);
    other->call();
    ++sequence;
    TEST_ASSERT_EQUAL(sequence, 3);
    TEST_ASSERT(Fiber::getThis() == self);
    TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    TEST_ASSERT(self->state() == Fiber::EXEC);
    TEST_ASSERT(other->state() == Fiber::TERM);
}

static void
fiberProc2b(Fiber::ptr mainFiber, Fiber::weak_ptr weakself,
            Fiber::weak_ptr weakother, int &sequence)
{
    Fiber::ptr self(weakself), other(weakother);
    ++sequence;
    TEST_ASSERT_EQUAL(sequence, 2);
    TEST_ASSERT(Fiber::getThis() == self);
    TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    TEST_ASSERT(other->state() == Fiber::EXEC);
    TEST_ASSERT(self->state() == Fiber::EXEC);
}

TEST_WITH_SUITE(Fibers, nestedCall)
{
    int sequence = 0;
    Fiber::ptr mainFiber(new Fiber());
    Fiber::ptr a(new Fiber(NULL, 65536 * 6));
    Fiber::ptr b(new Fiber(NULL, 65536 * 6));
    a->reset(boost::bind(&fiberProc2a, mainFiber, Fiber::weak_ptr(a),
        Fiber::weak_ptr(b), boost::ref(sequence)));
    b->reset(boost::bind(&fiberProc2b, mainFiber, Fiber::weak_ptr(b),
        Fiber::weak_ptr(a), boost::ref(sequence)));
    TEST_ASSERT(Fiber::getThis() == mainFiber);
    TEST_ASSERT(a != mainFiber);
    TEST_ASSERT(b != mainFiber);
    TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    TEST_ASSERT(a->state() == Fiber::HOLD);
    TEST_ASSERT(b->state() == Fiber::HOLD);
    a->call();
    ++sequence;
    TEST_ASSERT_EQUAL(sequence, 4);
    TEST_ASSERT(Fiber::getThis() == mainFiber);
    TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    TEST_ASSERT(a->state() == Fiber::TERM);
    TEST_ASSERT(b->state() == Fiber::TERM);
}
