#ifndef __IOMANAGER_EPOLL_H__
#define __IOMANAGER_EPOLL_H__
// Copyright (c) 2009 - Decho Corp.

#include <sys/types.h>
#include <sys/event.h>

#include <map>

#include "scheduler.h"
#include "timer.h"
#include "version.h"

#ifndef BSD
#error IOManagerKQueue is BSD only
#endif

class IOManagerKQueue : public Scheduler, public TimerManager
{
public:
    enum Event {
        READ = EVFILT_READ,
        WRITE = EVFILT_WRITE
    };

private:
    struct AsyncEvent
    {
        struct kevent event;

        Scheduler *m_scheduler;
        Fiber::ptr m_fiber;
        boost::function<void ()> m_dg;

        bool operator<(const AsyncEvent &rhs) const
        { if (event.ident < rhs.event.ident) return true; return event.filter < rhs.event.filter; }
    };

public:
    IOManagerKQueue(int threads = 1, bool useCaller = true);
    ~IOManagerKQueue();

    void registerEvent(int fd, Event events, boost::function<void ()> dg = NULL);
    void cancelEvent(int fd, Event events);

    Timer::ptr registerTimer(unsigned long long us, boost::function<void ()> dg,
        bool recurring = false);

protected:
    void idle();
    void tickle();

private:
    int m_kqfd;
    int m_tickleFds[2];
    std::map<std::pair<int, Event>, AsyncEvent> m_pendingEvents;
    boost::mutex m_mutex;
};

#endif

