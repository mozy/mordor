// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include <boost/bind.hpp>

#include "mordor/fiber.h"
#include "mordor/test/test.h"

using namespace Mordor;
using namespace Mordor::Test;

struct DummyException : public boost::exception, public std::exception {};

MORDOR_SUITE_INVARIANT(Fibers)
{
    MORDOR_TEST_ASSERT(!Fiber::getThis());
}

static void
fiberProc1(Fiber::ptr mainFiber, Fiber::weak_ptr weakself, int &sequence)
{
    Fiber::ptr self(weakself);
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);
    MORDOR_TEST_ASSERT(Fiber::getThis() == self);
    MORDOR_TEST_ASSERT(mainFiber != self);
    MORDOR_TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(self->state() == Fiber::EXEC);
    Fiber::yield();
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, 3);
    MORDOR_TEST_ASSERT(Fiber::getThis() == self);
    MORDOR_TEST_ASSERT(mainFiber != self);
    MORDOR_TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(self->state() == Fiber::EXEC);
}

MORDOR_UNITTEST(Fibers, call)
{
    int sequence = 0;
    Fiber::ptr mainFiber(new Fiber());
    Fiber::ptr a(new Fiber(NULL));
    a->reset(boost::bind(&fiberProc1, mainFiber, Fiber::weak_ptr(a), boost::ref(sequence)));
    MORDOR_TEST_ASSERT(Fiber::getThis() == mainFiber);
    MORDOR_TEST_ASSERT(a != mainFiber);
    MORDOR_TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(a->state() == Fiber::INIT);
    a->call();
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, 2);
    MORDOR_TEST_ASSERT(Fiber::getThis() == mainFiber);
    MORDOR_TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(a->state() == Fiber::HOLD);
    a->call();
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, 4);
    MORDOR_TEST_ASSERT(Fiber::getThis() == mainFiber);
    MORDOR_TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(a->state() == Fiber::TERM);
}

static void
fiberProc2a(Fiber::ptr mainFiber, Fiber::weak_ptr weakself,
            Fiber::weak_ptr weakother, int &sequence)
{
    Fiber::ptr self(weakself), other(weakother);
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);
    MORDOR_TEST_ASSERT(Fiber::getThis() == self);
    MORDOR_TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(self->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(other->state() == Fiber::INIT);
    other->call();
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, 3);
    MORDOR_TEST_ASSERT(Fiber::getThis() == self);
    MORDOR_TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(self->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(other->state() == Fiber::TERM);
}

static void
fiberProc2b(Fiber::ptr mainFiber, Fiber::weak_ptr weakself,
            Fiber::weak_ptr weakother, int &sequence)
{
    Fiber::ptr self(weakself), other(weakother);
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, 2);
    MORDOR_TEST_ASSERT(Fiber::getThis() == self);
    MORDOR_TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(other->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(self->state() == Fiber::EXEC);
}

MORDOR_UNITTEST(Fibers, nestedCall)
{
    int sequence = 0;
    Fiber::ptr mainFiber(new Fiber());
    Fiber::ptr a(new Fiber(NULL));
    Fiber::ptr b(new Fiber(NULL));
    a->reset(boost::bind(&fiberProc2a, mainFiber, Fiber::weak_ptr(a),
        Fiber::weak_ptr(b), boost::ref(sequence)));
    b->reset(boost::bind(&fiberProc2b, mainFiber, Fiber::weak_ptr(b),
        Fiber::weak_ptr(a), boost::ref(sequence)));
    MORDOR_TEST_ASSERT(Fiber::getThis() == mainFiber);
    MORDOR_TEST_ASSERT(a != mainFiber);
    MORDOR_TEST_ASSERT(b != mainFiber);
    MORDOR_TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(a->state() == Fiber::INIT);
    MORDOR_TEST_ASSERT(b->state() == Fiber::INIT);
    a->call();
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, 4);
    MORDOR_TEST_ASSERT(Fiber::getThis() == mainFiber);
    MORDOR_TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(a->state() == Fiber::TERM);
    MORDOR_TEST_ASSERT(b->state() == Fiber::TERM);
}

