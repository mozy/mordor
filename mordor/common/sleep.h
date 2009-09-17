#ifndef __SLEEP_H__
#define __SLEEP_H__
// Copyright (c) 2009 - Decho Corp.

class TimerManager;

void sleep(TimerManager &timerManager, unsigned long long us);
void sleep(unsigned long long us);

#endif
