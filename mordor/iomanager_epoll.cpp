// Copyright (c) 2009 - Decho Corp.

#include "pch.h"

#include "iomanager_epoll.h"

#include "assert.h"
#include "exception.h"
#include "log.h"

namespace Mordor {

static Logger::ptr g_log = Log::lookup("mordor:iomanager");

IOManagerEPoll::IOManagerEPoll(int threads, bool useCaller)
    : Scheduler(threads, useCaller)
{
    m_epfd = epoll_create(5000);
    MORDOR_LOG_LEVEL(g_log, m_epfd <= 0 ? Log::ERROR : Log::TRACE) << this
        << " epoll_create(5000): " << m_epfd;
    if (m_epfd <= 0)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("epoll_create");
    int rc = pipe(m_tickleFds);
    MORDOR_LOG_LEVEL(g_log, rc ? Log::ERROR : Log::VERBOSE) << this << " pipe(): "
        << rc << " (" << errno << ")";
    if (rc) {
        close(m_epfd);
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("pipe");
    }
    MORDOR_ASSERT(m_tickleFds[0] > 0);
    MORDOR_ASSERT(m_tickleFds[1] > 0);
    epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = m_tickleFds[0];
    rc = epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_tickleFds[0], &event);
    MORDOR_LOG_LEVEL(g_log, rc ? Log::ERROR : Log::VERBOSE) << this
        << " epoll_ctl(" << m_epfd << ", EPOLL_CTL_ADD, " << m_tickleFds[0]
        << ", EPOLLIN): " << rc << " (" << errno << ")";
    if (rc) {
        close(m_tickleFds[0]);
        close(m_tickleFds[1]);
        close(m_epfd);
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("epoll_ctl");
    }
}

IOManagerEPoll::~IOManagerEPoll()
{
    stop();
    close(m_epfd);
    MORDOR_LOG_TRACE(g_log) << this << " close(" << m_epfd << ")";
    close(m_tickleFds[0]);
    MORDOR_LOG_VERBOSE(g_log) << this << " close(" << m_tickleFds[0] << ")";
    close(m_tickleFds[1]);
}

void
IOManagerEPoll::registerEvent(int fd, Event events, boost::function<void ()> dg)
{
    MORDOR_ASSERT(fd > 0);
    MORDOR_ASSERT(Scheduler::getThis());
    MORDOR_ASSERT(Fiber::getThis());

    int epollevents = (int)events & (EPOLLIN | EPOLLOUT);
    MORDOR_ASSERT(epollevents != 0);
    boost::mutex::scoped_lock lock(m_mutex);
    int op;
    std::map<int, AsyncEvent>::iterator it = 
m_pendingEvents.find(fd);
    AsyncEvent *event;
    if (it == m_pendingEvents.end()) {
        op = EPOLL_CTL_ADD;
        event = &m_pendingEvents[fd];
        event->event.data.fd = fd;
        event->event.events = epollevents;
    } else {
        op = EPOLL_CTL_MOD;
        event = &it->second;
        // OR == XOR means that none of the same bits were set
        MORDOR_ASSERT((event->event.events | epollevents)
            == (event->event.events ^ epollevents));
        event->event.events |= epollevents;
    }
    if (epollevents & EPOLLIN) {
        event->m_schedulerIn = Scheduler::getThis();
        if (dg) {
            event->m_dgIn = dg;
            event->m_fiberIn.reset();
        } else {
            event->m_dgIn = NULL;
            event->m_fiberIn = Fiber::getThis();
        }
    }
    if (epollevents & EPOLLOUT) {
        event->m_schedulerOut = Scheduler::getThis();
        if (dg) {
            event->m_dgOut = dg;
            event->m_fiberOut.reset();
        } else {
            event->m_dgOut = NULL;
            event->m_fiberOut = Fiber::getThis();
        }
    }
    int rc = epoll_ctl(m_epfd, op, event->event.data.fd, &event->event);
    MORDOR_LOG_LEVEL(g_log, rc ? Log::ERROR : Log::VERBOSE) << this
        << " epoll_ctl(" << m_epfd << ", " << op << ", "
        << event->event.data.fd << ", " << event->event.events << "): " << rc
        << " (" << errno << ")";
    if (rc)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("epoll_ctl");
}

