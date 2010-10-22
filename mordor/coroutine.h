#ifndef __MORDOR_COROUTINE_H__
#define __MORDOR_COROUTINE_H__
// Copyright (c) 2009 - Decho Corporation

#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>

#include "fiber.h"

namespace Mordor {

struct DummyVoid;

template <class Result, class Arg = DummyVoid>
class Coroutine : public boost::enable_shared_from_this<Coroutine<Result, Arg> >
{
public:
    typedef boost::shared_ptr<Coroutine> ptr;
    typedef boost::weak_ptr<Coroutine> weak_ptr;
public:
    Coroutine()
    {
        m_fiber = Fiber::ptr(new Fiber(boost::bind(&Coroutine::run, this)));
    }

    Coroutine(boost::function<Result (typename Coroutine<Result, Arg>::ptr, Arg)> dg)
        : m_dg1(dg)
    {
        m_fiber = Fiber::ptr(new Fiber(boost::bind(&Coroutine::run, this)));
    }

    Coroutine(boost::function<void (typename Coroutine<Result, Arg>::ptr, Arg)> dg)
        : m_dg2(dg)
    {
        m_fiber = Fiber::ptr(new Fiber(boost::bind(&Coroutine::run, this)));
    }

    void reset()
    {
        m_fiber->reset();
    }

    void reset(boost::function<Result (typename Coroutine<Result, Arg>::ptr, Arg)> dg)
    {
        m_fiber->reset();
        m_dg1 = dg;
        m_dg2 = NULL;
    }

    void reset(boost::function<void (typename Coroutine<Result, Arg>::ptr, Arg)> dg)
    {
        m_fiber->reset();
        m_dg2 = NULL;
        m_dg2 = dg;
    }

    Result call(Arg arg)
    {
        m_arg = arg;
        m_fiber->call();
        return m_result;
    }

    Arg yield(Result r)
    {
        m_result = r;
        Fiber::yield();
        return m_arg;
    }
/*
    template <class OtherArg>
    Arg yieldTo(typename Coroutine<Arg, OtherArg>::ptr other, OtherArg arg)
    {
        other->m_arg = arg;
        other->m_fiber->yieldTo();
        return m_result;
    }
*/
    Fiber::State state()
    {
        return m_fiber->state();
    }

private:
    void run()
    {
        if (m_dg1) {
            m_result = m_dg1(boost::enable_shared_from_this<Coroutine<Result, Arg> >::shared_from_this(), m_arg);
        } else {
            m_dg2(boost::enable_shared_from_this<Coroutine<Result, Arg> >::shared_from_this(), m_arg);
            m_result = Result();
        }
    }

private:
    boost::function<Result (typename Coroutine<Result, Arg>::ptr, Arg)> m_dg1;
    boost::function<void (typename Coroutine<Result, Arg>::ptr, Arg)> m_dg2;
    Result m_result;
    Arg m_arg;
    Fiber::ptr m_fiber;
};


template <class Result>
class Coroutine<Result, DummyVoid> : public boost::enable_shared_from_this<Coroutine<Result> >
{
public:
    typedef boost::shared_ptr<Coroutine> ptr;
    typedef boost::weak_ptr<Coroutine> weak_ptr;
public:
    Coroutine()
    {
        m_fiber = Fiber::ptr(new Fiber(boost::bind(&Coroutine::run, this)));
    }

    Coroutine(boost::function<Result (typename Coroutine<Result>::ptr)> dg)
        : m_dg(dg)
    {
        m_fiber = Fiber::ptr(new Fiber(boost::bind(&Coroutine::run, this)));
    }

    void reset()
    {
        m_fiber->reset();
    }

    void reset(boost::function<Result (typename Coroutine<Result>::ptr)> dg)
    {
        m_fiber->reset();
        m_dg = dg;
    }

    Result call()
    {
        m_fiber->call();
        return m_result;
    }

    void yield(Result r)
    {
        m_result = r;
        Fiber::yield();
    }

    Fiber::State state()
    {
        return m_fiber->state();
    }

private:
    void run()
    {
        m_result = m_dg(boost::enable_shared_from_this<Coroutine<Result> >::shared_from_this());
    }

private:
    boost::function<Result (typename Coroutine<Result>::ptr)> m_dg;
    Result m_result;
    Fiber::ptr m_fiber;
};

}

#endif
