#ifndef __MORDOR_SEMAPHORE_H__
#define __MORDOR_SEMAPHORE_H__
// Copyright (c) 2009 - Decho Corp.

#include "version.h"

#ifdef WINDOWS
#include <windows.h>
#elif defined(OSX)
#include <mach/semaphore.h>
#elif defined(FREEBSD)
#else
#include <semaphore.h>
#endif

namespace Mordor {

class Semaphore
{
public:
    Semaphore(unsigned int count = 0);
    ~Semaphore();

    void wait();

    void notify();   

private:
#ifdef WINDOWS
    HANDLE m_semaphore;
#elif defined(OSX)
    task_t m_task;
    semaphore_t m_semaphore;
#elif defined(FREEBSD)
    int m_semaphore;
#else
    sem_t m_semaphore;
#endif
};

}

#endif
