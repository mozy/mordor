#ifndef __MORDOR_EVENTLOOP_H__
#define __MORDOR_EVENTLOOP_H__
// Copyright (c) 2010 - Mozy, Inc.

#ifdef WINDOWS

#include "scheduler.h"
#include "socket.h"
#include "timer.h"

namespace Mordor {

/// Scheduler that processes UI events
class EventLoop : public Scheduler, public TimerManager
{
public:
    enum Event {
        READ = FD_READ,
        WRITE = FD_WRITE,
        ACCEPT = FD_ACCEPT,
        CONNECT = FD_CONNECT
    };
private:
    struct Initializer
    {
        Initializer();
        ~Initializer();
    };

    struct AsyncEvent
    {
        Event m_events;

        Scheduler *m_schedulerRead, *m_schedulerWrite;
        Fiber::ptr m_fiberRead, m_fiberWrite;
    };

public:
    EventLoop();
    ~EventLoop();

    bool stopping();

    void registerEvent(SOCKET socket, Event events);
    /// Will not cause the event to fire
    /// @return If the event was successfully unregistered before firing normally
    bool unregisterEvent(SOCKET socket, Event events);
    /// Will cause the event to fire
    void cancelEvent(SOCKET socket, Event events);
    /// Unregisters all events for the SOCKET; for use by Socket::accept
    void clearEvents(SOCKET socket);

protected:
    void idle();
    void tickle();
    void onTimerInsertedAtFront();

private:
    static LRESULT CALLBACK wndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK foregroundIdleProc(int nCode, WPARAM wParam, LPARAM lParam);
    void messagePump();

private:
    static Initializer g_init;

    HWND m_messageWindow;
    HHOOK m_idleHook;
    std::map<SOCKET, AsyncEvent> m_pendingEvents;
    boost::mutex m_mutex;
};

}
#endif

#endif
