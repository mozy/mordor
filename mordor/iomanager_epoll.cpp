// Copyright (c) 2009 - Mozy, Inc.

#include "pch.h"

#include "iomanager_epoll.h"

#include "assert.h"
#include "exception.h"
#include "log.h"

namespace Mordor {

static Logger::ptr g_log = Log::lookup("mordor:iomanager");

enum epoll_ctl_op_t
{
    epoll_ctl_op_t_dummy = 0x7ffffff
};

enum epoll_events_t
{
    epoll_events_t_dummy = 0x7ffffff
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

static std::ostream &operator <<(std::ostream &os, epoll_events_t events)
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
    if (events & EPOLLRDHUP) {
        if (one) os << " | ";
        os << "EPOLLRDHUP";
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
    events = (epoll_events_t)(events & ~(EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLPRI | EPOLLERR | EPOLLHUP | EPOLLET | EPOLLONESHOT));
    if (events) {
        if (one) os << " | ";
        os << (uint32_t)events;
    }
    return os;
}

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

bool
IOManagerEPoll::stopping()
{
    unsigned long long timeout;
    return stopping(timeout);
}

void
IOManagerEPoll::registerEvent(int fd, Event events, boost::function<void ()> dg)
{
    MORDOR_ASSERT(fd > 0);
    MORDOR_ASSERT(Scheduler::getThis());
    MORDOR_ASSERT(Fiber::getThis());

    int epollevents = (int)events & (EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLERR);
    MORDOR_ASSERT(epollevents != 0);
    boost::mutex::scoped_lock lock(m_mutex);
    int op;
    std::map<int, AsyncEvent>::iterator it = m_pendingEvents.find(fd);
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
    if (epollevents & EPOLLHUP) {
        event->m_schedulerClose = Scheduler::getThis();
        if (dg) {
            event->m_dgClose = dg;
            event->m_fiberClose.reset();
        } else {
            event->m_dgClose = NULL;
            event->m_fiberClose = Fiber::getThis();
        }
    }
    if (epollevents & EPOLLERR) {
        event->m_schedulerError = Scheduler::getThis();
        if (dg) {
            event->m_dgError = dg;
            event->m_fiberError.reset();
        } else {
            event->m_dgError = NULL;
            event->m_fiberError = Fiber::getThis();
        }
    }
    int rc = epoll_ctl(m_epfd, op, event->event.data.fd, &event->event);
    MORDOR_LOG_LEVEL(g_log, rc ? Log::ERROR : Log::VERBOSE) << this
        << " epoll_ctl(" << m_epfd << ", " << (epoll_ctl_op_t)op << ", "
        << event->event.data.fd << ", " << (epoll_events_t)event->event.events << "): " << rc
        << " (" << errno << ")";
    if (rc)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("epoll_ctl");
}

bool
IOManagerEPoll::unregisterEvent(int fd, Event events)
{
    boost::mutex::scoped_lock lock(m_mutex);
    std::map<int, AsyncEvent>::iterator it = m_pendingEvents.find(fd);
    if (it == m_pendingEvents.end())
        return false;
    // Nothing matching
    if (!(events & it->second.event.events))
        return false;
    bool result = false;
    AsyncEvent &e = it->second;
    if ((events & EPOLLIN) && (e.event.events & EPOLLIN)) {
        e.m_dgIn = NULL;
        e.m_fiberIn.reset();
        result = true;
    }
    if ((events & EPOLLOUT) && (e.event.events & EPOLLOUT)) {
        e.m_dgOut = NULL;
        e.m_fiberOut.reset();
        result = true;
    }
    if ((events & EPOLLHUP) && (e.event.events & EPOLLHUP)) {
        e.m_dgClose = NULL;
        e.m_fiberClose.reset();
        result = true;
    }
    if ((events & EPOLLERR) && (e.event.events & EPOLLERR)) {
        e.m_dgError = NULL;
        e.m_fiberError.reset();
        result = true;
    }
    e.event.events &= ~events;
    int op = e.event.events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    int rc = epoll_ctl(m_epfd, op, fd, &e.event);
    MORDOR_LOG_LEVEL(g_log, rc ? Log::ERROR : Log::VERBOSE) << this
        << " epoll_ctl(" << m_epfd << ", " << (epoll_ctl_op_t)op << ", " << fd
        << ", " << (epoll_events_t)e.event.events << "): " << rc << " (" << errno << ")";
    if (op == EPOLL_CTL_DEL)
        m_pendingEvents.erase(it);
    if (rc)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("epoll_ctl");
    return result;
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
    if ((events & EPOLLHUP) && (e.event.events & EPOLLHUP)) {
        if (e.m_dgClose)
            e.m_schedulerClose->schedule(e.m_dgClose);
        else
            e.m_schedulerClose->schedule(e.m_fiberClose);
        e.m_dgClose = NULL;
        e.m_fiberClose.reset();
    }
    if ((events & EPOLLERR) && (e.event.events & EPOLLERR)) {
        if (e.m_dgError)
            e.m_schedulerError->schedule(e.m_dgError);
        else
            e.m_schedulerError->schedule(e.m_fiberError);
        e.m_dgError = NULL;
        e.m_fiberError.reset();
    }
    e.event.events &= ~events;
    int op = e.event.events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    int rc = epoll_ctl(m_epfd, op, fd, &e.event);
    MORDOR_LOG_LEVEL(g_log, rc ? Log::ERROR : Log::VERBOSE) << this
        << " epoll_ctl(" << m_epfd << ", " << (epoll_ctl_op_t)op << ", " << fd
        << ", " << (epoll_events_t)e.event.events << "): " << rc << " (" << errno << ")";
    if (op == EPOLL_CTL_DEL)
        m_pendingEvents.erase(it);
    if (rc)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("epoll_ctl");
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

bool
IOManagerEPoll::stopping(unsigned long long &nextTimeout)
{
    nextTimeout = nextTimer();
    if (nextTimeout == ~0ull && Scheduler::stopping()) {
        boost::mutex::scoped_lock lock(m_mutex);
        if (m_pendingEvents.empty()) {
            return true;
        }
    }
    return false;
}

void
IOManagerEPoll::idle()
{
    epoll_event events[64];
    while (true) {
        unsigned long long nextTimeout;
        if (stopping(nextTimeout))
            return;
        int rc = -1;
        errno = EINTR;
        while (rc < 0 && errno == EINTR) {
            int timeout = -1;
            if (nextTimeout != ~0ull)
                timeout = (int)(nextTimeout / 1000) + 1;
            rc = epoll_wait(m_epfd, events, 64, timeout);
            if (rc < 0 && errno == EINTR)
                nextTimeout = nextTimer();
        }
        MORDOR_LOG_LEVEL(g_log, rc < 0 ? Log::ERROR : Log::VERBOSE) << this
            << " epoll_wait(" << m_epfd << "): " << rc << " (" << errno << ")";
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
            bool err = event.events & (EPOLLERR | EPOLLHUP);
            boost::mutex::scoped_lock lock(m_mutex);
            std::map<int, AsyncEvent>::iterator it =
m_pendingEvents.find(event.data.fd);
            if (it == m_pendingEvents.end())
                continue;
            AsyncEvent &e = it->second;
            MORDOR_LOG_TRACE(g_log) << " epoll_event {"
                << (epoll_events_t)event.events << ", " << event.data.fd
                << "}, registered for " << (epoll_events_t)e.event.events;
            if ((event.events & EPOLLERR) && (e.event.events & EPOLLERR)) {
                if (e.m_dgError)
                    e.m_schedulerError->schedule(e.m_dgError);
                else
                    e.m_schedulerError->schedule(e.m_fiberError);
                // Block other events from firing
                e.m_dgError = NULL;
                e.m_fiberError.reset();
                e.m_dgIn = NULL;
                e.m_fiberIn.reset();
                e.m_dgOut = NULL;
                e.m_fiberOut.reset();
                event.events = 0;
                e.event.events = 0;
            }
            if ((event.events & EPOLLHUP) && (e.event.events & EPOLLHUP)) {
                if (e.m_dgClose)
                    e.m_schedulerError->schedule(e.m_dgClose);
                else
                    e.m_schedulerError->schedule(e.m_fiberClose);
                // Block write event from firing
                e.m_dgOut = NULL;
                e.m_fiberOut.reset();
                e.m_dgClose = NULL;
                e.m_fiberClose.reset();
                event.events &= EPOLLOUT;
                e.event.events &= EPOLLOUT;
                err = false;
            }

            if (((event.events & EPOLLIN) ||
                err) && (e.event.events & EPOLLIN)) {
                if (e.m_dgIn)
                    e.m_schedulerIn->schedule(e.m_dgIn);
                else
                    e.m_schedulerIn->schedule(e.m_fiberIn);
                e.m_dgIn = NULL;
                e.m_fiberIn.reset();
                event.events |= EPOLLIN;
            }
            if (((event.events & EPOLLOUT) ||
                err) && (e.event.events & EPOLLOUT)) {
                if (e.m_dgOut)
                    e.m_schedulerOut->schedule(e.m_dgOut);
                else
                    e.m_schedulerOut->schedule(e.m_fiberOut);
                e.m_dgOut = NULL;
                e.m_fiberOut.reset();
                event.events |= EPOLLOUT;
            }
            e.event.events &= ~event.events;

            int op = e.event.events == 0 ? EPOLL_CTL_DEL : EPOLL_CTL_MOD;
            int rc2 = epoll_ctl(m_epfd, op, event.data.fd,
                &e.event);
            MORDOR_LOG_LEVEL(g_log, rc2 ? Log::ERROR : Log::VERBOSE) << this
                << " epoll_ctl(" << m_epfd << ", " << (epoll_ctl_op_t)op << ", "
                << event.data.fd << ", " << (epoll_events_t)e.event.events << "): " << rc2
                << " (" << errno << ")";
            if (rc2)
                MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("epoll_ctl");
            if (op == EPOLL_CTL_DEL)
                m_pendingEvents.erase(it);
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
