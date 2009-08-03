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
    TEST_ASSERT(a->state() == Fiber::INIT);
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
    TEST_ASSERT(other->state() == Fiber::INIT);
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
    TEST_ASSERT(a->state() == Fiber::INIT);
    TEST_ASSERT(b->state() == Fiber::INIT);
    a->call();
    ++sequence;
    TEST_ASSERT_EQUAL(sequence, 4);
    TEST_ASSERT(Fiber::getThis() == mainFiber);
    TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    TEST_ASSERT(a->state() == Fiber::TERM);
    TEST_ASSERT(b->state() == Fiber::TERM);
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
    TEST_ASSERT_EQUAL(sequence, 1);
    TEST_ASSERT(Fiber::getThis() == a);
    TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    TEST_ASSERT(a->state() == Fiber::EXEC);
    TEST_ASSERT(b->state() == Fiber::INIT);
    TEST_ASSERT(c->state() == Fiber::INIT);
    TEST_ASSERT(d->state() == Fiber::INIT);
    b->call();
    ++sequence;
    TEST_ASSERT_EQUAL(sequence, 6);
    TEST_ASSERT(Fiber::getThis() == a);
    TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    TEST_ASSERT(a->state() == Fiber::EXEC);
    TEST_ASSERT(b->state() == Fiber::TERM);
    TEST_ASSERT(c->state() == Fiber::EXEC);
    TEST_ASSERT(d->state() == Fiber::HOLD);
}

static void
fiberProc3b(Fiber::ptr mainFiber, Fiber::weak_ptr weaka, Fiber::weak_ptr weakb,
            Fiber::weak_ptr weakc, Fiber::weak_ptr weakd, int &sequence)
{
    Fiber::ptr a(weaka), b(weakb), c(weakc), d(weakd);
    ++sequence;
    TEST_ASSERT_EQUAL(sequence, 2);
    TEST_ASSERT(Fiber::getThis() == b);
    TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    TEST_ASSERT(a->state() == Fiber::EXEC);
    TEST_ASSERT(b->state() == Fiber::EXEC);
    TEST_ASSERT(c->state() == Fiber::INIT);
    TEST_ASSERT(d->state() == Fiber::INIT);
    c->yieldTo();
    ++sequence;
    TEST_ASSERT_EQUAL(sequence, 5);
    TEST_ASSERT(Fiber::getThis() == b);
    TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    TEST_ASSERT(a->state() == Fiber::EXEC);
    TEST_ASSERT(b->state() == Fiber::EXEC);
    TEST_ASSERT(c->state() == Fiber::EXEC);
    TEST_ASSERT(d->state() == Fiber::HOLD);
}

static void
fiberProc3c(Fiber::ptr mainFiber, Fiber::weak_ptr weaka, Fiber::weak_ptr weakb,
            Fiber::weak_ptr weakc, Fiber::weak_ptr weakd, int &sequence)
{
    Fiber::ptr a(weaka), b(weakb), c(weakc), d(weakd);
    ++sequence;
    TEST_ASSERT_EQUAL(sequence, 3);
    TEST_ASSERT(Fiber::getThis() == c);
    TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    TEST_ASSERT(a->state() == Fiber::EXEC);
    TEST_ASSERT(b->state() == Fiber::HOLD);
    TEST_ASSERT(c->state() == Fiber::EXEC);
    TEST_ASSERT(d->state() == Fiber::INIT);
    d->call();
    ++sequence;
    TEST_ASSERT_EQUAL(sequence, 9);
    TEST_ASSERT(Fiber::getThis() == c);
    TEST_ASSERT(mainFiber->state() == Fiber::HOLD);
    TEST_ASSERT(a->state() == Fiber::TERM);
    TEST_ASSERT(b->state() == Fiber::TERM);
    TEST_ASSERT(c->state() == Fiber::EXEC);
    TEST_ASSERT(d->state() == Fiber::TERM);
    // Implicit yieldTo "caller"
}

