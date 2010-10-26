// Copyright (c) 2009 - Mozy, Inc.

#include "pch.h"

#ifdef LINUX

#include "iomanager_epoll.h"

#include <sys/epoll.h>

#include "assert.h"
#include "atomic.h"
#include "fiber.h"

// EPOLLRDHUP is missing in the header on etch
#ifndef EPOLLRDHUP
#define EPOLLRDHUP 0x2000
#endif

namespace Mordor {

static Logger::ptr g_log = Log::lookup("mordor:iomanager");

enum epoll_ctl_op_t
{
    epoll_ctl_op_t_dummy = 0x7ffffff
};

static std::ostream &operator <<(std::ostream &os, epoll_ctl_op_t op)
{
    switch (op) {
        case EPOLL_CTL_ADD:
            return os << "EPOLL_CTL_ADD";
        case EPOLL_CTL_MOD:
            return os << "EPOLL_CTL_MOD";
        case EPOLL_CTL_DEL:
            return os << "EPOLL_CTL_DEL";
        default:
            return os << (int)op;
    }
}

static std::ostream &operator <<(std::ostream &os, EPOLL_EVENTS events)
{
    if (!events) {
        return os << '0';
    }
    bool one = false;
    if (events & EPOLLIN) {
        os << "EPOLLIN";
        one = true;
    }
    if (events & EPOLLOUT) {
        if (one) os << " | ";
        os << "EPOLLOUT";
        one = true;
    }
    if (events & EPOLLPRI) {
        if (one) os << " | ";
        os << "EPOLLPRI";
        one = true;
    }
    if (events & EPOLLERR) {
        if (one) os << " | ";
        os << "EPOLLERR";
        one = true;
    }
    if (events & EPOLLHUP) {
        if (one) os << " | ";
        os << "EPOLLHUP";
        one = true;
    }
    if (events & EPOLLET) {
        if (one) os << " | ";
        os << "EPOLLET";
        one = true;
    }
    if (events & EPOLLONESHOT) {
        if (one) os << " | ";
        os << "EPOLLONESHOT";
        one = true;
    }
    if (events & EPOLLRDHUP) {
        if (one) os << " | ";
        os << "EPOLLRDHUP";
        one = true;
    }
    events = (EPOLL_EVENTS)(events & ~(EPOLLIN | EPOLLOUT | EPOLLPRI | EPOLLERR | EPOLLHUP | EPOLLET | EPOLLONESHOT | EPOLLRDHUP));
    if (events) {
        if (one) os << " | ";
        os << (uint32_t)events;
    }
    return os;
}

IOManager::AsyncState::AsyncState()
    : m_fd(0),
      m_events(NONE)
{}

IOManager::AsyncState::~AsyncState()
{
    boost::mutex::scoped_lock lock(m_mutex);
    MORDOR_ASSERT(!m_events);
}

IOManager::AsyncState::EventContext &
IOManager::AsyncState::contextForEvent(Event event)
{
    switch (event) {
        case READ:
            return m_in;
        case WRITE:
            return m_out;
        case CLOSE:
            return m_close;
        default:
            MORDOR_NOTREACHED();
    }
}

bool
IOManager::AsyncState::triggerEvent(Event event, size_t &pendingEventCount)
{
    if (!(m_events & event))
        return false;
    m_events = (Event)(m_events & ~event);
    atomicDecrement(pendingEventCount);
    EventContext &context = contextForEvent(event);
    if (context.dg)
        context.scheduler->schedule(context.dg);
    else
        context.scheduler->schedule(context.fiber);
    context.scheduler = NULL;
    context.fiber.reset();
    context.dg = NULL;
    return true;
}

IOManager::IOManager(size_t threads, bool useCaller)
    : Scheduler(threads, useCaller),
      m_pendingEventCount(0)
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
    memset(&event, 0, sizeof(epoll_event));
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = m_tickleFds[0];
    rc = epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_tickleFds[0], &event);
    MORDOR_LOG_LEVEL(g_log, rc ? Log::ERROR : Log::VERBOSE) << this
        << " epoll_ctl(" << m_epfd << ", EPOLL_CTL_ADD, " << m_tickleFds[0]
        << ", EPOLLIN | EPOLLET): " << rc << " (" << errno << ")";
    if (rc) {
        close(m_tickleFds[0]);
        close(m_tickleFds[1]);
        close(m_epfd);
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("epoll_ctl");
    }
    try {
        start();
    } catch (...) {
        close(m_tickleFds[0]);
        close(m_tickleFds[1]);
        close(m_epfd);
        throw;
    }
}

