// Copyright (c) 2009 - Decho Corp.

#include "iomanager_epoll.h"

#include "exception.h"

IOManagerEPoll::IOManagerEPoll(int threads, bool useCaller)
    : Scheduler(threads, useCaller)
{
    m_epfd = epoll_create(5000);
    if (m_epfd <= 0) {
        throwExceptionFromLastError();
    }
    if (pipe(m_tickleFds)) {
        close(m_epfd);
        throwExceptionFromLastError();
    }
    assert(m_tickleFds[0] > 0);
    assert(m_tickleFds[1] > 0);
    epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = m_tickleFds[0];
    if (epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_tickleFds[0], &event)) {
        close(m_tickleFds[0]);
        close(m_tickleFds[1]);
        close(m_epfd);
        throwExceptionFromLastError();
    }
}

IOManagerEPoll::~IOManagerEPoll()
{
    close(m_epfd);
    close(m_tickleFds[0]);
    close(m_tickleFds[1]);
}

void
IOManagerEPoll::registerEvent(int fd, Event events)
{
    assert(fd > 0);
    assert(Scheduler::getThis());
    assert(Fiber::getThis());

    int epollevents = (int)events & (EPOLLIN | EPOLLOUT);
    assert(epollevents != 0);
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
        assert((event->event.events | epollevents)
            == (event->event.events ^ epollevents));
        event->event.events |= epollevents;
    }
    if (epollevents & EPOLLIN) {
        event->m_schedulerIn = Scheduler::getThis();
        event->m_fiberIn = Fiber::getThis();
    }
    if (epollevents & EPOLLOUT) {
        event->m_schedulerOut = Scheduler::getThis();
        event->m_fiberOut = Fiber::getThis();
    }
    if (epoll_ctl(m_epfd, op, event->event.data.fd, &event->event)) {
        throwExceptionFromLastError();
    }
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
        while (rc < 0 && errno == EINTR)
            rc = epoll_wait(m_epfd, events, 64, -1);
        if (rc <= 0) {
            throwExceptionFromLastError();
        }

        for(int i = 0; i < rc; ++i) {
            epoll_event &event = events[i];
            if (event.data.fd == m_tickleFds[0]) {
                unsigned char dummy;
                int rc2 = read(m_tickleFds[0], &dummy, 1);
                assert(rc2 == 1);
                continue;
            }
            bool err = event.events & (EPOLLERR | EPOLLHUP);
            boost::mutex::scoped_lock lock(m_mutex);
            std::map<int, AsyncEvent>::iterator it = 
m_pendingEvents.find(event.data.fd);
            assert(it != m_pendingEvents.end());
            AsyncEvent &e = it->second;
            if ((event.events & EPOLLIN) ||
                (err && (e.event.events & EPOLLIN))) {
                e.m_schedulerIn->schedule(e.m_fiberIn);
            }
            if ((event.events & EPOLLOUT) ||
                (err && (e.event.events & EPOLLOUT))) {
                e.m_schedulerOut->schedule(e.m_fiberOut);
            }
            e.event.events &= ~event.events;
            if (err || e.event.events == 0) {
                if (epoll_ctl(m_epfd, EPOLL_CTL_DEL, event.data.fd, &e.event)) {
                    throwExceptionFromLastError();
                }
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
    assert(rc == 1);
}