static void
fiberProc3d(Fiber::ptr mainFiber, Fiber::weak_ptr weaka, Fiber::weak_ptr weakb,
            Fiber::weak_ptr weakc, Fiber::weak_ptr weakd, int &sequence)
{
    Fiber::ptr a(weaka), b(weakb), c(weakc), d(weakd);
    ++sequence;
    TEST_ASSERT_EQUAL(sequence, 4);
    TEST_ASSERT(Fiber::getThis() == d);
    TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    TEST_ASSERT(a->state() == Fiber::EXEC);
    TEST_ASSERT(b->state() == Fiber::HOLD);
    TEST_ASSERT(c->state() == Fiber::EXEC);
    TEST_ASSERT(d->state() == Fiber::EXEC);
    b->yieldTo();
    ++sequence;
    TEST_ASSERT_EQUAL(sequence, 8);
    TEST_ASSERT(Fiber::getThis() == d);
    TEST_ASSERT(mainFiber->state() == Fiber::HOLD);
    TEST_ASSERT(a->state() == Fiber::TERM);
    TEST_ASSERT(b->state() == Fiber::TERM);
    TEST_ASSERT(c->state() == Fiber::EXEC);
    TEST_ASSERT(d->state() == Fiber::EXEC);
}

TEST_WITH_SUITE(Fibers, yieldTo)
{
    int sequence = 0;
    Fiber::ptr mainFiber(new Fiber());
    Fiber::ptr a(new Fiber(NULL, 65536 * 6));
    Fiber::ptr b(new Fiber(NULL, 65536 * 6));
    Fiber::ptr c(new Fiber(NULL, 65536 * 6));
    Fiber::ptr d(new Fiber(NULL, 65536 * 6));
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
    TEST_ASSERT(Fiber::getThis() == mainFiber);
    TEST_ASSERT(a != mainFiber);
    TEST_ASSERT(b != mainFiber);
    TEST_ASSERT(c != mainFiber);
    TEST_ASSERT(d != mainFiber);
    TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    TEST_ASSERT(a->state() == Fiber::INIT);
    TEST_ASSERT(b->state() == Fiber::INIT);
    TEST_ASSERT(c->state() == Fiber::INIT);
    TEST_ASSERT(d->state() == Fiber::INIT);
    a->call();
    ++sequence;
    TEST_ASSERT_EQUAL(sequence, 7);
    TEST_ASSERT(Fiber::getThis() == mainFiber);
    TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    TEST_ASSERT(a->state() == Fiber::TERM);
    TEST_ASSERT(b->state() == Fiber::TERM);
    TEST_ASSERT(c->state() == Fiber::EXEC);
    TEST_ASSERT(d->state() == Fiber::HOLD);
    d->yieldTo();
    ++sequence;
    TEST_ASSERT_EQUAL(sequence, 10);
    TEST_ASSERT(Fiber::getThis() == mainFiber);
    TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    TEST_ASSERT(a->state() == Fiber::TERM);
    TEST_ASSERT(b->state() == Fiber::TERM);
    TEST_ASSERT(c->state() == Fiber::TERM);
    TEST_ASSERT(d->state() == Fiber::TERM);
}

static void
fiberProc4(Fiber::ptr mainFiber, Fiber::weak_ptr weakself, int &sequence)
{
    Fiber::ptr self(weakself);
    TEST_ASSERT(Fiber::getThis() == self);
    TEST_ASSERT(mainFiber != self);
    TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    TEST_ASSERT(self->state() == Fiber::EXEC);
    ++sequence;
}

