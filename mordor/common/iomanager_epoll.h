#ifndef __IOMANAGER_EPOLL_H__
#define __IOMANAGER_EPOLL_H__
// Copyright (c) 2009 - Decho Corp.

#include "scheduler.h"

struct AsyncEventEPoll
{
};

class IOManagerEPoll : public Scheduler
{
public:
    void registerEvent(AsyncEventEPoll *event);
protected:
    void idle();
    void tickle();
};

#endif

