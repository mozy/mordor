#ifndef __MORDOR_SLEEP_H__
#define __MORDOR_SLEEP_H__
// Copyright (c) 2009 - Mozy, Inc.

namespace Mordor {

class TimerManager;

/// Suspend execution of the current thread
/// @note This is a normal sleep, and will block the current thread
/// @param us How long to sleep, in microseconds
void sleep(unsigned long long us);

/// Suspend execution of the current Fiber
/// @note This will use the TimerManager to yield the current Fiber and allow
/// other Fibers to run until this Fiber is ready to run again.
/// @param timerManager The TimerManager (typically an IOManager) to use to
/// to control this sleep
/// @param us How long to sleep, in microseconds
/// @pre Scheduler::getThis() != NULL
void sleep(TimerManager &timerManager, unsigned long long us);

}

#endif
