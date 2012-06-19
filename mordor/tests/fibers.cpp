// Copyright (c) 2009 - Mozy, Inc.

#include <boost/bind.hpp>

#include "mordor/fiber.h"
#include "mordor/test/test.h"

using namespace Mordor;
using namespace Mordor::Test;

struct DummyException : public boost::exception, public std::exception
{
    ~DummyException() throw() {}
};

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
    Fiber::ptr mainFiber = Fiber::getThis();
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
    Fiber::ptr mainFiber = Fiber::getThis();
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
    Fiber::ptr mainFiber = Fiber::getThis();
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

static void fiberProcYieldBack(int &sequence, Fiber::ptr caller,
                               Fiber::weak_ptr weakself)
{
    Fiber::ptr self(weakself);
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 1);
    MORDOR_TEST_ASSERT(Fiber::getThis() == self);
    MORDOR_TEST_ASSERT(caller != self);
    MORDOR_TEST_ASSERT(caller->state() == Fiber::HOLD);
    MORDOR_TEST_ASSERT(self->state() == Fiber::EXEC);
    caller->yieldTo();
    MORDOR_TEST_ASSERT(Fiber::getThis() == self);
    MORDOR_TEST_ASSERT(caller != self);
    MORDOR_TEST_ASSERT(caller->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(self->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 3);
}

MORDOR_UNITTEST(Fibers, yieldBackThenCall)
{
    int sequence = 0;
    Fiber::ptr mainFiber = Fiber::getThis();
    Fiber::ptr a(new Fiber(NULL));
    a->reset(boost::bind(&fiberProcYieldBack, boost::ref(sequence),
        mainFiber, Fiber::weak_ptr(a)));
    MORDOR_TEST_ASSERT(Fiber::getThis() == mainFiber);
    MORDOR_TEST_ASSERT(mainFiber != a);
    MORDOR_TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(a->state() == Fiber::INIT);
    a->yieldTo();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 2);
    MORDOR_TEST_ASSERT(Fiber::getThis() == mainFiber);
    MORDOR_TEST_ASSERT(mainFiber != a);
    MORDOR_TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(a->state() == Fiber::HOLD);
    a->call();
    MORDOR_TEST_ASSERT(Fiber::getThis() == mainFiber);
    MORDOR_TEST_ASSERT(mainFiber != a);
    MORDOR_TEST_ASSERT(mainFiber->state() == Fiber::EXEC);
    MORDOR_TEST_ASSERT(a->state() == Fiber::TERM);
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 4);
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
    Fiber::ptr mainFiber = Fiber::getThis();
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
    MORDOR_THROW_EXCEPTION(std::bad_alloc());
}

MORDOR_UNITTEST(Fibers, badAlloc)
{
    Fiber::ptr fiber(new Fiber(&throwBadAlloc));
    MORDOR_TEST_ASSERT_EXCEPTION(fiber->call(), std::bad_alloc);
}

static void throwFileNotFound()
{
    MORDOR_THROW_EXCEPTION(FileNotFoundException());
}

MORDOR_UNITTEST(Fibers, nativeException)
{
    Fiber::ptr fiber(new Fiber(&throwFileNotFound));
    MORDOR_TEST_ASSERT_EXCEPTION(fiber->call(), FileNotFoundException);
}

static void throwAccessDenied()
{
    MORDOR_THROW_EXCEPTION(AccessDeniedException());
}

MORDOR_UNITTEST(Fibers, nativeException2)
{
    Fiber::ptr fiber(new Fiber(&throwAccessDenied));
    MORDOR_TEST_ASSERT_EXCEPTION(fiber->call(), AccessDeniedException);
}

static void throwRuntimeError()
{
    throw std::runtime_error("message");
}

MORDOR_UNITTEST(Fibers, runtimeError)
{
    Fiber::ptr fiber(new Fiber(&throwRuntimeError));
    try {
        fiber->call();
    } catch (std::runtime_error &ex) {
        MORDOR_TEST_ASSERT_EQUAL(std::string(ex.what()), "message");
    }
}

MORDOR_UNITTEST(Fibers, badAllocYieldTo)
{
    Fiber::ptr fiber(new Fiber(&throwBadAlloc));
    MORDOR_TEST_ASSERT_EXCEPTION(fiber->yieldTo(), std::bad_alloc);
}

static void throwGenericException()
{
    MORDOR_THROW_EXCEPTION(DummyException());
}

MORDOR_UNITTEST(Fibers, genericException)
{
    Fiber::ptr fiber(new Fiber(&throwGenericException));
    MORDOR_TEST_ASSERT_EXCEPTION(fiber->call(), DummyException);
}

MORDOR_UNITTEST(Fibers, fiberThrowingExceptionOutOfScope)
{
    try {
        Fiber::ptr fiber(new Fiber(&throwGenericException));
        fiber->call();
        MORDOR_NOTREACHED();
    } catch (DummyException &) {
    }
}

