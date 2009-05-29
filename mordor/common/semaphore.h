#ifndef __SEMAPHORE_H__
#define __SEMAPHORE_H__
// Copyright (c) 2009 - Decho Corp.

#include "version.h"

#ifdef WINDOWS
#include <windows.h>
#endif

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
#else
#endif
};

#endif