IOManager::~IOManager()
{
    stop();
    close(m_epfd);
    MORDOR_LOG_TRACE(g_log) << this << " close(" << m_epfd << ")";
    close(m_tickleFds[0]);
    MORDOR_LOG_VERBOSE(g_log) << this << " close(" << m_tickleFds[0] << ")";
    close(m_tickleFds[1]);
    // Yes, it would be more C++-esque to store a boost::shared_ptr in the
    // vector, but that requires an extra allocation per fd for the counter
    for (size_t i = 0; i < m_pendingEvents.size(); ++i) {
        if (m_pendingEvents[i])
            delete m_pendingEvents[i];
    }
}

bool
IOManager::stopping()
{
    unsigned long long timeout;
    return stopping(timeout);
}

void
IOManager::registerEvent(int fd, Event event, boost::function<void ()> dg)
{
    MORDOR_ASSERT(fd > 0);
    MORDOR_ASSERT(Scheduler::getThis());
    MORDOR_ASSERT(dg || Fiber::getThis());
    MORDOR_ASSERT(event == READ || event == WRITE || event == CLOSE);

    // Look up our state in the global map, expanding it if necessary
    boost::mutex::scoped_lock lock(m_mutex);
    if (m_pendingEvents.size() < (size_t)fd)
        m_pendingEvents.resize(fd * 3 / 2);
    if (!m_pendingEvents[fd - 1]) {
        m_pendingEvents[fd - 1] = new AsyncState();
        m_pendingEvents[fd - 1]->m_fd = fd;
    }
    AsyncState &state = *m_pendingEvents[fd - 1];
    MORDOR_ASSERT(fd == state.m_fd);
    lock.unlock();

    boost::mutex::scoped_lock lock2(state.m_mutex);

    MORDOR_ASSERT(!(state.m_events & event));
    int op = state.m_events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
    epoll_event epevent;
    epevent.events = EPOLLET | state.m_events | event;
    epevent.data.ptr = &state;
    int rc = epoll_ctl(m_epfd, op, fd, &epevent);
    MORDOR_LOG_LEVEL(g_log, rc ? Log::ERROR : Log::VERBOSE) << this
        << " epoll_ctl(" << m_epfd << ", " << (epoll_ctl_op_t)op << ", "
        << fd << ", " << (EPOLL_EVENTS)epevent.events << "): " << rc
        << " (" << errno << ")";
    if (rc)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("epoll_ctl");
    atomicIncrement(m_pendingEventCount);
    state.m_events = (Event)(state.m_events | event);
    AsyncState::EventContext &context = state.contextForEvent(event);
    MORDOR_ASSERT(!context.scheduler);
    MORDOR_ASSERT(!context.fiber);
    MORDOR_ASSERT(!context.dg);
    context.scheduler = Scheduler::getThis();
    if (!dg)
        context.fiber = Fiber::getThis();
    context.dg.swap(dg);
}

bool
IOManager::unregisterEvent(int fd, Event event)
{
    MORDOR_ASSERT(fd > 0);
    MORDOR_ASSERT(event == READ || event == WRITE || event == CLOSE);

    boost::mutex::scoped_lock lock(m_mutex);
    if (m_pendingEvents.size() < (size_t)fd)
        return false;
    if (!m_pendingEvents[fd - 1])
        return false;
    AsyncState &state = *m_pendingEvents[fd - 1];
    MORDOR_ASSERT(fd == state.m_fd);
    lock.unlock();

    boost::mutex::scoped_lock lock2(state.m_mutex);
    if (!(state.m_events & event))
        return false;

    MORDOR_ASSERT(fd == state.m_fd);
    Event newEvents = (Event)(state.m_events &~event);
    int op = newEvents ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events = EPOLLET | newEvents;
    epevent.data.fd = fd;
    int rc = epoll_ctl(m_epfd, op, fd, &epevent);
    MORDOR_LOG_LEVEL(g_log, rc ? Log::ERROR : Log::VERBOSE) << this
        << " epoll_ctl(" << m_epfd << ", " << (epoll_ctl_op_t)op << ", "
        << fd << ", " << (EPOLL_EVENTS)epevent.events << "): " << rc
        << " (" << errno << ")";
    if (rc)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("epoll_ctl");
    atomicDecrement(m_pendingEventCount);
    state.m_events = newEvents;
    AsyncState::EventContext &context = state.contextForEvent(event);
    context.scheduler = NULL;
    context.fiber.reset();
    context.dg = NULL;

    return true;
}

