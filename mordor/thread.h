#ifndef __MORDOR_THREAD_H__
#define __MORDOR_THREAD_H__
// Copyright (c) 2010 - Mozy, Inc.

#include <iosfwd>

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>

#include "version.h"
#ifndef WINDOWS
#include "semaphore.h"
#endif

namespace Mordor {

#ifdef WINDOWS
typedef DWORD tid_t;
#elif defined(LINUX)
typedef pid_t tid_t;
#elif defined(OSX)
typedef mach_port_t tid_t;
#else
typedef pthread_t tid_t;
#endif

inline tid_t emptytid() { return (tid_t)-1; }
tid_t gettid();

class Thread : boost::noncopyable
{
public:
    Thread(boost::function<void ()> dg);
    ~Thread();

    tid_t tid() const { return m_tid; }

    void join();

private:
    static
#ifdef WINDOWS
    unsigned WINAPI
#else
    void *
#endif
    run(void *self);

private:
    tid_t m_tid;
#ifdef WINDOWS
    HANDLE m_hThread;
#else
    pthread_t m_thread;
#endif

#ifdef LINUX
    boost::function<void ()> m_dg;
    Semaphore m_semaphore;
#endif
};

}

#endif
