#ifndef __MORDOR_IOMANAGER_EVENT_H__
#define __MORDOR_IOMANAGER_EVENT_H__
// Copyright (c) 2009 - Mozy, Inc.

#include <iostream>

#include <event.h>

#include "scheduler.h"
#include "timer.h"
#include "version.h"

namespace Mordor {

struct LibEventException : virtual NativeException {};

struct LibEventBaseNewFailed : virtual LibEventException {};
struct LibEventAddFailed : virtual LibEventException {};
struct LibEventAddTickleFailed : virtual LibEventException {};
struct LibEventDelFailed : virtual LibEventException {};
struct LibEventBaseSetFailed : virtual LibEventException {};
struct LibEventBaseLoopFailed : virtual LibEventException {};

class IOManagerEvent : public Scheduler, public TimerManager
{
public:
    typedef boost::function<void ()> Delegate;

    enum Event {
        READ = EV_READ,
        WRITE = EV_WRITE
    };

    struct AsyncEvent;

    class AsyncEventDispatch
    {
    public:
        AsyncEventDispatch() : m_scheduler(NULL) { }

        void set(boost::function<void ()>& dg);
        void transfer(AsyncEventDispatch& aed);
        void schedule();

    private:
        Scheduler* m_scheduler;
        Fiber::ptr m_fiber;
        boost::function<void ()> m_dg;
    };

    struct AsyncEvent
    {
        AsyncEvent() : m_queued(false), m_events(0) { }

        // True if this event is currently in the add or mod queue
        bool m_queued;

        // events the caller is registered for
        int m_events;

        // thread the event has been running on
        boost::thread::id m_tid;

        // Libevent state
        struct event m_ev;

        // State for read notification
        AsyncEventDispatch m_read;

        // State for write notification
        AsyncEventDispatch m_write;
    };

private:
    struct TickleState
    {
        bool m_tickled;
        int m_fds[2];
    };

    typedef std::map<boost::thread::id, TickleState*> ThreadTickleState;
    typedef std::map<int, AsyncEvent> RegisteredEvents;
    typedef std::list<AsyncEvent*> EventList;
    typedef std::map<boost::thread::id, EventList> ModifyEvents;

public:
    IOManagerEvent(int threads = 1, bool useCaller = true);
    ~IOManagerEvent();

    void registerEvent(int fd, Event event, Delegate dg = NULL);
    void cancelEvent(int fd, Event events);

    Timer::ptr registerTimer(unsigned long long us, Delegate dg,
                             bool recurring = false);

protected:
    void idle();
    void tickle();
    void tickle(boost::thread::id id);

private:
    void initThread();
    void cleanupThread();
    void addTickle(unsigned long long nextTimeout);
    bool checkDone(unsigned long long& nextTimeout);
    void processAdds();
    void processMods();
    static void tickled(int fd, short events, void* self);
    void addEvent(AsyncEvent* ev);
    static void processEvent(AsyncEvent* ev, int events);
    static void eventCb(int fd, short events, void* self);

    void tickleLocked();
    void tickleLocked(boost::thread::id id);
    void tickleLocked(TickleState* ts);

private:
    static ThreadLocalStorage<TickleState*> t_tickleState;
    static ThreadLocalStorage<struct event_base*> t_evBase;
    static ThreadLocalStorage<struct event*> t_evTickle;;

    boost::mutex m_mutex;
    ThreadTickleState m_threadTickleState;
    RegisteredEvents m_registeredEvents;
    EventList m_addEvents;
    size_t m_addEventsSize;
    ModifyEvents m_modEvents;
};

std::ostream& operator<<(std::ostream& os, const struct event& ev);
std::ostream& operator<<(std::ostream& os,
                         const struct IOManagerEvent::AsyncEvent& ev);

}

#endif

