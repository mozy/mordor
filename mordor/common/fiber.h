#ifndef __FIBER_H__
#define __FIBER_H__
// Copyright (c) 2009 - Decho Corp.

#ifdef WIN32
#else
#include <stddef.h>
#endif

class Fiber
{
public:
    enum State
    {
        HOLD,
        EXEC,
        TERM
    };

public:
    // Default constructor gets the currently executing fiber
    Fiber();
    Fiber(void (*fn)(), size_t stacksize = 0);
    ~Fiber();

    void reset();
    void reset(void (*fn)());

    static Fiber* getThis();

    void call();
    void yieldTo(bool yieldToCallerOnTerminate = true);
    static void yield();

    State state();

private:
    void yieldTo(bool yieldToCallerOnTerminate, bool terminateMe);
    static void setThis(Fiber* f);
    static void entryPoint();

private:
    void (*m_fn)();
    void *m_stack, *m_sp;
    size_t m_stacksize;
    State m_state, m_yielderNextState;
    Fiber *m_outer, *m_terminateOuter, *m_yielder;
};

#endif // __FIBER_H__
