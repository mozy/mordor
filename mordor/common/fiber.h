#ifndef __FIBER_H__
#define __FIBER_H__
// Copyright (c) 2009 - Decho Corp.

#ifdef WIN32
#else
#include <stddef.h>
#endif

#include <boost/enable_shared_from_this.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/tss.hpp>

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
    void yieldTo(bool yieldToCallerOnTerminate, bool terminateMe);
    static void setThis(Fiber *f);
    static void entryPoint();

private:
    boost::function<void ()> m_dg;
    void *m_stack, *m_sp;
    size_t m_stacksize;
    State m_state, m_yielderNextState;
    ptr m_outer, m_yielder;
    weak_ptr m_terminateOuter;

    static boost::thread_specific_ptr<Fiber> t_fiber;
};

#endif // __FIBER_H__
