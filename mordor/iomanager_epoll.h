#ifndef __MORDOR_IOMANAGER_EPOLL_H__
#define __MORDOR_IOMANAGER_EPOLL_H__
// Copyright (c) 2009 - Mozy, Inc.

#include <sys/epoll.h>

#include "scheduler.h"
#include "timer.h"
#include "version.h"

#ifndef LINUX
#error IOManagerEPoll is Linux only
#endif

namespace Mordor {

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
        boost::function<void ()> m_dgIn, m_dgOut;
    };

public:
    IOManagerEPoll(int threads = 1, bool useCaller = true);
    ~IOManagerEPoll();

    bool stopping();

    void registerEvent(int fd, Event events, boost::function<void ()> dg = NULL);
    void cancelEvent(int fd, Event events);

    Timer::ptr registerTimer(unsigned long long us, boost::function<void ()> dg,
        bool recurring = false);

protected:
    bool stopping(unsigned long long &nextTimeout);
    void idle();
    void tickle();

private:
    int m_epfd;
    int m_tickleFds[2];
    std::map<int, AsyncEvent> m_pendingEvents;
    boost::mutex m_mutex;
};

}

#endif
