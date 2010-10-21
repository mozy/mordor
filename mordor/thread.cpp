// Copyright (c) 2010 - Mozy, Inc.

#include "thread.h"

#ifdef LINUX
#include <sys/prctl.h>
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
#elif defined(OSX)
    return mach_thread_self();
#else
    return pthread_self();
#endif
}

#ifdef WINDOWS
//
// Usage: SetThreadName (-1, "MainThread");
//
#define MS_VC_EXCEPTION 0x406D1388

namespace {
typedef struct tagTHREADNAME_INFO
{
   DWORD dwType; // Must be 0x1000.
   LPCSTR szName; // Pointer to name (in user addr space).
   DWORD dwThreadID; // Thread ID (-1=caller thread).
   DWORD dwFlags; // Reserved for future use, must be zero.
} THREADNAME_INFO;
}

static void SetThreadName( DWORD dwThreadID, LPCSTR szThreadName)
{
   THREADNAME_INFO info;
   info.dwType = 0x1000;
   info.szName = szThreadName;
   info.dwThreadID = dwThreadID;
   info.dwFlags = 0;

   __try
   {
      RaiseException( MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(DWORD), (DWORD*)&info );
   }
   __except(EXCEPTION_CONTINUE_EXECUTION)
   {
   }
}
#endif

#ifndef LINUX
namespace {
struct Context {
    boost::function<void ()> dg;
    const char *name;
};
}
#endif

// The object of this function is to start a new thread running dg, and store
// that thread's id in m_tid.  Each platform is a little bit different, but
// there are similarities in different areas.  The areas are:
//
// * How to start a thread
//   Windows - _beginthreadex (essentially CreateThread, but keeps the CRT
//             happy)
//   Everything else - pthread_create
//
// * How to get the native thread id (we like the native thread id because it
//   makes it much easier to correlate with a debugger or performance tool,
//   instead of some random artificial thread id)
//   Linux - there is no documented way to query a pthread for its tid, so we
//           have to have the thread itself call gettid(), and report it back
//           to the constructor.  This means that a) the constructor will not
//           return until the thread is actually running; and b) the thread
//           neads a pointer back to the Thread object to store the tid
//   Windows - happily returns the thread id as part of _beginthreadex
//   OS X - Has a documented, non-portable function to query the
//          mach_thread_port from the pthread
//   Everything else - dunno, it's not special cased, so just use the pthread_t
//                     as the thread id
//
//   In all cases except for Linux, the thread itself doesn't need to know
//   about the Thread object, just which dg to execute.  Because we just fire
//   off the thread and return immediately, it's perfectly possible for the
//   constructor and destructor to run before the thread even starts, so move
//   dg onto the heap in newly allocated memory, and pass the thread start
//   function the pointer to the dg.  It will move the dg onto it's local
//   stack, deallocate the memory on the heap, and then call dg.
//   For Linux, because we need to have the thread tell us its own id, we
//   instead know that the constructor cannot return until the thread has
//   started, so simply pass a pointer to this to the thread, which will
//   set the tid in the object, copy the dg that is a member field onto the
//   stack, signal the constructor that it's ready to go, and then call dg
Thread::Thread(boost::function<void ()> dg, const char *name)
{
#ifdef LINUX
    m_dg = dg;
    void *arg = this;
    m_name = name;
#else
    Context *pContext = new Context;
    pContext->dg.swap(dg);
    pContext->name = name;
    void *arg = pContext;
#endif
#ifdef WINDOWS
    m_hThread = (HANDLE)_beginthreadex(NULL, 0, &Thread::run, arg, 0,
        (unsigned *)&m_tid);
    if (!m_hThread) {
        delete pContext;
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateThread");
    }
    if (name)
        SetThreadName(m_tid, name);
#else
    int rc = pthread_create(&m_thread, NULL, &Thread::run, arg);
    if (rc) {
#ifndef LINUX
        delete pContext;
#endif
        MORDOR_THROW_EXCEPTION_FROM_ERROR_API(rc, "pthread_create");
    }
#ifdef OSX
    m_tid = pthread_mach_thread_np(m_thread);
#elif defined(LINUX)
    m_semaphore.wait();
#else
    m_tid = m_thread;
#endif
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
    m_thread = 0;
#endif
}

#ifdef WINDOWS
unsigned WINAPI
#else
void *
#endif
Thread::run(void *arg)
{
   boost::function<void ()> dg;
#ifdef LINUX
   Thread *self = (Thread *)arg;
    self->m_tid = gettid();
    // Dg might disappear after notifying the caller that the thread is started
    // so copy it on to the stack
    dg.swap(self->m_dg);
    if (self->m_name)
        prctl(PR_SET_NAME, self->m_name, 0, 0, 0);
    self->m_semaphore.notify();
#else
    Context *pContext = (Context *)arg;
    dg.swap(pContext->dg);
#ifdef OSX
    if (pContext->name)
        pthread_setname_np(pContext->name);
#endif
    delete pContext;
#endif
    dg();
    return 0;
}

}
