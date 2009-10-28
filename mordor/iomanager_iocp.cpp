// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include "iomanager_iocp.h"

#include <boost/bind.hpp>

#include "assert.h"
#include "exception.h"
#include "log.h"
#include "runtime_linking.h"

namespace Mordor {

static Logger::ptr g_log = Log::lookup("mordor:iomanager");
static Logger::ptr g_logWaitBlock = Log::lookup("mordor:iomanager:waitblock");

AsyncEventIOCP::AsyncEventIOCP()
{
    memset(this, 0, sizeof(AsyncEventIOCP));
}

IOManagerIOCP::WaitBlock::WaitBlock(IOManagerIOCP &outer)
: m_outer(outer),
  m_inUseCount(0)
{
    m_handles[0] = CreateEventW(NULL, FALSE, FALSE, NULL);
    MORDOR_LOG_VERBOSE(g_logWaitBlock) << this << " CreateEventW(): " << m_handles[0]
        << " (" << GetLastError() << ")";
    if (!m_handles[0])
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateEventW");
}

IOManagerIOCP::WaitBlock::~WaitBlock()
{
    MORDOR_ASSERT(m_inUseCount <= 0);
    BOOL bRet = CloseHandle(m_handles[0]);
    MORDOR_LOG_VERBOSE(g_logWaitBlock) << this << " CloseHandle(" << m_handles[0]
        << "): " << bRet << " (" << GetLastError() << ")";
}

bool
IOManagerIOCP::WaitBlock::registerEvent(HANDLE hEvent,
                                        boost::function <void ()> dg)
{
    boost::mutex::scoped_lock lock(m_mutex);
    if (m_inUseCount == -1 || m_inUseCount == MAXIMUM_WAIT_OBJECTS)
        return false;
    ++m_inUseCount;
    m_handles[m_inUseCount] = hEvent;
    m_schedulers[m_inUseCount] = Scheduler::getThis();
    m_fibers[m_inUseCount] = Fiber::getThis();
    m_dgs[m_inUseCount] = dg;
    MORDOR_LOG_VERBOSE(g_logWaitBlock) << this << " registerEvent(" << hEvent << ", "
        << dg << ")";
    if (m_inUseCount == 1) {
        boost::thread thread(boost::bind(&WaitBlock::run, this));
    } else {
        if (!SetEvent(m_handles[0]))
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("SetEvent");
    }
    return true;
}

typedef boost::function<void ()> functor;
bool
IOManagerIOCP::WaitBlock::unregisterEvent(HANDLE handle)
{
    boost::mutex::scoped_lock lock(m_mutex);
    HANDLE *srcHandle = std::find(m_handles + 1, m_handles + m_inUseCount + 1, handle);
    MORDOR_LOG_VERBOSE(g_logWaitBlock) << this << " unregisterEvent(" << handle
        << "): " << (srcHandle != m_handles + m_inUseCount + 1);
    if (srcHandle != m_handles + m_inUseCount + 1) {
        int index = (int)(srcHandle - m_handles);
        memmove(&m_schedulers[index], &m_schedulers[index + 1], (m_inUseCount - index) * sizeof(Scheduler *));
        // Manually destruct old object, move others down, and default construct unused one
        m_dgs[index].~functor();
        memmove(&m_dgs[index], &m_dgs[index + 1], (m_inUseCount - index) * sizeof(boost::function<void ()>));
        new(&m_dgs[m_inUseCount]) boost::function<void ()>();
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

    MORDOR_LOG_VERBOSE(g_logWaitBlock) << this << " run " << count;

    while (true) {
        dwRet = WaitForMultipleObjects(count, handles, FALSE, INFINITE);
        MORDOR_LOG_LEVEL(g_log, dwRet == WAIT_FAILED ? Log::ERROR : Log::VERBOSE)
            << this << " WaitForMultipleObjects(" << count << ", " << handles
            << "): " << dwRet << " (" << GetLastError() << ")";
        if (dwRet == WAIT_OBJECT_0) {
            // Array just got reconfigured
            boost::mutex::scoped_lock lock(m_mutex);
            if (m_inUseCount == -1)
                break;
            count = m_inUseCount + 1;
            memcpy(handles, m_handles, (count) * sizeof(HANDLE));
            MORDOR_LOG_VERBOSE(g_logWaitBlock) << this << " reconfigure " << count;
        } else if (dwRet >= WAIT_OBJECT_0 + 1 && dwRet < WAIT_OBJECT_0 + MAXIMUM_WAIT_OBJECTS) {
            boost::mutex::scoped_lock lock(m_mutex);

            HANDLE handle = handles[dwRet - WAIT_OBJECT_0];
            HANDLE *srcHandle = std::find(m_handles + 1, m_handles + m_inUseCount + 1, handle);
            MORDOR_LOG_VERBOSE(g_log) << this << " event " << handle << " "
                << (srcHandle != m_handles + m_inUseCount + 1);
            if (srcHandle != m_handles + m_inUseCount + 1) {
                int index = (int)(srcHandle - m_handles);
                if (!m_dgs[index])
                    m_schedulers[index]->schedule(m_fibers[index]);
                else
                    m_schedulers[index]->schedule(m_dgs[index]);
                memmove(&m_schedulers[index], &m_schedulers[index + 1], (m_inUseCount - index) * sizeof(Scheduler *));
                // Manually destruct old object, move others down, and default construct unused one
                m_dgs[index].~functor();
                memmove(&m_dgs[index], &m_dgs[index + 1], (m_inUseCount - index) * sizeof(boost::function<void ()>));
                new(&m_dgs[m_inUseCount]) boost::function<void ()>();
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
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WaitForMultipleObjects");
        } else {
            MORDOR_NOTREACHED();
        }
    }
    MORDOR_LOG_VERBOSE(g_logWaitBlock) << this << " done";
    {
        ptr self = shared_from_this();
        boost::mutex::scoped_lock lock(m_outer.m_mutex);
        std::list<WaitBlock::ptr>::iterator it =
            std::find(m_outer.m_waitBlocks.begin(), m_outer.m_waitBlocks.end(),
                shared_from_this());
        MORDOR_ASSERT(it != m_outer.m_waitBlocks.end());
        m_outer.m_waitBlocks.erase(it);
        m_outer.tickle();
    }
}

IOManagerIOCP::IOManagerIOCP(int threads, bool useCaller)
    : Scheduler(threads, useCaller)
{
    m_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    MORDOR_LOG_LEVEL(g_log, m_hCompletionPort ? Log::TRACE : Log::ERROR) << this <<
        " CreateIoCompletionPort(): " << m_hCompletionPort << " ("
        << GetLastError() << ")";
    if (!m_hCompletionPort)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateIoCompletionPort");
}

IOManagerIOCP::~IOManagerIOCP()
{
    stop();
    CloseHandle(m_hCompletionPort);
}

bool
IOManagerIOCP::stopping()
{
    unsigned long long timeout;
    return stopping(timeout);
}

void
IOManagerIOCP::registerFile(HANDLE handle)
{
    HANDLE hRet = CreateIoCompletionPort(handle, m_hCompletionPort, 0, 0);
    MORDOR_LOG_LEVEL(g_log, m_hCompletionPort ? Log::VERBOSE : Log::ERROR) << this <<
        " CreateIoCompletionPort(" << handle << ", " << m_hCompletionPort
        << "): " << hRet << " (" << GetLastError() << ")";
    if (hRet != m_hCompletionPort) {
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateIoCompletionPort");
    }
}

void
IOManagerIOCP::registerEvent(AsyncEventIOCP *e)
{
    MORDOR_ASSERT(e);
    MORDOR_ASSERT(Scheduler::getThis());
    MORDOR_ASSERT(Fiber::getThis());
    e->m_scheduler = Scheduler::getThis();
    e->m_thread = boost::this_thread::get_id();
    e->m_fiber = Fiber::getThis();
    MORDOR_LOG_VERBOSE(g_log) << this << " registerEvent(" << &e->overlapped << ")";
    {
        boost::mutex::scoped_lock lock(m_mutex);
        MORDOR_ASSERT(m_pendingEvents.find(&e->overlapped) == m_pendingEvents.end());
        m_pendingEvents[&e->overlapped] = e;
    }
}

void
IOManagerIOCP::unregisterEvent(AsyncEventIOCP *e)
{
    MORDOR_ASSERT(e);
    MORDOR_LOG_VERBOSE(g_log) << this << " unregisterEvent(" << &e->overlapped << ")";
    {
        boost::mutex::scoped_lock lock(m_mutex);
        std::map<OVERLAPPED *, AsyncEventIOCP *>::iterator it =
            m_pendingEvents.find(&e->overlapped);
        MORDOR_ASSERT(it != m_pendingEvents.end());
        m_pendingEvents.erase(it);
    }
}

void
IOManagerIOCP::registerEvent(HANDLE handle, boost::function<void ()> dg)
{
    MORDOR_LOG_VERBOSE(g_log) << this << " registerEvent(" << handle << ", " << dg
        << ")";
    MORDOR_ASSERT(handle);
    if (!dg) {
        MORDOR_ASSERT(Scheduler::getThis());
        MORDOR_ASSERT(Fiber::getThis());
    }

    boost::mutex::scoped_lock lock(m_mutex);
    for (std::list<WaitBlock::ptr>::iterator it = m_waitBlocks.begin();
        it != m_waitBlocks.end();
        ++it) {
        if ((*it)->registerEvent(handle, dg))
            return;
    }
    m_waitBlocks.push_back(WaitBlock::ptr(new WaitBlock(*this)));
    bool result = m_waitBlocks.back()->registerEvent(handle, dg);
    MORDOR_ASSERT(result);
}

bool
IOManagerIOCP::unregisterEvent(HANDLE handle)
{
    MORDOR_ASSERT(handle);
    boost::mutex::scoped_lock lock(m_mutex);
    for (std::list<WaitBlock::ptr>::iterator it = m_waitBlocks.begin();
        it != m_waitBlocks.end();
        ++it) {
        if ((*it)->unregisterEvent(handle)) {
            MORDOR_LOG_VERBOSE(g_log) << this << " unregisterEvent(" << handle
                << "): 1";
            return true;
        }
    }
    MORDOR_LOG_VERBOSE(g_log) << this << " unregisterEvent(" << handle << "): 0";
    return false;
}

void
IOManagerIOCP::cancelEvent(HANDLE hFile, AsyncEventIOCP *e)
{
    MORDOR_ASSERT(hFile);
    MORDOR_ASSERT(e);
    MORDOR_LOG_VERBOSE(g_log) << this << " cancelEvent(" << hFile << ", "
        << &e->overlapped << ")";

    if (!pCancelIoEx(hFile, &e->overlapped)) {
        if (GetLastError() == ERROR_CALL_NOT_IMPLEMENTED) {
            if (e->m_thread == boost::this_thread::get_id()) {
                if (!CancelIo(hFile))
                    MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CancelIo");
            } else {
                MORDOR_ASSERT(e->m_scheduler);
                // Have to marshal to the original thread
                SchedulerSwitcher switcher;
                e->m_scheduler->switchTo(e->m_thread);
                if (!CancelIo(hFile))
                    MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CancelIo");
            }
        } else if (GetLastError() == ERROR_NOT_FOUND) {
            // Nothing to cancel
        } else {
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CancelIoEx");
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

bool
IOManagerIOCP::stopping(unsigned long long &nextTimeout)
{
    nextTimeout = nextTimer();
    if (nextTimeout == ~0ull && Scheduler::stopping()) {
        boost::mutex::scoped_lock lock(m_mutex);
        if (m_pendingEvents.empty() && m_waitBlocks.empty()) {
            return true;
        }
    }
    return false;
}

void
IOManagerIOCP::idle()
{
    DWORD numberOfBytes;
    ULONG_PTR completionKey;
    OVERLAPPED *overlapped;
    while (true) {
        unsigned long long nextTimeout;
        if (stopping(nextTimeout))
            return;
        DWORD timeout = INFINITE;
        if (nextTimeout != ~0ull)
            timeout = (DWORD)(nextTimeout / 1000);
        BOOL ret = GetQueuedCompletionStatus(m_hCompletionPort,
            &numberOfBytes, &completionKey, &overlapped, timeout);
        MORDOR_LOG_VERBOSE(g_log) << this << " GetQueuedCompletionStatus("
            << m_hCompletionPort << ", " << timeout << "): " << ret << ", ("
            << numberOfBytes << ", " << completionKey << ", " << overlapped
            << ") (" << GetLastError() << ")";
        if (ret && completionKey == ~0) {
            Fiber::yield();
            continue;
        }
        if (!ret && overlapped == NULL) {
            if (GetLastError() == WAIT_TIMEOUT) {
                processTimers();
                continue;
            }
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("GetQueuedCompletionStatus");
        }
        processTimers();

        AsyncEventIOCP *e;
        {
            boost::mutex::scoped_lock lock(m_mutex);
            std::map<OVERLAPPED *, AsyncEventIOCP *>::iterator it =
                m_pendingEvents.find(overlapped);
            MORDOR_ASSERT(it != m_pendingEvents.end());
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
    BOOL bRet = PostQueuedCompletionStatus(m_hCompletionPort, 0, ~0, NULL);
    MORDOR_LOG_LEVEL(g_log, bRet ? Log::VERBOSE : Log::ERROR) << this
        << " PostQueuedCompletionStatus(" << m_hCompletionPort
        << ", 0, ~0, NULL): " << bRet << " (" << GetLastError() << ")";
    if (!bRet)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("PostQueuedCompletionStatus");
}

}