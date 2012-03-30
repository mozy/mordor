// Copyright (c) 2009 - Mozy, Inc.

#include "pch.h"

#ifdef BSD

#include "iomanager_kqueue.h"

#include "assert.h"
#include "fiber.h"

namespace Mordor {

static Logger::ptr g_log = Log::lookup("mordor:iomanager");

IOManager::IOManager(size_t threads, bool useCaller)
    : Scheduler(threads, useCaller)
{
    m_kqfd = kqueue();
    MORDOR_LOG_LEVEL(g_log, m_kqfd <= 0 ? Log::ERROR : Log::TRACE) << this
        << " kqueue(): " << m_kqfd;
    if (m_kqfd <= 0) {
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("kqueue");
    }
    int rc = pipe(m_tickleFds);
    MORDOR_LOG_LEVEL(g_log, rc ? Log::ERROR : Log::VERBOSE) << this << " pipe(): "
        << rc << " (" << lastError() << ")";
    if (rc) {
        close(m_kqfd);
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("pipe");
    }
    MORDOR_ASSERT(m_tickleFds[0] > 0);
    MORDOR_ASSERT(m_tickleFds[1] > 0);
    struct kevent event;
    EV_SET(&event, m_tickleFds[0], EVFILT_READ, EV_ADD, 0, 0, NULL);
    rc = kevent(m_kqfd, &event, 1, NULL, 0, NULL);
    MORDOR_LOG_LEVEL(g_log, rc ? Log::ERROR : Log::VERBOSE) << this << " kevent("
        << m_kqfd << ", (" << m_tickleFds[0] << ", EVFILT_READ, EV_ADD)): " << rc
        << " (" << lastError() << ")";
    if (rc) {
        close(m_tickleFds[0]);
        close(m_tickleFds[1]);
        close(m_kqfd);
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("kevent");
    }
    try {
        start();
    } catch (...) {
        close(m_tickleFds[0]);
        close(m_tickleFds[1]);
        close(m_kqfd);
        throw;
    }    
}

IOManager::~IOManager()
{
    stop();
    close(m_kqfd);
    MORDOR_LOG_TRACE(g_log) << this << " close(" << m_kqfd << ")";
    close(m_tickleFds[0]);
    MORDOR_LOG_VERBOSE(g_log) << this << " close(" << m_tickleFds[0] << ")";
    close(m_tickleFds[1]);
}

bool
IOManager::stopping()
{
    unsigned long long timeout;
    return stopping(timeout);
}

void
IOManager::registerEvent(int fd, Event events,
                               boost::function<void ()> dg)
{
    MORDOR_ASSERT(fd > 0);
    MORDOR_ASSERT(Scheduler::getThis());
    MORDOR_ASSERT(Fiber::getThis());

    Event eventsKey = events;
    if (eventsKey == CLOSE)
        eventsKey = READ;
    boost::mutex::scoped_lock lock(m_mutex);
    AsyncEvent &e = m_pendingEvents[std::pair<int, Event>(fd, eventsKey)];
    memset(&e.event, 0, sizeof(struct kevent));
    e.event.ident = fd;
    e.event.flags = EV_ADD;
    switch (events) {
        case READ:
            e.event.filter = EVFILT_READ;
            break;
        case WRITE:
            e.event.filter = EVFILT_WRITE;
            break;
        case CLOSE:
            e.event.filter = EVFILT_READ;
            break;
        default:
            MORDOR_NOTREACHED();
    }
    if (events == READ || events == WRITE) {
        MORDOR_ASSERT(!e.m_dg && !e.m_fiber);
        e.m_dg = dg;
        if (!dg)
           e.m_fiber = Fiber::getThis();
        e.m_scheduler = Scheduler::getThis();
    } else {
        MORDOR_ASSERT(!e.m_dgClose && !e.m_fiberClose);
        e.m_dgClose = dg;
        if (!dg)
            e.m_fiberClose = Fiber::getThis();
        e.m_schedulerClose = Scheduler::getThis();
    }
    int rc = kevent(m_kqfd, &e.event, 1, NULL, 0, NULL);
    MORDOR_LOG_LEVEL(g_log, rc ? Log::ERROR : Log::VERBOSE) << this << " kevent("
        << m_kqfd << ", (" << fd << ", " << events << ", EV_ADD)): " << rc
        << " (" << lastError() << ")";
    if (rc)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("kevent");
}

void
IOManager::cancelEvent(int fd, Event events)
{
    Event eventsKey = events;
    if (eventsKey == CLOSE)
        eventsKey = READ;
    boost::mutex::scoped_lock lock(m_mutex);
    std::map<std::pair<int, Event>, AsyncEvent>::iterator it =
        m_pendingEvents.find(std::pair<int, Event>(fd, eventsKey));
    if (it == m_pendingEvents.end())
        return;
    AsyncEvent &e = it->second;
    MORDOR_ASSERT(e.event.ident == (unsigned)fd);
    Scheduler *scheduler;
    Fiber::ptr fiber;
    boost::function<void ()> dg;
    if (events == READ) {
        scheduler = e.m_scheduler;
        fiber.swap(e.m_fiber);
        dg.swap(e.m_dg);
        if (e.m_fiberClose || e.m_dgClose) {
            if (dg || fiber) {
                if (dg)
                    scheduler->schedule(dg);
                else
                    scheduler->schedule(fiber);
            }
            return;
        }
    } else if (events == CLOSE) {
        scheduler = e.m_schedulerClose;
        fiber.swap(e.m_fiberClose);
        dg.swap(e.m_dgClose);
        if (e.m_fiber || e.m_dg) {
            if (dg || fiber) {
                if (dg)
                    scheduler->schedule(dg);
                else
                    scheduler->schedule(fiber);
            }
            return;
        }
    } else if (events == WRITE) {
        scheduler = e.m_scheduler;
        fiber.swap(e.m_fiber);
        dg.swap(e.m_dg);
    } else {
        MORDOR_NOTREACHED();
    }
    e.event.flags = EV_DELETE;
    int rc = kevent(m_kqfd, &e.event, 1, NULL, 0, NULL);
    MORDOR_LOG_LEVEL(g_log, rc ? Log::ERROR : Log::VERBOSE) << this << " kevent("
        << m_kqfd << ", (" << fd << ", " << eventsKey << ", EV_DELETE)): " << rc
        << " (" << lastError() << ")";
    if (rc)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("kevent");
    if (dg)
        scheduler->schedule(dg);
    else
        scheduler->schedule(fiber);
    m_pendingEvents.erase(it);
}

void
IOManager::unregisterEvent(int fd, Event events)
{
    Event eventsKey = events;
    if (eventsKey == CLOSE)
        eventsKey = READ;
    boost::mutex::scoped_lock lock(m_mutex);
    std::map<std::pair<int, Event>, AsyncEvent>::iterator it =
        m_pendingEvents.find(std::pair<int, Event>(fd, eventsKey));
    if (it == m_pendingEvents.end())
        return;
    AsyncEvent &e = it->second;
    MORDOR_ASSERT(e.event.ident == (unsigned)fd);
    if (events == READ) {
        e.m_fiber.reset();
        e.m_dg = NULL;
        if (e.m_fiberClose || e.m_dgClose)
            return;
    } else if (events == CLOSE) {
        e.m_fiberClose.reset();
        e.m_dgClose = NULL;
        if (e.m_fiber || e.m_dg)
            return;
    }
    e.event.flags = EV_DELETE;
    int rc = kevent(m_kqfd, &e.event, 1, NULL, 0, NULL);
    MORDOR_LOG_LEVEL(g_log, rc ? Log::ERROR : Log::VERBOSE) << this << " kevent("
        << m_kqfd << ", (" << fd << ", " << eventsKey << ", EV_DELETE)): " << rc
        << " (" << lastError() << ")";
    if (rc)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("kevent");
    m_pendingEvents.erase(it);
}


bool
IOManager::stopping(unsigned long long &nextTimeout)
{
    nextTimeout = nextTimer();
    if (nextTimeout == ~0ull && Scheduler::stopping()) {
        boost::mutex::scoped_lock lock(m_mutex);
        if (m_pendingEvents.empty())
            return true;
    }
    return false;
}

void
IOManager::idle()
{
    struct kevent events[64];
    while (true) {
        unsigned long long nextTimeout;
        if (stopping(nextTimeout))
            return;
        int rc;
        do {
            struct timespec *timeout = NULL, timeoutStorage;
            if (nextTimeout != ~0ull) {
                timeout = &timeoutStorage;
                timeout->tv_sec = nextTimeout / 1000000;
                timeout->tv_nsec = (nextTimeout % 1000000) * 1000;
            }
            rc = kevent(m_kqfd, NULL, 0, events, 64, timeout);
            if (rc < 0 && errno == EINTR)
                nextTimeout = nextTimer();
            else
                break;
        } while (true);
        MORDOR_LOG_LEVEL(g_log, rc < 0 ? Log::ERROR : Log::VERBOSE) << this
            << " kevent(" << m_kqfd << "): " << rc << " (" << lastError()
            << ")";
        if (rc < 0)
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("kevent");
        std::vector<boost::function<void ()> > expired = processTimers();
        schedule(expired.begin(), expired.end());
        expired.clear();

        for(int i = 0; i < rc; ++i) {
            struct kevent &event = events[i];
            if ((int)event.ident == m_tickleFds[0]) {
                unsigned char dummy;
                MORDOR_VERIFY(read(m_tickleFds[0], &dummy, 1) == 1);
                MORDOR_LOG_VERBOSE(g_log) << this << " received tickle (" << event.data
                    << " remaining)";
                continue;
            }

            boost::mutex::scoped_lock lock(m_mutex);
            Event key;
            switch (event.filter) {
                case EVFILT_READ:
                    key = READ;
                    break;
                case EVFILT_WRITE:
                    key = WRITE;
                    break;
                default:
                    MORDOR_NOTREACHED();
            }
            std::map<std::pair<int, Event>, AsyncEvent>::iterator it =
                m_pendingEvents.find(std::pair<int, Event>((int)event.ident, key));
            if (it == m_pendingEvents.end())
                continue;
            AsyncEvent &e = it->second;

            bool remove = false;
            bool eof = !!(event.flags & EV_EOF);
            if ( (event.flags & EV_EOF) || (!e.m_dgClose && !e.m_fiberClose) ) {
                remove = true;
                event.flags = EV_DELETE;
                int rc2 = kevent(m_kqfd, &event, 1, NULL, 0, NULL);
                MORDOR_LOG_LEVEL(g_log, rc2 ? Log::ERROR : Log::VERBOSE)
                    << this << " kevent(" << m_kqfd << ", (" << event.ident
                    << ", " << event.filter << ", EV_DELETE)): " << rc2 << " ("
                    << lastError() << ")";
                if (rc2)
                    MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("kevent");
            }
            if (e.m_dg) {
                e.m_scheduler->schedule(e.m_dg);
                e.m_dg = NULL;
            } else if (e.m_fiber) {
                e.m_scheduler->schedule(e.m_fiber);
                e.m_fiber.reset();
            }
            if (eof && e.event.filter == EVFILT_READ) {
                if (e.m_dgClose) {
                    e.m_schedulerClose->schedule(e.m_dgClose);
                    e.m_dgClose = NULL;
                } else if (e.m_fiberClose) {
                    e.m_schedulerClose->schedule(e.m_fiberClose);
                    e.m_fiberClose.reset();
                }
            }
            if (remove)
                m_pendingEvents.erase(it);
        }
        try {
            Fiber::yield();
        } catch (OperationAbortedException &) {
            return;
        }
    }
}

void
IOManager::tickle()
{
    if (!hasIdleThreads()) {
        MORDOR_LOG_VERBOSE(g_log) << this << " 0 idle thread, no tickle.";
        return;
    }
    int rc = write(m_tickleFds[1], "T", 1);
    MORDOR_LOG_VERBOSE(g_log) << this << " write(" << m_tickleFds[1] << ", 1): "
        << rc << " (" << lastError() << ")";
    MORDOR_VERIFY(rc == 1);
}

}

#endif
