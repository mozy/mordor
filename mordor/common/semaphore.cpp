// Copyright (c) 2009 - Decho Corp.

#include "semaphore.h"

#include <cassert>

#include "exception.h"

#ifndef WINDOWS
#include <errno.h>
#endif

#ifdef OSX
#include <mach/mach_init.h>
#include <mach/task.h>
#endif

Semaphore::Semaphore(unsigned int count)
{
#ifdef WINDOWS
    m_semaphore = CreateSemaphore(NULL, count, 2147483647, NULL);
    if (m_semaphore == NULL) {
        throwExceptionFromLastError();
    }
#elif defined(OSX)
    m_task = mach_task_self();
    if (semaphore_create(m_task, &m_semaphore, SYNC_POLICY_FIFO, count)) {
        throwExceptionFromLastError();
    }
#else
    if (sem_init(&m_semaphore, 0, count)) {
        throwExceptionFromLastError();
    }
#endif
}

Semaphore::~Semaphore()
{
#ifdef WINDOWS
    BOOL bRet = CloseHandle(m_semaphore);
    assert(bRet);
#elif defined(OSX)
    int rc = semaphore_destroy(m_task, m_semaphore);
    assert(!rc);
#else
    int rc = sem_destroy(&m_semaphore);
    assert(!rc);
#endif
}

void
Semaphore::wait()
{
#ifdef WINDOWS
    DWORD dwRet = WaitForSingleObject(m_semaphore, INFINITE);
    if (dwRet != WAIT_OBJECT_0) {
        throwExceptionFromLastError();
    }
#elif defined(OSX)
    while (true) {
        if (!semaphore_wait(m_semaphore))
            return;
        if (errno != EINTR) {
            throwExceptionFromLastError();
        }
    }
#else
    while (true) {
        if (!sem_wait(&m_semaphore))
            return;
        if (errno != EINTR) {
            throwExceptionFromLastError();
        }
    }
#endif
}

void
Semaphore::notify()
{
#ifdef WINDOWS
    if (!ReleaseSemaphore(m_semaphore, 1, NULL)) {
        throwExceptionFromLastError();
    }
#elif defined(OSX)
    semaphore_signal(m_semaphore);
#else
    if (sem_post(&m_semaphore)) {
        throwExceptionFromLastError();
    }
#endif
}