MORDOR_UNITTEST(Fibers, forceThrowExceptionNewFiberCall)
{
    Fiber::ptr f(new Fiber(&throwRuntimeError));
    boost::exception_ptr exception;
    try {
        throw boost::enable_current_exception(DummyException());
    } catch (...) {
        exception = boost::current_exception();
    }
    MORDOR_TEST_ASSERT_EXCEPTION(f->inject(exception), DummyException);
}

static void catchAndThrowDummy()
{
    try {
        Fiber::yield();
        MORDOR_NOTREACHED();
    } catch (DummyException &) {
        throw;
    }
    MORDOR_NOTREACHED();
}

MORDOR_UNITTEST(Fibers, forceThrowExceptionFiberYield)
{
    Fiber::ptr f(new Fiber(&catchAndThrowDummy));
    boost::exception_ptr exception;
    try {
        throw boost::enable_current_exception(DummyException());
    } catch (...) {
        exception = boost::current_exception();
    }
    f->call();
    MORDOR_TEST_ASSERT_EXCEPTION(f->inject(exception), DummyException);
}

static void catchAndThrowDummyYieldTo(Fiber::ptr caller)
{
    try {
        caller->yieldTo();
        MORDOR_NOTREACHED();
    } catch (DummyException &) {
        throw;
    }
    MORDOR_NOTREACHED();
}

MORDOR_UNITTEST(Fibers, forceThrowExceptionFiberYieldTo)
{
    Fiber::ptr mainFiber = Fiber::getThis();
    Fiber::ptr f(new Fiber(boost::bind(&catchAndThrowDummyYieldTo,
        mainFiber)));
    boost::exception_ptr exception;
    try {
        throw boost::enable_current_exception(DummyException());
    } catch (...) {
        exception = boost::current_exception();
    }
    f->yieldTo();
    MORDOR_TEST_ASSERT_EXCEPTION(f->inject(exception), DummyException);
    f->reset(NULL);
}

static void eatSomeStack()
{
    char UNUSED stackEater[4096];
    stackEater[0] = 1;
}

MORDOR_UNITTEST(Fibers, resetStress)
{
    Fiber::ptr f(new Fiber(&eatSomeStack));
    for (int i = 0; i < 1025; ++i) {
        f->call();
        f->reset();
    }
}

static void gimmeYourFiber(Fiber::ptr &threadFiber)
{
    threadFiber = Fiber::getThis();
}

MORDOR_UNITTEST(Fibers, threadFiberHeldAfterThreadEnd)
{
    Fiber::ptr threadFiber;
    Thread thread(boost::bind(&gimmeYourFiber, boost::ref(threadFiber)));
    thread.join();
}

namespace {
struct LastObject
{
    LastObject(Fiber::ptr mainFiber, int &sequence)
        : m_mainFiber(mainFiber),
          m_sequence(sequence)
    {
        MORDOR_TEST_ASSERT_EQUAL(++sequence, 3);
    }

    ~LastObject()
    {
        MORDOR_TEST_ASSERT(Fiber::getThis() == m_mainFiber.lock());
        MORDOR_TEST_ASSERT_EQUAL(++m_sequence, 6);
    }

private:
    Fiber::weak_ptr m_mainFiber;
    int &m_sequence;
};
struct ExceptionDestructsBeforeFiberDestructsException : virtual Exception
{
    ExceptionDestructsBeforeFiberDestructsException(Fiber::ptr mainFiber,
        int &sequence)
        : m_lastObject(new LastObject(mainFiber, sequence))
    {}
    ~ExceptionDestructsBeforeFiberDestructsException() throw() {}

private:
    boost::shared_ptr<LastObject> m_lastObject;
};
}

static void exceptionDestructsBeforeFiberDestructs(Fiber::ptr mainFiber,
    int &sequence)
{
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 2);
    MORDOR_THROW_EXCEPTION(ExceptionDestructsBeforeFiberDestructsException(
        mainFiber, sequence));
}

MORDOR_UNITTEST(Fibers, exceptionDestructsBeforeFiberDestructs)
{
    int sequence = 0;
    {
        Fiber::ptr mainFiber = Fiber::getThis();
        Fiber::ptr throwingFiber(new Fiber(boost::bind(
            &exceptionDestructsBeforeFiberDestructs, mainFiber,
            boost::ref(sequence))));

        MORDOR_TEST_ASSERT_EQUAL(++sequence, 1);
        try {
            throwingFiber->call();
            MORDOR_NOTREACHED();
        } catch (ExceptionDestructsBeforeFiberDestructsException &) {
            MORDOR_TEST_ASSERT_EQUAL(++sequence, 4);
        }
        MORDOR_TEST_ASSERT_EQUAL(++sequence, 5);
    }
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 7);
}