// Call graphs for this test:
// main -> A -> B
// (yieldTo C) C -> D
// (yieldTo B), unwind to main
// (yieldTo D), unwind to C
// (implicit yieldTo main)

static void
fiberProc3a(Fiber::ptr mainFiber, Fiber::weak_ptr weaka, Fiber::weak_ptr weakb,
            Fiber::weak_ptr weakc, Fiber::weak_ptr weakd, int &sequence)
{
    Fiber::ptr a(weaka), b(weakb), c(weakc), d(weakd);
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);
    MORDOR_TEST_ASSERT(Fiber::getThis() == a);
    MORDOR_TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(a->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(b->state() == Fiber::INIT);
    MORDOR_TEST_ASSERT(c->state() == Fiber::INIT);
    MORDOR_TEST_ASSERT(d->state() == Fiber::INIT);
    b->call();
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, 6);
    MORDOR_TEST_ASSERT(Fiber::getThis() == a);
    MORDOR_TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(a->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(b->state() == Fiber::TERM);
    MORDOR_TEST_ASSERT(c->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(d->state() == Fiber::HOLD);
}

static void
fiberProc3b(Fiber::ptr mainFiber, Fiber::weak_ptr weaka, Fiber::weak_ptr weakb,
            Fiber::weak_ptr weakc, Fiber::weak_ptr weakd, int &sequence)
{
    Fiber::ptr a(weaka), b(weakb), c(weakc), d(weakd);
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, 2);
    MORDOR_TEST_ASSERT(Fiber::getThis() == b);
    MORDOR_TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(a->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(b->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(c->state() == Fiber::INIT);
    MORDOR_TEST_ASSERT(d->state() == Fiber::INIT);
    c->yieldTo();
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, 5);
    MORDOR_TEST_ASSERT(Fiber::getThis() == b);
    MORDOR_TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(a->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(b->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(c->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(d->state() == Fiber::HOLD);
}

static void
fiberProc3c(Fiber::ptr mainFiber, Fiber::weak_ptr weaka, Fiber::weak_ptr weakb,
            Fiber::weak_ptr weakc, Fiber::weak_ptr weakd, int &sequence)
{
    Fiber::ptr a(weaka), b(weakb), c(weakc), d(weakd);
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, 3);
    MORDOR_TEST_ASSERT(Fiber::getThis() == c);
    MORDOR_TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(a->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(b->state() == Fiber::HOLD);
    MORDOR_TEST_ASSERT(c->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(d->state() == Fiber::INIT);
    d->call();
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, 9);
    MORDOR_TEST_ASSERT(Fiber::getThis() == c);
    MORDOR_TEST_ASSERT(mainFiber->state() == Fiber::HOLD);
    MORDOR_TEST_ASSERT(a->state() == Fiber::TERM);
    MORDOR_TEST_ASSERT(b->state() == Fiber::TERM);
    MORDOR_TEST_ASSERT(c->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(d->state() == Fiber::TERM);
    // Implicit yieldTo "caller"
}

static void
fiberProc3d(Fiber::ptr mainFiber, Fiber::weak_ptr weaka, Fiber::weak_ptr weakb,
            Fiber::weak_ptr weakc, Fiber::weak_ptr weakd, int &sequence)
{
    Fiber::ptr a(weaka), b(weakb), c(weakc), d(weakd);
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, 4);
    MORDOR_TEST_ASSERT(Fiber::getThis() == d);
    MORDOR_TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(a->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(b->state() == Fiber::HOLD);
    MORDOR_TEST_ASSERT(c->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(d->state() == Fiber::EXEC);
    b->yieldTo();
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, 8);
    MORDOR_TEST_ASSERT(Fiber::getThis() == d);
    MORDOR_TEST_ASSERT(mainFiber->state() == Fiber::HOLD);
    MORDOR_TEST_ASSERT(a->state() == Fiber::TERM);
    MORDOR_TEST_ASSERT(b->state() == Fiber::TERM);
    MORDOR_TEST_ASSERT(c->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(d->state() == Fiber::EXEC);
}

MORDOR_UNITTEST(Fibers, yieldTo)
{
    int sequence = 0;
    Fiber::ptr mainFiber(new Fiber());
    Fiber::ptr a(new Fiber(NULL));
    Fiber::ptr b(new Fiber(NULL));
    Fiber::ptr c(new Fiber(NULL));
    Fiber::ptr d(new Fiber(NULL));
    a->reset(boost::bind(&fiberProc3a, mainFiber, Fiber::weak_ptr(a),
        Fiber::weak_ptr(b), Fiber::weak_ptr(c),
        Fiber::weak_ptr(d), boost::ref(sequence)));
    b->reset(boost::bind(&fiberProc3b, mainFiber, Fiber::weak_ptr(a),
        Fiber::weak_ptr(b), Fiber::weak_ptr(c),
        Fiber::weak_ptr(d), boost::ref(sequence)));
    c->reset(boost::bind(&fiberProc3c, mainFiber, Fiber::weak_ptr(a),
        Fiber::weak_ptr(b), Fiber::weak_ptr(c),
        Fiber::weak_ptr(d), boost::ref(sequence)));
    d->reset(boost::bind(&fiberProc3d, mainFiber, Fiber::weak_ptr(a),
        Fiber::weak_ptr(b), Fiber::weak_ptr(c),
        Fiber::weak_ptr(d), boost::ref(sequence)));
    MORDOR_TEST_ASSERT(Fiber::getThis() == mainFiber);
    MORDOR_TEST_ASSERT(a != mainFiber);
    MORDOR_TEST_ASSERT(b != mainFiber);
    MORDOR_TEST_ASSERT(c != mainFiber);
    MORDOR_TEST_ASSERT(d != mainFiber);
    MORDOR_TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(a->state() == Fiber::INIT);
    MORDOR_TEST_ASSERT(b->state() == Fiber::INIT);
    MORDOR_TEST_ASSERT(c->state() == Fiber::INIT);
    MORDOR_TEST_ASSERT(d->state() == Fiber::INIT);
    a->call();
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, 7);
    MORDOR_TEST_ASSERT(Fiber::getThis() == mainFiber);
    MORDOR_TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(a->state() == Fiber::TERM);
    MORDOR_TEST_ASSERT(b->state() == Fiber::TERM);
    MORDOR_TEST_ASSERT(c->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(d->state() == Fiber::HOLD);
    d->yieldTo();
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, 10);
    MORDOR_TEST_ASSERT(Fiber::getThis() == mainFiber);
    MORDOR_TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(a->state() == Fiber::TERM);
    MORDOR_TEST_ASSERT(b->state() == Fiber::TERM);
    MORDOR_TEST_ASSERT(c->state() == Fiber::TERM);
    MORDOR_TEST_ASSERT(d->state() == Fiber::TERM);
}

static void
fiberProc4(Fiber::ptr mainFiber, Fiber::weak_ptr weakself, int &sequence, bool exception)
{
    Fiber::ptr self(weakself);
    MORDOR_TEST_ASSERT(Fiber::getThis() == self);
    MORDOR_TEST_ASSERT(mainFiber != self);
    MORDOR_TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(self->state() == Fiber::EXEC);
    ++sequence;
    if (exception)
        MORDOR_THROW_EXCEPTION(DummyException());
}

MORDOR_UNITTEST(Fibers, reset)
{
    int sequence = 0;
    Fiber::ptr mainFiber(new Fiber());
    Fiber::ptr a(new Fiber(NULL));
    a->reset(boost::bind(&fiberProc4, mainFiber, Fiber::weak_ptr(a),
        boost::ref(sequence), false));
    MORDOR_TEST_ASSERT(Fiber::getThis() == mainFiber);
    MORDOR_TEST_ASSERT(a != mainFiber);
    MORDOR_TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(a->state() == Fiber::INIT);
    a->call();
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, 2);
    MORDOR_TEST_ASSERT(Fiber::getThis() == mainFiber);
    MORDOR_TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(a->state() == Fiber::TERM);
    a->reset();
    MORDOR_TEST_ASSERT(a->state() == Fiber::INIT);
    a->call();
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, 4);
    MORDOR_TEST_ASSERT(Fiber::getThis() == mainFiber);
    MORDOR_TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(a->state() == Fiber::TERM);
    a->reset();
    MORDOR_TEST_ASSERT(a->state() == Fiber::INIT);
    a->call();
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, 6);
    MORDOR_TEST_ASSERT(Fiber::getThis() == mainFiber);
    MORDOR_TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(a->state() == Fiber::TERM);
    a->reset(boost::bind(&fiberProc4, mainFiber, Fiber::weak_ptr(a),
        boost::ref(sequence), true));
    MORDOR_TEST_ASSERT(a->state() == Fiber::INIT);
    MORDOR_TEST_ASSERT_EXCEPTION(a->call(), DummyException);
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, 8);
    MORDOR_TEST_ASSERT(Fiber::getThis() == mainFiber);
    MORDOR_TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(a->state() == Fiber::EXCEPT);
    a->reset();
    MORDOR_TEST_ASSERT(a->state() == Fiber::INIT);
    MORDOR_TEST_ASSERT_EXCEPTION(a->call(), DummyException);
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, 10);
    MORDOR_TEST_ASSERT(Fiber::getThis() == mainFiber);
    MORDOR_TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(a->state() == Fiber::EXCEPT);
}

