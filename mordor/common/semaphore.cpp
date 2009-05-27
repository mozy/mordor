// Copyright (c) 2009 - Decho Corp.

#include "semaphore.h"

#include <cassert>

#ifdef WINDOWS
Semaphore::Semaphore(unsigned int count)
{
    m_semaphore = CreateSemaphore(NULL, count, 2147483647, NULL);
    if (m_semaphore == NULL) {
        // TODO: throw exception
    }
}

Semaphore::~Semaphore()
{
    BOOL bRet = CloseHandle(m_semaphore);
    assert(bRet);
}

void
Semaphore::wait()
{
    DWORD dwRet = WaitForSingleObject(m_semaphore, INFINITE);
    // TODO: throw exception
}

void
Semaphore::notify()
{
    if (!ReleaseSemaphore(m_semaphore, 1, NULL)) {
        // TODO: throw exception
    }
}
#else
#endif
