#ifndef __MORDOR_IOMANAGER_EPOLL_H__
#define __MORDOR_IOMANAGER_EPOLL_H__
// Copyright (c) 2009 - Mozy, Inc.

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
        NONE  = 0x0000,
        READ  = 0x0001,
        WRITE = 0x0004,
        CLOSE = 0x2000
    };

private:
    struct AsyncState : boost::noncopyable
    {
        AsyncState();
        ~AsyncState();

        struct EventContext
        {
            EventContext() : scheduler(NULL) {}
            Scheduler *scheduler;
            boost::shared_ptr<Fiber> fiber;
            boost::function<void ()> dg;
        };

        EventContext &contextForEvent(Event event);
        bool triggerEvent(Event event, size_t &pendingEventCount);
        void resetContext(EventContext &);

        int m_fd;
        EventContext m_in, m_out, m_close;
        Event m_events;
        boost::mutex m_mutex;

    private:
        void asyncResetContextFiber(boost::shared_ptr<Fiber>);
    };

public:
    IOManager(size_t threads = 1, bool useCaller = true);
    ~IOManager();

    bool stopping();

    void registerEvent(int fd, Event events,
        boost::function<void ()> dg = NULL);
    /// Will not cause the event to fire
    /// @return If the event was successfully unregistered before firing normally
    bool unregisterEvent(int fd, Event events);
    /// Will cause the event to fire
    bool cancelEvent(int fd, Event events);

protected:
    bool stopping(unsigned long long &nextTimeout);
    void idle();
    void tickle();

    void onTimerInsertedAtFront() { tickle(); }

private:
    int m_epfd;
    int m_tickleFds[2];
    size_t m_pendingEventCount;
    boost::mutex m_mutex;
    std::vector<AsyncState *> m_pendingEvents;
};

}

#endif
