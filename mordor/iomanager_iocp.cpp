// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/pch.h"

#include "iomanager_iocp.h"

#include <boost/bind.hpp>

#include "assert.h"
#include "atomic.h"
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
    MORDOR_LOG_DEBUG(g_logWaitBlock) << this << " CreateEventW(): " << m_handles[0]
        << " (" << GetLastError() << ")";
    if (!m_handles[0])
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateEventW");
    m_reconfigured = CreateEventW(NULL, FALSE, FALSE, NULL);
    MORDOR_LOG_VERBOSE(g_logWaitBlock) << this << " CreateEventW(): "
        << m_reconfigured << " (" << GetLastError() << ")";
    if (!m_reconfigured) {
        CloseHandle(m_handles[0]);
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateEventW");
    }
}

IOManagerIOCP::WaitBlock::~WaitBlock()
{
    MORDOR_ASSERT(m_inUseCount <= 0);
    BOOL bRet = CloseHandle(m_handles[0]);
    MORDOR_LOG_DEBUG(g_logWaitBlock) << this << " CloseHandle("
        << m_handles[0] << "): " << bRet << " (" << GetLastError() << ")";
    bRet = CloseHandle(m_reconfigured);
    MORDOR_LOG_DEBUG(g_logWaitBlock) << this << " CloseHandle("
        << m_reconfigured << "): " << bRet << " (" << GetLastError() << ")";
}

