// Copyright (c) 2009 - Decho Corp.

#include <cassert>

#include "iomanager_iocp.h"

IOManagerIOCP::IOManagerIOCP(int threads, bool useCaller)
    : Scheduler(threads, useCaller)
{
    m_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (!m_hCompletionPort) {
        // throwExceptionFromLastError();
    }
}

void
IOManagerIOCP::registerFile(HANDLE handle)
{
    HANDLE hRet = CreateIoCompletionPort(handle, m_hCompletionPort, 0, 0);
    if (hRet != m_hCompletionPort) {
        // throwExceptionFromLastError();
    }
}

void
IOManagerIOCP::registerEvent(AsyncEventIOCP *e)
{
    assert(e);
    assert(Scheduler::getThis());
    assert(Fiber::getThis());
    e->m_scheduler = Scheduler::getThis();
    e->m_fiber = Fiber::getThis();
    {
        boost::mutex::scoped_lock lock(m_mutex);
        assert(m_pendingEvents.find(&e->overlapped) == m_pendingEvents.end());
        m_pendingEvents[&e->overlapped] = e;
    }
}

void
IOManagerIOCP::idle()
{
    DWORD numberOfBytes;
    ULONG_PTR completionKey;
    OVERLAPPED *overlapped;
    while (true) {
        if (stopping()) {
            boost::mutex::scoped_lock lock(m_mutex);
            if (m_pendingEvents.empty()) {
                return;
            }
        }
        BOOL ret = GetQueuedCompletionStatus(m_hCompletionPort,
            &numberOfBytes, &completionKey, &overlapped, INFINITE);
        if (ret && completionKey == ~0) {
            Fiber::yield();
            continue;
        }
        if (!ret && overlapped == NULL) {
            // throwExceptionFromLastError();
        }

        AsyncEventIOCP *e;
        {
            boost::mutex::scoped_lock lock(m_mutex);
            std::map<OVERLAPPED *, AsyncEventIOCP *>::iterator it =
                m_pendingEvents.find(overlapped);
            assert(it != m_pendingEvents.end());
            e = it->second;
            m_pendingEvents.erase(it);
        }

        e->ret = ret;
        e->numberOfBytes = numberOfBytes;
        e->completionKey = completionKey;
        e->lastError = GetLastError();
        e->m_scheduler->schedule(e->m_fiber);
        Fiber::yield();
    }
}

void
IOManagerIOCP::tickle()
{
    if (!PostQueuedCompletionStatus(m_hCompletionPort, 0, ~0, NULL)) {
        // throwExceptionFromLastError();
    }
}