bool
IOManager::cancelEvent(int fd, Event event)
{
    MORDOR_ASSERT(fd > 0);
    MORDOR_ASSERT(event == READ || event == WRITE || event == CLOSE);

    boost::mutex::scoped_lock lock(m_mutex);
    if (m_pendingEvents.size() < (size_t)fd)
        return false;
    if (!m_pendingEvents[fd - 1])
        return false;
    AsyncState &state = *m_pendingEvents[fd - 1];
    MORDOR_ASSERT(fd == state.m_fd);
    lock.unlock();

    boost::mutex::scoped_lock lock2(state.m_mutex);
    if (!(state.m_events & event))
        return false;

    MORDOR_ASSERT(fd == state.m_fd);
    Event newEvents = (Event)(state.m_events &~event);
    int op = newEvents ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events = EPOLLET | newEvents;
    epevent.data.fd = fd;
    int rc = epoll_ctl(m_epfd, op, fd, &epevent);
    MORDOR_LOG_LEVEL(g_log, rc ? Log::ERROR : Log::VERBOSE) << this
        << " epoll_ctl(" << m_epfd << ", " << (epoll_ctl_op_t)op << ", "
        << fd << ", " << (EPOLL_EVENTS)epevent.events << "): " << rc
        << " (" << errno << ")";
    if (rc)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("epoll_ctl");
    state.triggerEvent(event, m_pendingEventCount);
    return true;
}

bool
IOManager::stopping(unsigned long long &nextTimeout)
{
    nextTimeout = nextTimer();
    return nextTimeout == ~0ull && Scheduler::stopping() &&
        m_pendingEventCount == 0;
}

void
IOManager::idle()
{
    epoll_event events[64];
    while (true) {
        unsigned long long nextTimeout;
        if (stopping(nextTimeout))
            return;
        int rc = -1;
        errno = EINTR;
        int timeout;
        while (rc < 0 && errno == EINTR) {
            timeout = -1;
            if (nextTimeout != ~0ull)
                timeout = (int)(nextTimeout / 1000) + 1;
            rc = epoll_wait(m_epfd, events, 64, timeout);
            if (rc < 0 && errno == EINTR)
                nextTimeout = nextTimer();
        }
        MORDOR_LOG_LEVEL(g_log, rc < 0 ? Log::ERROR : Log::VERBOSE) << this
            << " epoll_wait(" << m_epfd << ", 64, " << timeout << "): " << rc
            << " (" << errno << ")";
        if (rc < 0)
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("epoll_wait");
        std::vector<boost::function<void ()> > expired = processTimers();
        schedule(expired.begin(), expired.end());

        for(int i = 0; i < rc; ++i) {
            epoll_event &event = events[i];
            if (event.data.fd == m_tickleFds[0]) {
                unsigned char dummy;
                int rc2 = read(m_tickleFds[0], &dummy, 1);
                MORDOR_VERIFY(rc2 == 1);
                MORDOR_LOG_VERBOSE(g_log) << this << " received tickle";
                continue;
            }

            AsyncState &state = *(AsyncState *)event.data.ptr;

            boost::mutex::scoped_lock lock2(state.m_mutex);
            MORDOR_LOG_TRACE(g_log) << " epoll_event {"
                << (EPOLL_EVENTS)event.events << ", " << state.m_fd
                << "}, registered for " << (EPOLL_EVENTS)state.m_events;

            if (event.events & (EPOLLERR | EPOLLHUP))
                event.events |= EPOLLIN | EPOLLOUT;

            bool triggered = false;
            if (event.events & EPOLLIN)
                triggered = state.triggerEvent(READ, m_pendingEventCount);
            if (event.events & EPOLLOUT)
                triggered = triggered ||
                    state.triggerEvent(WRITE, m_pendingEventCount);
            if (event.events & EPOLLRDHUP)
                triggered = triggered ||
                    state.triggerEvent(CLOSE, m_pendingEventCount);

            // Nothing was triggered, probably because a prior cancelEvent call
            // (probably on a different thread) already triggered it, so no
            // need to tell epoll anything
            if (!triggered)
                continue;

            int op = state.m_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
            event.events = EPOLLET | state.m_events;
            int rc2 = epoll_ctl(m_epfd, op, state.m_fd, &event);
            MORDOR_LOG_LEVEL(g_log, rc2 ? Log::ERROR : Log::VERBOSE) << this
                << " epoll_ctl(" << m_epfd << ", " << (epoll_ctl_op_t)op << ", "
                << state.m_fd << ", " << (EPOLL_EVENTS)event.events << "): " << rc2
                << " (" << errno << ")";
            if (rc2)
                MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("epoll_ctl");
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
    int rc = write(m_tickleFds[1], "T", 1);
    MORDOR_LOG_VERBOSE(g_log) << this << " write(" << m_tickleFds[1] << ", 1): "
        << rc << " (" << errno << ")";
    MORDOR_VERIFY(rc == 1);
}

}

#endif
