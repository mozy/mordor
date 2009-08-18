// Copyright (c) 2009 - Decho Corp.

#include "iomanager_iocp.h"

#include <boost/bind.hpp>

#include "assert.h"
#include "exception.h"

AsyncEventIOCP::AsyncEventIOCP()
{
    memset(this, 0, sizeof(AsyncEventIOCP));
}

IOManagerIOCP::IOManagerIOCP(int threads, bool useCaller)
    : Scheduler(threads, useCaller)
{
    m_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (!m_hCompletionPort) {
        throwExceptionFromLastError();
    }
}

void
IOManagerIOCP::registerFile(HANDLE handle)
{
    HANDLE hRet = CreateIoCompletionPort(handle, m_hCompletionPort, 0, 0);
    if (hRet != m_hCompletionPort) {
        throwExceptionFromLastError();
    }
}



void
IOManagerIOCP::unregisterEvent(AsyncEventIOCP *e)
{
    ASSERT(e);
    {
        boost::mutex::scoped_lock lock(m_mutex);
        std::map<OVERLAPPED *, AsyncEventIOCP *>::iterator it =
            m_pendingEvents.find(&e->overlapped);
        ASSERT(it != m_pendingEvents.end());
        m_pendingEvents.erase(it);
    }
}

void
IOManagerIOCP::registerEvent(AsyncEventIOCP *e)
{
    ASSERT(e);
    ASSERT(Scheduler::getThis());
    ASSERT(Fiber::getThis());
    e->m_scheduler = Scheduler::getThis();
    e->m_thread = boost::this_thread::get_id();
    e->m_fiber = Fiber::getThis();
    {
        boost::mutex::scoped_lock lock(m_mutex);
        ASSERT(m_pendingEvents.find(&e->overlapped) == m_pendingEvents.end());
        m_pendingEvents[&e->overlapped] = e;
    }
}

static void CancelIoShim(HANDLE hFile)
{
    CancelIo(hFile);
}

void
IOManagerIOCP::cancelEvent(HANDLE hFile, AsyncEventIOCP *e)
{
    // TODO: Use CancelIoEx if available
    if (e->m_thread == boost::this_thread::get_id()) {
        CancelIo(hFile);
    } else {
        // Have to marshal to the original thread
        e->m_scheduler->schedule(boost::bind(&CancelIoShim, hFile), e->m_thread);
    }
}

Timer::ptr
IOManagerIOCP::registerTimer(unsigned long long us, boost::function<void ()> dg,
        bool recurring)
{
    bool atFront;
    Timer::ptr result = TimerManager::registerTimer(us, dg, recurring, atFront);
    if (atFront)
        tickle();
    return result;
}

void
IOManagerIOCP::idle()
{
    DWORD numberOfBytes;
    ULONG_PTR completionKey;
    OVERLAPPED *overlapped;
    while (true) {
        unsigned long long nextTimeout = nextTimer();
        if (nextTimeout == ~0ull && stopping()) {
            boost::mutex::scoped_lock lock(m_mutex);
            if (m_pendingEvents.empty()) {
                return;
            }
        }
        DWORD timeout = INFINITE;
        if (nextTimeout != ~0ull)
            timeout = (DWORD)(nextTimeout / 1000);
        BOOL ret = GetQueuedCompletionStatus(m_hCompletionPort,
            &numberOfBytes, &completionKey, &overlapped, timeout);
        if (ret && completionKey == ~0) {
            Fiber::yield();
            continue;
        }
        if (!ret && overlapped == NULL) {
            if (GetLastError() == WAIT_TIMEOUT) {
                processTimers();
                continue;
            }
            throwExceptionFromLastError();
        }
        processTimers();

        AsyncEventIOCP *e;
        {
            boost::mutex::scoped_lock lock(m_mutex);
            std::map<OVERLAPPED *, AsyncEventIOCP *>::iterator it =
                m_pendingEvents.find(overlapped);
            ASSERT(it != m_pendingEvents.end());
            e = it->second;
            m_pendingEvents.erase(it);
        }

        e->ret = ret;
        e->numberOfBytes = numberOfBytes;
        e->completionKey = completionKey;
        e->lastError = GetLastError();
        e->m_scheduler->schedule(e->m_fiber);
        e->m_fiber.reset();
        Fiber::yield();
    }
}

void
IOManagerIOCP::tickle()
{
    if (!PostQueuedCompletionStatus(m_hCompletionPort, 0, ~0, NULL)) {
        throwExceptionFromLastError();
    }
}
