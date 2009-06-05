// Copyright (c) 2009 - Decho Corp.

#include "semaphore.h"

#include <cassert>

#include "exception.h"

#ifndef WINDOWS
#include <errno.h>
#endif

Semaphore::Semaphore(unsigned int count)
{
#ifdef WINDOWS
    m_semaphore = CreateSemaphore(NULL, count, 2147483647, NULL);
    if (m_semaphore == NULL) {
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
#else
    if (sem_post(&m_semaphore)) {
        throwExceptionFromLastError();
    }
#endif
}
