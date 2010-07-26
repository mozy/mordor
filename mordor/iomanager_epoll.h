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

class Fiber;

class IOManager : public Scheduler, public TimerManager
{
public:
    enum Event {
        READ = EPOLLIN,
        WRITE = EPOLLOUT,
        // CLOSE and ERROR are for compatibility with legacy apps *only*
        // If you register for ERROR, and an error happens, no other events
        // will fire if an ERROR fires
        // If you register for CLOSE, a WRITE will not fire if a CLOSE
        // fires
        CLOSE = EPOLLHUP,
        ERROR = EPOLLERR
    };

private:
    struct AsyncEvent
    {
        epoll_event event;

        Scheduler *m_schedulerIn, *m_schedulerOut, *m_schedulerClose, *m_schedulerError;
        boost::shared_ptr<Fiber> m_fiberIn, m_fiberOut, m_fiberClose, m_fiberError;
        boost::function<void ()> m_dgIn, m_dgOut, m_dgClose, m_dgError;
    };

public:
    IOManager(size_t threads = 1, bool useCaller = true);
    ~IOManager();

    bool stopping();

    void registerEvent(int fd, Event events, boost::function<void ()> dg = NULL);
    /// Will not cause the event to fire
    /// @return If the event was successfully unregistered before firing normally
    bool unregisterEvent(int fd, Event events);
    /// Will cause the event to fire
    void cancelEvent(int fd, Event events);

protected:
    bool stopping(unsigned long long &nextTimeout);
    void idle();
    void tickle();

    void onTimerInsertedAtFront() { tickle(); }

private:
    int m_epfd;
    int m_tickleFds[2];
    std::map<int, AsyncEvent> m_pendingEvents;
    boost::mutex m_mutex;
};

}

#endif
