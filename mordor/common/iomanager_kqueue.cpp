// Copyright (c) 2009 - Decho Corp.

#include "pch.h"

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
    stop();
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

    std::map<std::pair<int, Event>, AsyncEvent>::iterator it =
        m_pendingEvents.find(std::pair<int, Event>(fd, events));
    assert(it == m_pendingEvents.end());
    AsyncEvent& e = m_pendingEvents[std::pair<int, Event>(fd, events)];
    e.event.ident = fd;
    e.event.flags = EV_ADD;
    e.event.filter = (short)events;
    e.m_scheduler = Scheduler::getThis();
    e.m_fiber = Fiber::getThis();
    if (kevent(m_kqfd, &e.event, 1, NULL, 0, NULL)) {
        throwExceptionFromLastError();
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

    e.event.flags = EV_DELETE;
    if (kevent(m_kqfd, &e.event, 1, NULL, 0, NULL)) {
        throwExceptionFromLastError();
    }
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
        if (rc < 0) {
            throwExceptionFromLastError();
        }
        processTimers();

        for(int i = 0; i < rc; ++i) {
            struct kevent &event = events[i];
            if ((int)event.ident == m_tickleFds[0]) {
                unsigned char dummy;
                int rc2 = read(m_tickleFds[0], &dummy, 1);
                assert(rc2 == 1);
                continue;
            }

            boost::mutex::scoped_lock lock(m_mutex);
            std::map<std::pair<int, Event>, AsyncEvent>::iterator it =
                m_pendingEvents.find(std::pair<int, Event>((int)event.ident, (Event)event.filter));
            if (it == m_pendingEvents.end())
                continue;
            const AsyncEvent &e = it->second;

            event.flags = EV_DELETE;
            if (kevent(m_kqfd, &event, 1, NULL, 0, NULL)) {
                throwExceptionFromLastError();
            }
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
    assert(rc == 1);
}
