#ifndef __MORDOR_EVENTLOOP_H__
#define __MORDOR_EVENTLOOP_H__
// Copyright (c) 2010 - Mozy, Inc.

#include "scheduler.h"
#include "timer.h"

namespace Mordor {

#ifdef WINDOWS
/// Scheduler that processes UI events
class EventLoop : public Scheduler, public TimerManager
{
private:
    struct Initializer
    {
        Initializer();
        ~Initializer();
    };

public:
    EventLoop();
    ~EventLoop();

    bool stopping();

protected:
    void idle();
    void tickle();

private:
    static LRESULT CALLBACK wndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK foregroundIdleProc(int nCode, WPARAM wParam, LPARAM lParam);
    void messagePump();

private:
    static Initializer g_init;

    HWND m_messageWindow;
    HHOOK m_idleHook;
};
#endif

}

#endif
