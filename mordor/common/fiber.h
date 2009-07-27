#ifndef __FIBER_H__
#define __FIBER_H__
// Copyright (c) 2009 - Decho Corp.

#ifdef WIN32
#else
#include <stddef.h>
#endif

#include <list>

#include <boost/enable_shared_from_this.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/tss.hpp>

#include "exception.h"

class Fiber : public boost::enable_shared_from_this<Fiber>
{
public:
    typedef boost::shared_ptr<Fiber> ptr;
    typedef boost::weak_ptr<Fiber> weak_ptr;

    enum State
    {
        INIT,
        HOLD,
        EXEC,
        EXCEPT,
        TERM
    };

public:
    // Default constructor gets the currently executing fiber
    Fiber();
    Fiber(boost::function<void ()> dg, size_t stacksize = 0);
    ~Fiber();

    void reset();
    void reset(boost::function<void ()> dg);

    static ptr getThis();

    void call();
    void yieldTo(bool yieldToCallerOnTerminate = true);
    static void yield();

    State state();

private:
    void call(bool destructor);
    void yieldTo(bool yieldToCallerOnTerminate, State targetState);
    static void setThis(Fiber *f);
    static void entryPoint();
    static void exitPoint(Fiber::ptr &cur, Fiber *curp, State targetState);
    void throwExceptions();

private:
    boost::function<void ()> m_dg;
    void *m_stack, *m_sp;
    size_t m_stacksize;
    State m_state, m_yielderNextState;
    ptr m_outer, m_yielder;
    weak_ptr m_terminateOuter;
    std::exception *m_exception;

    static boost::thread_specific_ptr<Fiber> t_fiber;
};

#define THROW_ORIGINAL_EXCEPTION(exceptionPtr, exceptionType)                   \
    if (typeid(*exceptionPtr) == typeid(exceptionType))                         \
            throw *dynamic_cast<exceptionType *>(exceptionPtr);

class FiberException : public NestedException
{
    friend class Fiber;
public:
    FiberException(Fiber::ptr fiber, std::exception &ex);

    static void registerExceptionHandler(boost::function<void (std::exception &)> handler);

    Fiber::ptr fiber() { return m_fiber; }

private:
    Fiber::ptr m_fiber;

    static std::list<boost::function<void (std::exception &)> > m_handlers;
};

#endif // __FIBER_H__
