// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include "iomanager_iocp.h"

#include <boost/bind.hpp>

#include "assert.h"
#include "exception.h"
#include "runtime_linking.h"

AsyncEventIOCP::AsyncEventIOCP()
{
    memset(this, 0, sizeof(AsyncEventIOCP));
}

IOManagerIOCP::WaitBlock::WaitBlock(IOManagerIOCP &outer)
: m_outer(outer),
  m_inUseCount(0)
{
    m_handles[0] = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (!m_handles[0])
        throwExceptionFromLastError("CreateEventW");
}

IOManagerIOCP::WaitBlock::~WaitBlock()
{
    ASSERT(m_inUseCount <= 0);
    CloseHandle(m_handles[0]);
}

bool
IOManagerIOCP::WaitBlock::registerEvent(HANDLE hEvent)
{
    boost::mutex::scoped_lock lock(m_mutex);
    if (m_inUseCount == -1 || m_inUseCount == MAXIMUM_WAIT_OBJECTS)
        return false;
    ++m_inUseCount;
    m_handles[m_inUseCount] = hEvent;
    m_schedulers[m_inUseCount] = Scheduler::getThis();
    m_fibers[m_inUseCount] = Fiber::getThis();
    if (m_inUseCount == 1) {
        boost::thread thread(boost::bind(&WaitBlock::run, this));
    } else {
        SetEvent(m_handles[0]);
    }
    return true;
}

bool
IOManagerIOCP::WaitBlock::unregisterEvent(HANDLE handle)
{
    boost::mutex::scoped_lock lock(m_mutex);
    HANDLE *srcHandle = std::find(m_handles + 1, m_handles + m_inUseCount + 1, handle);
    if (srcHandle != m_handles + m_inUseCount + 1) {
        int index = (int)(srcHandle - m_handles);
        memmove(&m_schedulers[index], &m_schedulers[index + 1], (m_inUseCount - index) * sizeof(Scheduler *));
        // Manually destruct old object, move others down, and default construct unused one
        m_fibers[index].~shared_ptr<Fiber>();
        memmove(&m_fibers[index], &m_fibers[index + 1], (m_inUseCount - index) * sizeof(Fiber::ptr));
        new(&m_fibers[m_inUseCount]) Fiber::ptr();

        if (--m_inUseCount == 0)
            --m_inUseCount;
        SetEvent(m_handles[0]);
        return true;
    }
    return false;
}

void
IOManagerIOCP::WaitBlock::run()
{
    DWORD dwRet;
    DWORD count;
    HANDLE handles[MAXIMUM_WAIT_OBJECTS];

    {
        boost::mutex::scoped_lock lock(m_mutex);
        count = m_inUseCount + 1;
        memcpy(handles, m_handles, (count) * sizeof(HANDLE));        
    }

    while (true) {
        dwRet = WaitForMultipleObjects(count, handles, FALSE, INFINITE);
        if (dwRet == WAIT_OBJECT_0) {
            // Array just got reconfigured
            boost::mutex::scoped_lock lock(m_mutex);
            if (m_inUseCount == -1)
                break;
            count = m_inUseCount + 1;
            memcpy(handles, m_handles, (count) * sizeof(HANDLE));
        } else if (dwRet >= WAIT_OBJECT_0 + 1 && dwRet < WAIT_OBJECT_0 + MAXIMUM_WAIT_OBJECTS) {
            boost::mutex::scoped_lock lock(m_mutex);

            HANDLE handle = handles[dwRet - WAIT_OBJECT_0];
            HANDLE *srcHandle = std::find(m_handles + 1, m_handles + m_inUseCount + 1, handle);
            if (srcHandle != m_handles + m_inUseCount + 1) {
                int index = (int)(srcHandle - m_handles);
                m_schedulers[index]->schedule(m_fibers[index]);
                memmove(&m_schedulers[index], &m_schedulers[index + 1], (m_inUseCount - index) * sizeof(Scheduler *));
                // Manually destruct old object, move others down, and default construct unused one
                m_fibers[index].~shared_ptr<Fiber>();
                memmove(&m_fibers[index], &m_fibers[index + 1], (m_inUseCount - index) * sizeof(Fiber::ptr));
                new(&m_fibers[m_inUseCount]) Fiber::ptr();

                if (--m_inUseCount == 0) {
                    --m_inUseCount;
                    break;
                }
                count = m_inUseCount + 1;
                memcpy(handles, m_handles, (count) * sizeof(HANDLE));
            }
        } else if (dwRet == WAIT_FAILED) {
            // What to do, what to do?  Probably a bad handle.
            // This will bring down the whole process
            throwExceptionFromLastError("WaitForMultipleObjects");
        } else {
            NOTREACHED();
        }
    }
    {
        ptr self = shared_from_this();
        boost::mutex::scoped_lock lock(m_outer.m_mutex);
        std::list<WaitBlock::ptr>::iterator it =
            std::find(m_outer.m_waitBlocks.begin(), m_outer.m_waitBlocks.end(),
                shared_from_this());
        ASSERT(it != m_outer.m_waitBlocks.end());
        m_outer.m_waitBlocks.erase(it);
        m_outer.tickle();
    }
}

IOManagerIOCP::IOManagerIOCP(int threads, bool useCaller)
    : Scheduler(threads, useCaller)
{
    m_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (!m_hCompletionPort) {
        throwExceptionFromLastError("CreateIoCompletionPort");
    }
}

void
IOManagerIOCP::registerFile(HANDLE handle)
{
    HANDLE hRet = CreateIoCompletionPort(handle, m_hCompletionPort, 0, 0);
    if (hRet != m_hCompletionPort) {
        throwExceptionFromLastError("CreateIoCompletionPort");
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
IOManagerIOCP::registerEvent(HANDLE handle)
{
    ASSERT(handle);
    ASSERT(Scheduler::getThis());
    ASSERT(Fiber::getThis());

    boost::mutex::scoped_lock lock(m_mutex);
    for (std::list<WaitBlock::ptr>::iterator it = m_waitBlocks.begin();
        it != m_waitBlocks.end();
        ++it) {
        if ((*it)->registerEvent(handle))
            return;
    }
    m_waitBlocks.push_back(WaitBlock::ptr(new WaitBlock(*this)));
    bool result = m_waitBlocks.back()->registerEvent(handle);
    ASSERT(result);
}

bool
IOManagerIOCP::unregisterEvent(HANDLE handle)
{
    ASSERT(handle);
    for (std::list<WaitBlock::ptr>::iterator it = m_waitBlocks.begin();
        it != m_waitBlocks.end();
        ++it) {
        if ((*it)->unregisterEvent(handle))
            return true;
    }
    return false;
}

static void CancelIoShim(HANDLE hFile)
{
    CancelIo(hFile);
}

void
IOManagerIOCP::cancelEvent(HANDLE hFile, AsyncEventIOCP *e)
{
    if (!pCancelIoEx(hFile, &e->overlapped) &&
        GetLastError() == ERROR_CALL_NOT_IMPLEMENTED) {
        if (e->m_thread == boost::this_thread::get_id()) {
            CancelIo(hFile);
        } else {
            // Have to marshal to the original thread
            e->m_scheduler->schedule(boost::bind(&CancelIoShim, hFile),
                e->m_thread);
        }
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
            if (m_pendingEvents.empty() && m_waitBlocks.empty()) {
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
            throwExceptionFromLastError("GetQueuedCompletionStatus");
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
        throwExceptionFromLastError("PostQueuedCompletionStatus");
    }
}
