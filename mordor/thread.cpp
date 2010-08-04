// Copyright (c) 2010 - Mozy, Inc.

#include "thread.h"

#ifdef LINUX
#include <syscall.h>
#endif
#ifdef WINDOWS
#include <process.h>
#endif

#include "exception.h"

namespace Mordor {

tid_t gettid()
{
#ifdef WINDOWS
    return GetCurrentThreadId();
#elif defined(LINUX)
    return syscall(__NR_gettid);
#else
    return getpid();
#endif
}

Thread::Thread(boost::function<void ()> dg)
{
#ifdef WINDOWS
    boost::function<void ()> *pDg = new boost::function<void ()>();
    pDg->swap(dg);
    m_hThread = (HANDLE)_beginthreadex(NULL, 0, &Thread::run, pDg, 0,
        (unsigned *)&m_tid);
    if (!m_hThread)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateThread");
#else
    m_dg = dg;
    int rc = pthread_create(&m_thread, NULL, &Thread::run, this);
    if (rc)
        MORDOR_THROW_EXCEPTION_FROM_ERROR_API(rc, "pthread_create");
    m_semaphore.wait();
#endif
}

Thread::~Thread()
{
#ifdef WINDOWS
    if (m_hThread)
        CloseHandle(m_hThread);
#else
    if (m_thread)
        pthread_detach(m_thread);
#endif
}

void
Thread::join()
{
#ifdef WINDOWS
    if (WaitForSingleObject(m_hThread, INFINITE) == WAIT_FAILED)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WaitForSingleObject");
    CloseHandle(m_hThread);
    m_hThread = NULL;
#else
    int rc = pthread_join(m_thread, NULL);
    if (rc)
        MORDOR_THROW_EXCEPTION_FROM_ERROR_API(rc, "pthread_join");
    m_thread = NULL;
#endif
}

#ifdef WINDOWS
unsigned WINAPI
#else
void *
#endif
Thread::run(void *self)
{
    boost::function<void ()> dg;
#ifdef WINDOWS
    boost::function<void ()> *pDg;
    pDg = (boost::function<void ()> *)self;
    dg.swap(*pDg);
    delete pDg;
#else
   Thread *me = (Thread *)self;
    me->m_tid = gettid();
    // Dg might disappear after notifying the caller that the thread is started
    // so copy it on to the stack
    dg.swap(me->m_dg);
    me->m_semaphore.notify();
#endif
    dg();
    return 0;
}

}