void
IOManagerEPoll::cancelEvent(int fd, Event events)
{
    boost::mutex::scoped_lock lock(m_mutex);
    std::map<int, AsyncEvent>::iterator it = m_pendingEvents.find(fd);
    if (it == m_pendingEvents.end())
        return;
    AsyncEvent &e = it->second;
    if ((events & EPOLLIN) && (e.event.events & EPOLLIN)) {
        if (e.m_dgIn)
            e.m_schedulerIn->schedule(e.m_dgIn);
        else
            e.m_schedulerIn->schedule(e.m_fiberIn);
        e.m_dgIn = NULL;
        e.m_fiberIn.reset();
    }
    if ((events & EPOLLOUT) && (e.event.events & EPOLLOUT)) {
        if (e.m_dgOut)
            e.m_schedulerOut->schedule(e.m_dgOut);
        else
            e.m_schedulerOut->schedule(e.m_fiberOut);
        e.m_dgOut = NULL;
        e.m_fiberOut.reset();
    }
    e.event.events &= ~events;
    if (e.event.events == 0) {
        int rc = epoll_ctl(m_epfd, EPOLL_CTL_DEL, fd, &e.event);
        MORDOR_LOG_LEVEL(g_log, rc ? Log::ERROR : Log::VERBOSE) << this
            << " epoll_ctl(" << m_epfd << ", EPOLL_CTL_DEL, " << fd
            << ", " << e.event.events << "): " << rc << " (" << errno << ")";
        if (rc)
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("epoll_ctl");
        m_pendingEvents.erase(it);
    }
}

Timer::ptr
IOManagerEPoll::registerTimer(unsigned long long us, boost::function<void ()> dg,
        bool recurring)
{
    bool atFront;
    Timer::ptr result = TimerManager::registerTimer(us, dg, recurring, atFront);
    if (atFront)
        tickle();
    return result;
}

void
IOManagerEPoll::idle()
{
    epoll_event events[64];
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
            int timeout = -1;
            unsigned long long nextTimeout = nextTimer();
            if (nextTimeout != ~0ull)
                timeout = (int)(nextTimeout / 1000);
            rc = epoll_wait(m_epfd, events, 64, timeout);
        }
        MORDOR_LOG_LEVEL(g_log, rc < 0 ? Log::ERROR : Log::VERBOSE) << this
            << " epoll_wait(" << m_epfd << "): " << rc << " (" << errno << ")";
        if (rc < 0)
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("epoll_wait");
        processTimers();

        for(int i = 0; i < rc; ++i) {
            epoll_event &event = events[i];
            if (event.data.fd == m_tickleFds[0]) {
                unsigned char dummy;
                int rc2 = read(m_tickleFds[0], &dummy, 1);
                MORDOR_VERIFY(rc2 == 1);
                MORDOR_LOG_VERBOSE(g_log) << this << " received tickle";
                continue;
            }
            bool err = event.events & (EPOLLERR | EPOLLHUP);
            boost::mutex::scoped_lock lock(m_mutex);
            std::map<int, AsyncEvent>::iterator it = 
m_pendingEvents.find(event.data.fd);
            if (it == m_pendingEvents.end())
                continue;
            AsyncEvent &e = it->second;
            if ((event.events & EPOLLIN) ||
                (err && (e.event.events & EPOLLIN))) {
                if (e.m_dgIn)
                    e.m_schedulerIn->schedule(e.m_dgIn);
                else
                    e.m_schedulerIn->schedule(e.m_fiberIn);
                e.m_dgIn = NULL;
                e.m_fiberIn.reset();
            }
            if ((event.events & EPOLLOUT) ||
                (err && (e.event.events & EPOLLOUT))) {
                if (e.m_dgOut)
                    e.m_schedulerOut->schedule(e.m_dgOut);
                else
                    e.m_schedulerOut->schedule(e.m_fiberOut);
                e.m_dgOut = NULL;
                e.m_fiberOut.reset();
            }
            e.event.events &= ~event.events;
            if (err || e.event.events == 0) {
                int rc2 = epoll_ctl(m_epfd, EPOLL_CTL_DEL, event.data.fd,
                    &e.event);
                MORDOR_LOG_LEVEL(g_log, rc2 ? Log::ERROR : Log::VERBOSE) << this
                    << " epoll_ctl(" << m_epfd << ", EPOLL_CTL_DEL, "
                    << event.data.fd << ", " << e.event.events << "): " << rc2
                    << " (" << errno << ")";
                if (rc2)
                    MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("epoll_ctl");
                m_pendingEvents.erase(it);
            }
        }
        Fiber::yield();
    }
}

void
IOManagerEPoll::tickle()
{
    int rc = write(m_tickleFds[1], "T", 1);
    MORDOR_LOG_VERBOSE(g_log) << this << " write(" << m_tickleFds[1] << ", 1): "
        << rc << " (" << errno << ")";
    MORDOR_VERIFY(rc == 1);
}

}
