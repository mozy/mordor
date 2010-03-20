// Copyright (c) 2010 - Mozy, Inc.

#include "mordor/pch.h"

#include "eventloop.h"

#include "assert.h"
#include "log.h"

namespace Mordor {

static Logger::ptr g_log = Log::lookup("mordor:eventloop");

static UINT g_tickleMessage = RegisterWindowMessageW(L"MordorEventLoopTickle");

EventLoop::Initializer::Initializer()
{
    WNDCLASSW wndClass;
    memset(&wndClass, 0, sizeof(wndClass));
    wndClass.lpfnWndProc = &EventLoop::wndProc;
    wndClass.hInstance = GetModuleHandleW(NULL);
    wndClass.lpszClassName = L"MordorEventLoop";
    if (!RegisterClassW(&wndClass))
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("RegisterClassW");

}

EventLoop::Initializer::~Initializer()
{
    UnregisterClassW(L"MordorEventLoop", GetModuleHandleW(NULL));
}

EventLoop::Initializer EventLoop::g_init;

EventLoop::EventLoop()
    : Scheduler(1, true, 1)
{
    m_messageWindow = CreateWindowW(L"MordorEventLoop",
        L"Mordor Event Loop",
        0,
        0, 0,
        0, 0,
        HWND_MESSAGE,
        NULL,
        GetModuleHandleW(NULL),
        this);
    if (!m_messageWindow)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateWindowW");
    m_idleHook = SetWindowsHookExW(WH_FOREGROUNDIDLE,
        &EventLoop::foregroundIdleProc,
        NULL,
        GetCurrentThreadId());
    if (!m_idleHook) {
        DestroyWindow(m_messageWindow);
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("SetWindowsHookEx");
    }
    start();
}

EventLoop::~EventLoop()
{
    stop();
    DestroyWindow(m_messageWindow);
    UnhookWindowsHookEx(m_idleHook);
}

bool
EventLoop::stopping()
{
    MSG msg;
    return Scheduler::stopping() &&
        !PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE) && nextTimer() == ~0ull;
}

void
EventLoop::idle()
{
    while (!stopping()) {
        MORDOR_LOG_DEBUG(g_log) << m_messageWindow << " Starting new message pump";
        schedule(boost::bind(&EventLoop::messagePump, this));
        Fiber::yield();
    }
}

void
EventLoop::tickle()
{
    MORDOR_LOG_TRACE(g_log) << m_messageWindow << " tickling";
    if (!PostMessage(m_messageWindow, g_tickleMessage, 0, 0))
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("PostMessage");
}

LRESULT CALLBACK
EventLoop::wndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == g_tickleMessage) {
        MORDOR_LOG_TRACE(g_log) << hWnd << " received tickle";
        Scheduler::yield();
        return 0;
    }
    switch (uMsg) {
        case WM_TIMER:
        {
            MORDOR_LOG_TRACE(g_log) << hWnd << " processing timers";
            EventLoop *self = (EventLoop *)Scheduler::getThis();
            MORDOR_ASSERT(self->m_messageWindow == hWnd);
            std::vector<boost::function<void ()> > expired = self->processTimers();
            if (!expired.empty())
                self->schedule(expired.begin(), expired.end());

            unsigned long long nextTimer = self->nextTimer();
            if (nextTimer == ~0ull) {
                UINT uElapse = (UINT)((nextTimer / 1000) + 1);
                uElapse = std::min<UINT>(USER_TIMER_MINIMUM, uElapse);
                uElapse = std::max<UINT>(USER_TIMER_MAXIMUM, uElapse);
                if (!SetTimer(hWnd, 1, uElapse, NULL))
                    MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("SetTimer");
            } else {
                if (!KillTimer(hWnd, 1))
                    MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("KillTimer");
            }

            return 0;
        }
        default:
            return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }
}

LRESULT CALLBACK
EventLoop::foregroundIdleProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    EventLoop *self = (EventLoop *)Scheduler::getThis();

    MORDOR_LOG_TRACE(g_log) << self->m_messageWindow << " message pump idle";
    if (self->hasWorkToDo())
        self->tickle();

    return CallNextHookEx(self->m_idleHook, nCode, wParam, lParam);
}

void
EventLoop::messagePump()
{
    MORDOR_LOG_DEBUG(g_log) << m_messageWindow << " starting message pump";
    while (!hasWorkToDo() && !stopping()) {
        MSG msg;
        BOOL bRet = GetMessageW(&msg, NULL, 0, 0);
        if (bRet < 0)
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("GetMessageW");
        if (bRet == 0) {
            stop();
            return;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    MORDOR_LOG_DEBUG(g_log) << m_messageWindow << " exiting message pump";
}

}