bool
IOManagerIOCP::WaitBlock::registerEvent(HANDLE hEvent,
                                        boost::function <void ()> dg,
                                        bool recurring)
{
    boost::mutex::scoped_lock lock(m_mutex);
    if (m_inUseCount == -1 || m_inUseCount == MAXIMUM_WAIT_OBJECTS)
        return false;
    ++m_inUseCount;
    m_handles[m_inUseCount] = hEvent;
    m_schedulers[m_inUseCount] = Scheduler::getThis();
    m_fibers[m_inUseCount] = Fiber::getThis();
    m_dgs[m_inUseCount] = dg;
    m_recurring[m_inUseCount] = recurring;
    MORDOR_LOG_DEBUG(g_logWaitBlock) << this << " registerEvent(" << hEvent
        << ", " << dg << ")";
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
    MORDOR_LOG_DEBUG(g_logWaitBlock) << this << " unregisterEvent(" << handle
        << "): " << (srcHandle != m_handles + m_inUseCount + 1);
    if (srcHandle != m_handles + m_inUseCount + 1) {
        int index = (int)(srcHandle - m_handles);
        removeEntry(index);

        if (--m_inUseCount == 0)
            --m_inUseCount;
        if (!ResetEvent(m_reconfigured))
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("ResetEvent");
        if (!SetEvent(m_handles[0]))
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("SetEvent");
        lock.unlock();
        if (WaitForSingleObject(m_reconfigured, INFINITE) == WAIT_FAILED)
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WaitForSingleObject");
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
        if (m_inUseCount == -1) {
            // The first/final handle was unregistered out from under us
            // before we could even start
            if (!SetEvent(m_reconfigured))
                MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("SetEvent");
        }
        count = m_inUseCount + 1;
        memcpy(handles, m_handles, (count) * sizeof(HANDLE));        
    }

    MORDOR_LOG_DEBUG(g_logWaitBlock) << this << " run " << count;

    while (count) {
        dwRet = WaitForMultipleObjects(count, handles, FALSE, INFINITE);
        MORDOR_LOG_LEVEL(g_logWaitBlock, dwRet == WAIT_FAILED ? Log::ERROR : Log::DEBUG)
            << this << " WaitForMultipleObjects(" << count << ", " << handles
            << "): " << dwRet << " (" << GetLastError() << ")";
        if (dwRet == WAIT_OBJECT_0) {
            // Array just got reconfigured
            boost::mutex::scoped_lock lock(m_mutex);
            if (!SetEvent(m_reconfigured))
                MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("SetEvent");
            if (m_inUseCount == -1)
                break;
            count = m_inUseCount + 1;
            memcpy(handles, m_handles, (count) * sizeof(HANDLE));
            MORDOR_LOG_DEBUG(g_logWaitBlock) << this << " reconfigure " << count;
        } else if (dwRet >= WAIT_OBJECT_0 + 1 && dwRet < WAIT_OBJECT_0 + MAXIMUM_WAIT_OBJECTS) {
            boost::mutex::scoped_lock lock(m_mutex);

            if (m_inUseCount == -1) {
                // The final handle was unregistered out from under us
                if (!SetEvent(m_reconfigured))
                    MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("SetEvent");
                break;
            }

            HANDLE handle = handles[dwRet - WAIT_OBJECT_0];
            HANDLE *srcHandle = std::find(m_handles + 1, m_handles + m_inUseCount + 1, handle);
            MORDOR_LOG_DEBUG(g_logWaitBlock) << this << " Event " << handle << " "
                << (srcHandle != m_handles + m_inUseCount + 1);
            if (srcHandle != m_handles + m_inUseCount + 1) {
                int index = (int)(srcHandle - m_handles);
                if (!m_dgs[index])
                    m_schedulers[index]->schedule(m_fibers[index]);
                else
                    m_schedulers[index]->schedule(m_dgs[index]);
                if (!m_recurring[index]) {
                    removeEntry(index);

                    if (--m_inUseCount == 0) {
                        --m_inUseCount;
                        break;
                    }
                    count = m_inUseCount + 1;
                    memcpy(handles, m_handles, (count) * sizeof(HANDLE));
                }
            }
        } else if (dwRet == WAIT_FAILED) {
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WaitForMultipleObjects");
        } else {
            MORDOR_NOTREACHED();
        }
    }
    MORDOR_LOG_DEBUG(g_logWaitBlock) << this << " done";
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

void
IOManagerIOCP::WaitBlock::removeEntry(int index)
{
    memmove(&m_handles[index], &m_handles[index + 1], (m_inUseCount - index) * sizeof(HANDLE));
    memmove(&m_schedulers[index], &m_schedulers[index + 1], (m_inUseCount - index) * sizeof(Scheduler *));
    // Manually destruct old object, move others down, and default construct unused one
    m_dgs[index].~functor();
    memmove(&m_dgs[index], &m_dgs[index + 1], (m_inUseCount - index) * sizeof(boost::function<void ()>));
    new(&m_dgs[m_inUseCount]) boost::function<void ()>();
    // Manually destruct old object, move others down, and default construct unused one
    m_fibers[index].~shared_ptr<Fiber>();
    memmove(&m_fibers[index], &m_fibers[index + 1], (m_inUseCount - index) * sizeof(Fiber::ptr));
    new(&m_fibers[m_inUseCount]) Fiber::ptr();
    memmove(&m_recurring[index], &m_recurring[index + 1], (m_inUseCount - index) * sizeof(bool));
}

IOManagerIOCP::IOManagerIOCP(int threads, bool useCaller)
    : Scheduler(threads, useCaller)
{
    m_pendingEventCount = 0;
    m_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    MORDOR_LOG_LEVEL(g_log, m_hCompletionPort ? Log::VERBOSE : Log::ERROR) << this <<
        " CreateIoCompletionPort(): " << m_hCompletionPort << " ("
        << GetLastError() << ")";
    if (!m_hCompletionPort)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateIoCompletionPort");
    try {
        if (threads - (useCaller ? 1 : 0)) start();
    } catch (...) {
        CloseHandle(m_hCompletionPort);
        throw;
    }
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
    MORDOR_LOG_LEVEL(g_log, m_hCompletionPort ? Log::DEBUG : Log::ERROR) << this <<
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
    e->m_scheduler = Scheduler::getThis();
    e->m_thread = boost::this_thread::get_id();
    e->m_fiber = Fiber::getThis();
    MORDOR_LOG_DEBUG(g_log) << this << " registerEvent(" << &e->overlapped << ")";
    atomicIncrement(m_pendingEventCount);
#ifdef DEBUG
    {
        boost::mutex::scoped_lock lock(m_mutex);
        MORDOR_ASSERT(m_pendingEvents.find(&e->overlapped) == m_pendingEvents.end());
        m_pendingEvents[&e->overlapped] = e;
    }
#endif
}

void
IOManagerIOCP::unregisterEvent(AsyncEventIOCP *e)
{
    MORDOR_ASSERT(e);
    MORDOR_LOG_DEBUG(g_log) << this << " unregisterEvent(" << &e->overlapped << ")";
    atomicDecrement(m_pendingEventCount);
#ifdef DEBUG
    {
        boost::mutex::scoped_lock lock(m_mutex);
        std::map<OVERLAPPED *, AsyncEventIOCP *>::iterator it =
            m_pendingEvents.find(&e->overlapped);
        MORDOR_ASSERT(it != m_pendingEvents.end());
        m_pendingEvents.erase(it);
    }
#endif
}

void
IOManagerIOCP::registerEvent(HANDLE handle, boost::function<void ()> dg, bool recurring)
{
    MORDOR_LOG_DEBUG(g_log) << this << " registerEvent(" << handle << ", " << dg
        << ")";
    MORDOR_ASSERT(handle);
    MORDOR_ASSERT(Scheduler::getThis());

    boost::mutex::scoped_lock lock(m_mutex);
    for (std::list<WaitBlock::ptr>::iterator it = m_waitBlocks.begin();
        it != m_waitBlocks.end();
        ++it) {
        if ((*it)->registerEvent(handle, dg, recurring))
            return;
    }
    m_waitBlocks.push_back(WaitBlock::ptr(new WaitBlock(*this)));
    bool result = m_waitBlocks.back()->registerEvent(handle, dg, recurring);
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
            MORDOR_LOG_DEBUG(g_log) << this << " unregisterEvent(" << handle
                << "): 1";
            return true;
        }
    }
    MORDOR_LOG_DEBUG(g_log) << this << " unregisterEvent(" << handle << "): 0";
    return false;
}

