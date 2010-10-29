// Copyright (c) 2009 - Mozy, Inc.

#include <boost/bind.hpp>

#include "sleep.h"

#include "assert.h"
#include "fiber.h"
#include "scheduler.h"
#include "timer.h"

namespace Mordor {

static void scheduleMe(Scheduler *scheduler, Fiber::ptr fiber)
{
    scheduler->schedule(fiber);
}

void
sleep(TimerManager &timerManager, unsigned long long us)
{
    MORDOR_ASSERT(Scheduler::getThis());
    timerManager.registerTimer(us,
        boost::bind(&scheduleMe, Scheduler::getThis(), Fiber::getThis()));
    Scheduler::yieldTo();
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
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("nanosleep");
        }
        break;
    }
#endif
}

}
