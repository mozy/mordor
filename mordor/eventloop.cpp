// Copyright (c) 2010 - Mozy, Inc.

#include "mordor/pch.h"

#include "eventloop.h"

#include "assert.h"
#include "fiber.h"

namespace Mordor {

static Logger::ptr g_log = Log::lookup("mordor:eventloop");

static UINT g_tickleMessage = RegisterWindowMessageW(L"MordorEventLoopTickle");
static UINT g_socketIOMessage = RegisterWindowMessageW(L"MordorEventLoopSocketIO");

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
    m_messageWindow = NULL;
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
EventLoop::registerEvent(SOCKET socket, Event events)
{
    events = (Event)(events & (FD_READ | FD_WRITE | FD_CONNECT | FD_ACCEPT));
    MORDOR_ASSERT(events);
    boost::mutex::scoped_lock lock(m_mutex);
    std::map<SOCKET, AsyncEvent>::iterator it = m_pendingEvents.find(socket);
    AsyncEvent *event;
    if (it == m_pendingEvents.end()) {
        event = &m_pendingEvents[socket];
        event->m_events = (Event)0;
    } else {
        event = &it->second;
    }
    // Not already set
    MORDOR_ASSERT(!(event->m_events & events));
    if ((events & FD_READ) || (events & FD_ACCEPT)) {
        MORDOR_ASSERT(!(event->m_events & (FD_READ | FD_ACCEPT)));
        event->m_schedulerRead = Scheduler::getThis();
        event->m_fiberRead = Fiber::getThis();
    }
    if ((events & FD_WRITE) || (events & FD_CONNECT)) {
        MORDOR_ASSERT(!(event->m_events & (FD_WRITE | FD_CONNECT)));
        event->m_schedulerWrite = Scheduler::getThis();
        event->m_fiberWrite = Fiber::getThis();
    }
    event->m_events = (Event)(event->m_events | events);
    int result = WSAAsyncSelect(socket, m_messageWindow, g_socketIOMessage,
        (long)event->m_events);
    MORDOR_LOG_LEVEL(g_log, result ? Log::ERROR : Log::VERBOSE)
        << this << " WSAAsyncSelect(" << socket << ", " << m_messageWindow
        << ", " << g_socketIOMessage << ", " << event->m_events << "): "
        << result << " (" << GetLastError() << ")";
    if (result)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WSAAsyncSelect");
}


bool
EventLoop::unregisterEvent(SOCKET socket, Event events)
{
    boost::mutex::scoped_lock lock(m_mutex);
    std::map<SOCKET, AsyncEvent>::iterator it = m_pendingEvents.find(socket);
    if (it == m_pendingEvents.end())
        return false;
    // Nothing matching
    if (!(events & it->second.m_events))
        return false;
    AsyncEvent &event = it->second;
    if ((events & FD_READ) || (events & FD_ACCEPT)) {
        event.m_schedulerRead = NULL;
        event.m_fiberRead.reset();
    }
    if ((events & FD_WRITE) || (events & FD_CONNECT)) {
        event.m_schedulerWrite = NULL;
        event.m_fiberWrite.reset();
    }
    event.m_events = (Event)(event.m_events & ~events);
    int result = WSAAsyncSelect(socket, m_messageWindow, g_socketIOMessage,
        (long)event.m_events);
    MORDOR_LOG_LEVEL(g_log, result ? Log::ERROR : Log::VERBOSE)
        << this << " WSAAsyncSelect(" << socket << ", " << m_messageWindow
        << ", " << g_socketIOMessage << ", " << event.m_events << "): "
        << result << " (" << GetLastError() << ")";
    if (result)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WSAAsyncSelect");
    if (!event.m_events)
        m_pendingEvents.erase(it);
    return true;
}

void
EventLoop::cancelEvent(SOCKET socket, Event events)
{
    boost::mutex::scoped_lock lock(m_mutex);
    std::map<SOCKET, AsyncEvent>::iterator it = m_pendingEvents.find(socket);
    if (it == m_pendingEvents.end())
        return;
    // Nothing matching
    if (!(events & it->second.m_events))
        return;
    AsyncEvent &event = it->second;
    if ((events & FD_READ) || (events & FD_ACCEPT)) {
        if ((event.m_events & FD_READ) || (event.m_events & FD_ACCEPT))
            event.m_schedulerRead->schedule(event.m_fiberRead);
        event.m_schedulerRead = NULL;
        event.m_fiberRead.reset();
    }
    if ((events & FD_WRITE) || (events & FD_CONNECT)) {
        if ((event.m_events & FD_READ) || (event.m_events & FD_ACCEPT))
            event.m_schedulerWrite->schedule(event.m_fiberWrite);
        event.m_schedulerWrite = NULL;
        event.m_fiberWrite.reset();
    }
    event.m_events = (Event)(event.m_events & ~events);
    int result = WSAAsyncSelect(socket, m_messageWindow, g_socketIOMessage,
        (long)event.m_events);
    MORDOR_LOG_LEVEL(g_log, result ? Log::ERROR : Log::VERBOSE)
        << this << " WSAAsyncSelect(" << socket << ", " << m_messageWindow
        << ", " << g_socketIOMessage << ", " << event.m_events << "): "
        << result << " (" << GetLastError() << ")";
    if (result)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WSAAsyncSelect");
    if (!event.m_events)
        m_pendingEvents.erase(it);
}

void
EventLoop::clearEvents(SOCKET socket)
{
    if (WSAAsyncSelect(socket, m_messageWindow, 0, 0))
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WSAAsyncSelect");
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

void
EventLoop::onTimerInsertedAtFront()
{
    unsigned long long next = nextTimer();
    MORDOR_ASSERT(next != ~0ull);
    UINT uElapse = (UINT)((next / 1000) + 1);
    uElapse = std::max<UINT>(USER_TIMER_MINIMUM, uElapse);
    uElapse = std::min<UINT>(USER_TIMER_MAXIMUM, uElapse);
    if (!SetTimer(m_messageWindow, 1, uElapse, NULL))
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("SetTimer");
}

LRESULT CALLBACK
EventLoop::wndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    EventLoop *self = (EventLoop *)Scheduler::getThis();
    MORDOR_ASSERT(self->m_messageWindow == hWnd ||
        self->m_messageWindow == NULL);

    if (uMsg == g_tickleMessage) {
        MORDOR_LOG_TRACE(g_log) << hWnd << " received tickle";
        Scheduler::yield();
        return 0;
    } else if (uMsg == g_socketIOMessage) {
        boost::mutex::scoped_lock lock(self->m_mutex);
        std::map<SOCKET, AsyncEvent>::iterator it =
            self->m_pendingEvents.find((SOCKET)wParam);
        if (it == self->m_pendingEvents.end())
            return 0;
        AsyncEvent &e = it->second;
        MORDOR_LOG_TRACE(g_log) << hWnd << " WSAAsyncSelect {"
            << (SOCKET)wParam << ", " << WSAGETSELECTEVENT(lParam) << ", "
            << WSAGETSELECTERROR(lParam) << "}";
        Event event = (Event)WSAGETSELECTEVENT(lParam);
        if (e.m_events & event & FD_READ) {
            e.m_schedulerRead->schedule(e.m_fiberRead);
            e.m_schedulerRead = NULL;
            e.m_fiberRead.reset();
        }
        if (e.m_events & event & FD_WRITE) {
            e.m_schedulerWrite->schedule(e.m_fiberWrite);
            e.m_schedulerWrite = NULL;
            e.m_fiberWrite.reset();
        }
        if (e.m_events & event & FD_ACCEPT) {
            e.m_schedulerRead->schedule(e.m_fiberRead);
            e.m_schedulerRead = NULL;
            e.m_fiberRead.reset();
        }
        if (e.m_events & event & FD_CONNECT) {
            e.m_schedulerWrite->schedule(e.m_fiberWrite);
            e.m_schedulerWrite = NULL;
            e.m_fiberWrite.reset();
        }
        e.m_events = (Event)(e.m_events & ~event);
        int result = WSAAsyncSelect(wParam, hWnd, g_socketIOMessage,
            (long)e.m_events);
        MORDOR_LOG_LEVEL(g_log, result ? Log::ERROR : Log::VERBOSE)
            << self << " WSAAsyncSelect(" << wParam << ", " << hWnd
            << ", " << g_socketIOMessage << ", " << e.m_events << "): "
            << result << " (" << GetLastError() << ")";
        if (result)
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WSAAsyncSelect");
        if (e.m_events == 0)
            self->m_pendingEvents.erase(it);
        return 0;
    }

    switch (uMsg) {
        case WM_TIMER:
        {
            MORDOR_LOG_TRACE(g_log) << hWnd << " processing timers";
            std::vector<boost::function<void ()> > expired = self->processTimers();
            if (!expired.empty())
                self->schedule(expired.begin(), expired.end());

            unsigned long long nextTimer = self->nextTimer();
            if (nextTimer != ~0ull) {
                UINT uElapse = (UINT)((nextTimer / 1000) + 1);
                uElapse = std::max<UINT>(USER_TIMER_MINIMUM, uElapse);
                uElapse = std::min<UINT>(USER_TIMER_MAXIMUM, uElapse);
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
