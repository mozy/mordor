#ifndef __MORDOR_FIBER_H__
#define __MORDOR_FIBER_H__
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
#include "version.h"

// Fiber impl selection

#ifdef X86_64
#   ifdef WINDOWS
#       define NATIVE_WINDOWS_FIBERS
#   elif defined(POSIX)
#       define ASM_X86_64_POSIX_FIBERS
#   endif
#elif defined(X86)
#   ifdef WINDOWS
#       define NATIVE_WINDOWS_FIBERS
#   elif defined(POSIX)
#       define ASM_X86_POSIX_FIBERS
#   endif
#else
#   error Platform not supported
#endif

#ifdef UCONTEXT_FIBERS
#include <ucontext.h>
#endif

namespace Mordor {

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
    // Returns whoever yielded back to us
    Fiber::ptr yieldTo(bool yieldToCallerOnTerminate = true);
    static void yield();

    State state();

private:
    void call(bool destructor);
    Fiber::ptr yieldTo(bool yieldToCallerOnTerminate, State targetState);
    static void setThis(Fiber *f);
    static void entryPoint();
    static void exitPoint(Fiber::ptr &cur, Fiber *curp, State targetState);

private:
    boost::function<void ()> m_dg;
    void *m_stack, *m_sp;
    size_t m_stacksize;
#ifdef UCONTEXT_FIBERS
    ucontext_t m_ctx;
#endif
    State m_state, m_yielderNextState;
    ptr m_outer, m_yielder;
    weak_ptr m_terminateOuter;
    boost::exception_ptr m_exception;

    static boost::thread_specific_ptr<Fiber> t_fiber;
};

}

#endif // __FIBER_H__
