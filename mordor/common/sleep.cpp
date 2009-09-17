// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include <boost/bind.hpp>

#include "sleep.h"

#include "assert.h"
#include "scheduler.h"
#include "timer.h"

static void scheduleMe(Scheduler *scheduler, Fiber::ptr fiber)
{
    scheduler->schedule(fiber);
}

void
sleep(TimerManager &timerManager, unsigned long long us)
{
    ASSERT(Scheduler::getThis());
    ASSERT(Fiber::getThis());
    timerManager.registerTimer(us,
        boost::bind(&scheduleMe, Scheduler::getThis(), Fiber::getThis()));
    Scheduler::getThis()->yieldTo();
}

void
sleep(unsigned long long us)
{
#ifdef WINDOWS
    Sleep((DWORD)(us / 1000));
#else
    struct timespec ts;
    ts.tv_sec = us / 1000000;
    ts.tv_nsec = (us % 1000) * 1000;
    while (true) {
        if (nanosleep(&ts, &ts) == -1) {
            if (errno == EINTR)
                continue;
            throwExceptionFromLastError();
        }
        break;
    }
#endif
}