void
IOManagerIOCP::cancelEvent(HANDLE hFile, AsyncEventIOCP *e)
{
    MORDOR_ASSERT(hFile);
    MORDOR_ASSERT(e);
    MORDOR_LOG_DEBUG(g_log) << this << " cancelEvent(" << hFile << ", "
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
        if (m_pendingEventCount != 0)
            return false;
        boost::mutex::scoped_lock lock(m_mutex);
        return m_waitBlocks.empty();
    }
    return false;
}

void
IOManagerIOCP::idle()
{
    OVERLAPPED_ENTRY events[64];
    ULONG count;
    while (true) {
        unsigned long long nextTimeout;
        if (stopping(nextTimeout))
            return;
        DWORD timeout = INFINITE;
        if (nextTimeout != ~0ull)
            timeout = (DWORD)(nextTimeout / 1000);
        count = 0;
        BOOL ret = pGetQueuedCompletionStatusEx(m_hCompletionPort,
            events,
            64,
            &count,
            timeout,
            FALSE);
        DWORD lastError = GetLastError();
        MORDOR_LOG_DEBUG(g_log) << this << " GetQueuedCompletionStatusEx("
            << m_hCompletionPort << ", " << timeout << "): " << ret << ", ("
            << count << ") (" << lastError << ")";
        if (!ret && lastError) {
            if (lastError == WAIT_TIMEOUT) {
                std::vector<boost::function<void ()> > expired = processTimers();
                if (!expired.empty()) {
                    schedule(expired.begin(), expired.end());
                    Fiber::yield();
                }
                continue;
            }
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("GetQueuedCompletionStatusEx");
        }
        std::vector<boost::function<void ()> > expired = processTimers();
        schedule(expired.begin(), expired.end());

#ifdef DEBUG
        boost::mutex::scoped_lock lock(m_mutex, boost::defer_lock_t());
#endif
        int tickles = 0;
        for (ULONG i = 0; i < count; ++i) {
            if (events[i].lpCompletionKey == ~0) {
                ++tickles;
                continue;
            }
            AsyncEventIOCP *e = (AsyncEventIOCP *)events[i].lpOverlapped;
#ifdef DEBUG
            if (!lock.owns_lock())
                lock.lock();

            std::map<OVERLAPPED *, AsyncEventIOCP *>::iterator it =
                m_pendingEvents.find(events[i].lpOverlapped);
            MORDOR_ASSERT(it != m_pendingEvents.end());
            MORDOR_ASSERT(e == it->second);
            m_pendingEvents.erase(it);
#endif

            e->m_scheduler->schedule(e->m_fiber);
            e->m_fiber.reset();
        }
#ifdef DEBUG
        if (lock.owns_lock())
            lock.unlock();
#endif
        atomicAdd(m_pendingEventCount, (size_t)(-(ptrdiff_t)(count - tickles)));
        // We could have possibly retrieved more tickles than we needed;
        // retickle so other threads will get them
        while (--tickles > 0) tickle();
        Fiber::yield();
    }
}

void
IOManagerIOCP::tickle()
{
    BOOL bRet = PostQueuedCompletionStatus(m_hCompletionPort, 0, ~0, NULL);
    MORDOR_LOG_LEVEL(g_log, bRet ? Log::DEBUG : Log::ERROR) << this
        << " PostQueuedCompletionStatus(" << m_hCompletionPort
        << ", 0, ~0, NULL): " << bRet << " (" << GetLastError() << ")";
    if (!bRet)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("PostQueuedCompletionStatus");
}

}