TEST_WITH_SUITE(Fibers, reset)
{
    int sequence = 0;
    Fiber::ptr mainFiber(new Fiber());
    Fiber::ptr a(new Fiber(NULL, 65536 * 6));
    a->reset(boost::bind(&fiberProc4, mainFiber, Fiber::weak_ptr(a),
        boost::ref(sequence)));
    TEST_ASSERT(Fiber::getThis() == mainFiber);
    TEST_ASSERT(a != mainFiber);
    TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    TEST_ASSERT(a->state() == Fiber::INIT);
    a->call();
    ++sequence;
    TEST_ASSERT_EQUAL(sequence, 2);
    TEST_ASSERT(Fiber::getThis() == mainFiber);
    TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    TEST_ASSERT(a->state() == Fiber::TERM);
    a->reset();
    TEST_ASSERT(a->state() == Fiber::INIT);
    a->call();
    ++sequence;
    TEST_ASSERT_EQUAL(sequence, 4);
    TEST_ASSERT(Fiber::getThis() == mainFiber);
    TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    TEST_ASSERT(a->state() == Fiber::TERM);
    a->reset();
    TEST_ASSERT(a->state() == Fiber::INIT);
    a->call();
    ++sequence;
    TEST_ASSERT_EQUAL(sequence, 6);
    TEST_ASSERT(Fiber::getThis() == mainFiber);
    TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    TEST_ASSERT(a->state() == Fiber::TERM);
}

static void throwBadAlloc()
{
    throw std::bad_alloc();
}

TEST_WITH_SUITE(Fibers, badAlloc)
{
    Fiber::ptr mainFiber(new Fiber());
    Fiber::ptr fiber(new Fiber(&throwBadAlloc));
    TEST_ASSERT_EXCEPTION(fiber->call(), std::bad_alloc);
}

static void throwFileNotFound()
{
    throw FileNotFoundException();
}

TEST_WITH_SUITE(Fibers, nativeException)
{
    Fiber::ptr mainFiber(new Fiber());
    Fiber::ptr fiber(new Fiber(&throwFileNotFound));
    TEST_ASSERT_EXCEPTION(fiber->call(), FileNotFoundException);
}

static void throwRuntimeError()
{
    throw std::runtime_error("message");
}

TEST_WITH_SUITE(Fibers, runtimeError)
{
    Fiber::ptr mainFiber(new Fiber());
    Fiber::ptr fiber(new Fiber(&throwRuntimeError));
    try {
        fiber->call();
    } catch (std::runtime_error &ex) {
        TEST_ASSERT_EQUAL(std::string(ex.what()), "message");
    }
}

TEST_WITH_SUITE(Fibers, badAllocYieldTo)
{
    Fiber::ptr mainFiber(new Fiber());
    Fiber::ptr fiber(new Fiber(&throwBadAlloc));
    TEST_ASSERT_EXCEPTION(fiber->yieldTo(), std::bad_alloc);
}

class GenericException : public std::runtime_error
{
public:
    GenericException() : std::runtime_error("message")
    {}
};

static void throwGenericException()
{
    throw GenericException();
}

TEST_WITH_SUITE(Fibers, genericException)
{
    Fiber::ptr mainFiber(new Fiber());
    Fiber::ptr fiber(new Fiber(&throwGenericException));
    try {
        fiber->call();
    } catch (FiberException &ex) {
        TEST_ASSERT(typeid(ex.inner()) == typeid(GenericException));
        TEST_ASSERT_EQUAL(std::string(ex.inner().what()), "message");
    }
}

TEST_WITH_SUITE(Fibers, fiberThrowingExceptionOutOfScope)
{
    Fiber::ptr mainFiber(new Fiber());
    try {
        Fiber::ptr fiber(new Fiber(&throwGenericException));
        fiber->call();
    } catch (FiberException &ex) {
        TEST_ASSERT(typeid(ex.inner()) == typeid(GenericException));
        TEST_ASSERT_EQUAL(std::string(ex.inner().what()), "message");
    }
}

#ifdef DEBUG
TEST_WITH_SUITE(Fibers, assertNeedCallingFiber)
{
    Fiber::ptr fiber(new Fiber(&throwGenericException));
    TEST_ASSERT_ASSERTED(fiber->call());
}
#endif
