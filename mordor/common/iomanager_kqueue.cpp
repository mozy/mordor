// Copyright (c) 2009 - Decho Corp.

#include "iomanager_kqueue.h"

#include "exception.h"

IOManagerKQueue::IOManagerKQueue(int threads, bool useCaller)
    : Scheduler(threads, useCaller)
{
    m_kqfd = kqueue();
    if (m_kqfd <= 0) {
        throwExceptionFromLastError();
    }
    if (pipe(m_tickleFds)) {
        close(m_kqfd);
        throwExceptionFromLastError();
    }
    assert(m_tickleFds[0] > 0);
    assert(m_tickleFds[1] > 0);
    struct kevent event;
    EV_SET(&event, m_tickleFds[0], EVFILT_READ, EV_ADD, 0, 0, NULL);
    if (kevent(m_kqfd, &event, 1, NULL, 0, NULL)) {
        close(m_tickleFds[0]);
        close(m_tickleFds[1]);
        close(m_kqfd);
        throwExceptionFromLastError();
    }
}

IOManagerKQueue::~IOManagerKQueue()
{
    close(m_kqfd);
    close(m_tickleFds[0]);
    close(m_tickleFds[1]);
}

void
IOManagerKQueue::registerEvent(int fd, Event events)
{
    assert(fd > 0);
    assert(Scheduler::getThis());
    assert(Fiber::getThis());

    AsyncEvent event;
    event.event.ident = fd;
    event.event.flags = EV_ADD;
    event.event.filter = (short)events;
    std::pair<std::set<AsyncEvent>::iterator, bool> it = m_pendingEvents.insert(event);
    assert(it.second);
    ((AsyncEvent*)&*it.first)->event.udata = (void *)&it.first->event;
    if (kevent(m_kqfd, &it.first->event, 1, NULL, 0, NULL)) {
        throwExceptionFromLastError();
    }
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
        while (rc < 0 && errno == EINTR)
            rc = kevent(m_kqfd, NULL, 0, events, 64, NULL);
        if (rc <= 0) {
            throwExceptionFromLastError();
        }

        for(int i = 0; i < rc; ++i) {
            struct kevent &event = events[i];
            if ((int)event.ident == m_tickleFds[0]) {
                unsigned char dummy;
                int rc2 = read(m_tickleFds[0], &dummy, 1);
                assert(rc2 == 1);
                continue;
            }

            boost::mutex::scoped_lock lock(m_mutex);
            std::set<AsyncEvent>::iterator it = m_pendingEvents.find(*(AsyncEvent*)event.udata);
            assert(it != m_pendingEvents.end());
            const AsyncEvent &e = *it;
            e.m_scheduler->schedule(e.m_fiber);

            event.flags = EV_DELETE;
            if (kevent(m_kqfd, &event, 1, NULL, 0, NULL)) {
                throwExceptionFromLastError();
            }
            m_pendingEvents.erase(it);
        }
        Fiber::yield();
    }
}

void
IOManagerKQueue::tickle()
{
    int rc = write(m_tickleFds[1], "T", 1);
    assert(rc == 1);
}
