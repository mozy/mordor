#ifndef __IOMANAGER_EPOLL_H__
#define __IOMANAGER_EPOLL_H__
// Copyright (c) 2009 - Decho Corp.

#include <sys/epoll.h>

#include "scheduler.h"
#include "timer.h"
#include "version.h"

#ifndef LINUX
#error IOManagerEPoll is Linux only
#endif

class IOManagerEPoll : public Scheduler, public TimerManager
{
public:
    enum Event {
        READ = EPOLLIN,
        WRITE = EPOLLOUT
    };

private:
    struct AsyncEvent
    {
        epoll_event event;

        Scheduler *m_schedulerIn, *m_schedulerOut;
        Fiber::ptr m_fiberIn, m_fiberOut;
    };

public:
    IOManagerEPoll(int threads = 1, bool useCaller = true);
    ~IOManagerEPoll();

    void registerEvent(int fd, Event events);
    void cancelEvent(int fd, Event events);

    Timer::ptr registerTimer(unsigned long long us, boost::function<void ()> dg,
        bool recurring = false);

protected:
    void idle();
    void tickle();

private:
    int m_epfd;
    int m_tickleFds[2];
    std::map<int, AsyncEvent> m_pendingEvents;
    boost::mutex m_mutex;
};

#endif
