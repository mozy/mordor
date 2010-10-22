#ifndef __MORDOR_SLEEP_H__
#define __MORDOR_SLEEP_H__
// Copyright (c) 2009 - Decho Corporation

namespace Mordor {

class TimerManager;

void sleep(TimerManager &timerManager, unsigned long long us);
void sleep(unsigned long long us);

}

#endif
