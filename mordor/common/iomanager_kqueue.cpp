// Copyright (c) 2009 - Decho Corp.

#include "pch.h"

#include "iomanager_kqueue.h"

#include "assert.h"
#include "exception.h"
#include "log.h"

static Logger::ptr g_log = Log::lookup("mordor:common:iomanager");

IOManagerKQueue::IOManagerKQueue(int threads, bool useCaller)
    : Scheduler(threads, useCaller)
{
    m_kqfd = kqueue();
    LOG_LEVEL(g_log, m_kqfd <= 0 ? Log::ERROR : Log::TRACE) << this
        << " kqueue(): " << m_kqfd;
    if (m_kqfd <= 0) {
        throwExceptionFromLastError("kqueue");
    }
    int rc = pipe(m_tickleFds);
    LOG_LEVEL(g_log, rc ? Log::ERROR : Log::VERBOSE) << this << " pipe(): "
        << rc << " (" << errno << ")";
    if (rc) {
        close(m_kqfd);
        throwExceptionFromLastError("pipe");
    }
    ASSERT(m_tickleFds[0] > 0);
    ASSERT(m_tickleFds[1] > 0);
    struct kevent event;
    EV_SET(&event, m_tickleFds[0], EVFILT_READ, EV_ADD, 0, 0, NULL);
    rc = kevent(m_kqfd, &event, 1, NULL, 0, NULL);
    LOG_LEVEL(g_log, rc ? Log::ERROR : Log::VERBOSE) << this << " kevent("
        << m_kqfd << ", " << m_tickleFds[0] << ", EVFILT_READ, EV_ADD): " << rc
        << " (" << errno << ")";
    if (rc) {
        close(m_tickleFds[0]);
        close(m_tickleFds[1]);
        close(m_kqfd);
        throwExceptionFromLastError("kevent");
    }
}

IOManagerKQueue::~IOManagerKQueue()
{
    stop();
    close(m_kqfd);
    LOG_TRACE(g_log) << this << " close(" << m_kqfd << ")";
    close(m_tickleFds[0]);
    LOG_VERBOSE(g_log) << this << " close(" << m_tickleFds[0] << ")";
    close(m_tickleFds[1]);
}

void
IOManagerKQueue::registerEvent(int fd, Event events,
                               boost::function<void ()> dg)
{
    ASSERT(fd > 0);
    ASSERT(Scheduler::getThis());
    ASSERT(Fiber::getThis());

    std::map<std::pair<int, Event>, AsyncEvent>::iterator it =
        m_pendingEvents.find(std::pair<int, Event>(fd, events));
    ASSERT(it == m_pendingEvents.end());
    AsyncEvent& e = m_pendingEvents[std::pair<int, Event>(fd, events)];
    e.event.ident = fd;
    e.event.flags = EV_ADD;
    e.event.filter = (short)events;
    e.m_scheduler = Scheduler::getThis();
    if (dg) {
        e.m_dg = dg;
    } else {
        e.m_dg = NULL;
        e.m_fiber = Fiber::getThis();
    }
    int rc = kevent(m_kqfd, &e.event, 1, NULL, 0, NULL);
    LOG_LEVEL(g_log, rc ? Log::ERROR : Log::VERBOSE) << this << " kevent("
        << m_kqfd << ", (" << fd << ", " << events << ", EV_ADD)): " << rc
        << " (" << errno << ")";
    if (rc) {
        throwExceptionFromLastError("kevent");
    }
}

void
IOManagerKQueue::cancelEvent(int fd, Event events)
{
    boost::mutex::scoped_lock lock(m_mutex);
    std::map<std::pair<int, Event>, AsyncEvent>::iterator it =
        m_pendingEvents.find(std::pair<int, Event>(fd, events));
    if (it == m_pendingEvents.end())
        return;
    AsyncEvent &e = it->second;
    ASSERT(e.event.ident == (unsigned)fd);
    ASSERT(e.event.filter == (short)events);
    e.event.flags = EV_DELETE;
    int rc = kevent(m_kqfd, &e.event, 1, NULL, 0, NULL);
    LOG_LEVEL(g_log, rc ? Log::ERROR : Log::VERBOSE) << this << " kevent("
        << m_kqfd << ", (" << fd << ", " << events << ", EV_DELETE)): " << rc
        << " (" << errno << ")";
    if (rc) {
        throwExceptionFromLastError("kevent");
    }
    if (e.m_dg)
        e.m_scheduler->schedule(e.m_dg);
    else
        e.m_scheduler->schedule(e.m_fiber);
    m_pendingEvents.erase(it);
}

Timer::ptr
IOManagerKQueue::registerTimer(unsigned long long us, boost::function<void ()> dg,
        bool recurring)
{
    bool atFront;
    Timer::ptr result = TimerManager::registerTimer(us, dg, recurring, atFront);
    if (atFront)
        tickle();
    return result;
}

void
IOManagerKQueue::idle()
{
    struct kevent events[64];
    while (true) {
        if (stopping()) {
            boost::mutex::scoped_lock lock(m_mutex);
            if (m_pendingEvents.empty()) {
                return;
            }
        }
        int rc = -1;
        errno = EINTR;
        while (rc < 0 && errno == EINTR) {
            struct timespec *timeout = NULL, timeoutStorage;
            unsigned long long nextTimeout = nextTimer();
            if (nextTimeout != ~0ull) {
                timeout = &timeoutStorage;
                timeout->tv_sec = nextTimeout / 1000000;
                timeout->tv_nsec = (nextTimeout % 1000000) * 1000;
            }
            rc = kevent(m_kqfd, NULL, 0, events, 64, timeout);
        }
        LOG_LEVEL(g_log, rc < 0 ? Log::ERROR : Log::VERBOSE) << this
            << " kevent(" << m_kqfd << "): " << rc << " (" << errno << ")";
        if (rc < 0) {
            throwExceptionFromLastError("kevent");
        }
        processTimers();

        for(int i = 0; i < rc; ++i) {
            struct kevent &event = events[i];
            if ((int)event.ident == m_tickleFds[0]) {
                unsigned char dummy;
                int rc2 = read(m_tickleFds[0], &dummy, 1);
                ASSERT(rc2 == 1);
                continue;
            }

            boost::mutex::scoped_lock lock(m_mutex);
            std::map<std::pair<int, Event>, AsyncEvent>::iterator it =
                m_pendingEvents.find(std::pair<int, Event>((int)event.ident, (Event)event.filter));
            if (it == m_pendingEvents.end())
                continue;
            const AsyncEvent &e = it->second;

            event.flags = EV_DELETE;
            rc = kevent(m_kqfd, &event, 1, NULL, 0, NULL);
            LOG_LEVEL(g_log, rc ? Log::ERROR : Log::VERBOSE) << this
                << " kevent(" << m_kqfd << ", (" << event.ident << ", "
                << event.filter << ", EV_DELETE)): " << rc << " (" << errno
                << ")";
            if (rc) {
                throwExceptionFromLastError("kevent");
            }
            if (e.m_dg)
                e.m_scheduler->schedule(e.m_dg);
            else
                e.m_scheduler->schedule(e.m_fiber);
            m_pendingEvents.erase(it);
        }
        Fiber::yield();
    }
}

void
IOManagerKQueue::tickle()
{
    int rc = write(m_tickleFds[1], "T", 1);
    LOG_VERBOSE(g_log) << this << " write(" << m_tickleFds[1] << ", 1): "
        << rc << " (" << errno << ")";
    VERIFY(rc == 1);
}