static void throwBadAlloc()
{
    throw std::bad_alloc();
}

MORDOR_UNITTEST(Fibers, badAlloc)
{
    Fiber::ptr mainFiber(new Fiber());
    Fiber::ptr fiber(new Fiber(&throwBadAlloc));
    MORDOR_TEST_ASSERT_EXCEPTION(fiber->call(), std::bad_alloc);
}

static void throwFileNotFound()
{
    MORDOR_THROW_EXCEPTION(FileNotFoundException());
}

MORDOR_UNITTEST(Fibers, nativeException)
{
    Fiber::ptr mainFiber(new Fiber());
    Fiber::ptr fiber(new Fiber(&throwFileNotFound));
    MORDOR_TEST_ASSERT_EXCEPTION(fiber->call(), FileNotFoundException);
}

static void throwRuntimeError()
{
    throw std::runtime_error("message");
}

MORDOR_UNITTEST(Fibers, runtimeError)
{
    Fiber::ptr mainFiber(new Fiber());
    Fiber::ptr fiber(new Fiber(&throwRuntimeError));
    try {
        fiber->call();
    } catch (std::runtime_error &ex) {
        MORDOR_TEST_ASSERT_EQUAL(std::string(ex.what()), "message");
    }
}

MORDOR_UNITTEST(Fibers, badAllocYieldTo)
{
    Fiber::ptr mainFiber(new Fiber());
    Fiber::ptr fiber(new Fiber(&throwBadAlloc));
    MORDOR_TEST_ASSERT_EXCEPTION(fiber->yieldTo(), std::bad_alloc);
}

static void throwGenericException()
{
    MORDOR_THROW_EXCEPTION(DummyException());
}

MORDOR_UNITTEST(Fibers, genericException)
{
    Fiber::ptr mainFiber(new Fiber());
    Fiber::ptr fiber(new Fiber(&throwGenericException));
    MORDOR_TEST_ASSERT_EXCEPTION(fiber->call(), DummyException);
}

MORDOR_UNITTEST(Fibers, fiberThrowingExceptionOutOfScope)
{
    Fiber::ptr mainFiber(new Fiber());
    try {
        Fiber::ptr fiber(new Fiber(&throwGenericException));
        fiber->call();
        MORDOR_NOTREACHED();
    } catch (DummyException &) {
    }
}

#ifdef DEBUG
MORDOR_UNITTEST(Fibers, assertNeedCallingFiber)
{
    Fiber::ptr fiber(new Fiber(&throwGenericException));
    MORDOR_TEST_ASSERT_ASSERTED(fiber->call());
}
#endif
