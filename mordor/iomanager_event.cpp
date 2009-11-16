// Copyright (c) 2009 - Decho Corp.

#include "pch.h"

#include "iomanager_event.h"

#include "assert.h"
#include "exception.h"
#include "log.h"

namespace Mordor {

static Logger::ptr g_log = Log::lookup("mordor:iomanager");

ThreadLocalStorage<struct event_base> IOManagerEvent::t_evBase;
ThreadLocalStorage<struct event> IOManagerEvent::t_evTickle;;
ThreadLocalStorage<IOManagerEvent::TickleState> IOManagerEvent::t_tickleState;

IOManagerEvent::IOManagerEvent(int threads, bool useCaller)
    : Scheduler(threads, useCaller),
      m_addEventsSize(0)
{
    MORDOR_LOG_DEBUG(g_log) << this << " IOManagerEvent()";
}

IOManagerEvent::~IOManagerEvent()
{
    MORDOR_LOG_DEBUG(g_log) << this << " ~IOManagerEvent()";
    stop();
    MORDOR_LOG_DEBUG(g_log) << this << " ~IOManagerEvent() done";
}

void
IOManagerEvent::registerEvent(int fd, Event events, Delegate dg)
{
    MORDOR_LOG_DEBUG(g_log) << this
        << " registerEvent(" << fd << ", " << events << ")";

    MORDOR_ASSERT(fd > 0);
    MORDOR_ASSERT(Scheduler::getThis());
    MORDOR_ASSERT(Fiber::getThis());

    // Mordor's iomanager interface only allows for one registration
    // type at a time, although, this code actually supports
    // both, so removing this assert would be safe
    MORDOR_ASSERT(events == EV_READ || events == EV_WRITE);

    boost::mutex::scoped_lock lock(m_mutex);

    // Find or allocate an event and get it put into the right
    // queue, either the new queue for initial add or queue it
    // up for the thread that has already been working with
    // the event... note, the event hasn't been fully
    // initialized yet, so don't release the lock at the end
    // of this snippet even though we are done with the
    // shared collection datastructures (since the event
    // itself doesn't have a lock)
    AsyncEvent* ev;
    RegisteredEvents::iterator it = m_registeredEvents.find(fd);
    if (it == m_registeredEvents.end()) {
        ev = &m_registeredEvents[fd];
        event_set(&ev->m_ev, fd, events, &IOManagerEvent::eventCb, this);
        MORDOR_LOG_VERBOSE(g_log) << this
            << " m_addEvents.push_back(" << *ev << ")";
        m_addEventsSize++;
        m_addEvents.push_back(ev);
        tickleLocked();
    } else {
        ev = &it->second;

        // Double registration of the same event type isn't allowed
        MORDOR_ASSERT(!(ev->m_events & events));

        // If the event is already in a queue being passed to a thread
        // we shouldn't requeue it.  The changes get merged and will
        // be processed correctly
        if (!ev->m_queued) {
            MORDOR_LOG_VERBOSE(g_log) << this
                << " m_modEvents[" << ev->m_tid << "].push_back("
                << *ev << ")";
            m_modEvents[ev->m_tid].push_back(ev);
            tickleLocked(ev->m_tid);
        }
    }

    ev->m_events |= events;
    ev->m_queued = true;

    // Prep up the read callback state
    if (events == EV_READ) {
        ev->m_schedulerRead = Scheduler::getThis();
        if (dg) {
            MORDOR_LOG_VERBOSE(g_log) << this
                << " register EV_READ dg " << *ev;
            ev->m_dgRead = dg;
            ev->m_fiberRead.reset();
        } else {
            MORDOR_LOG_VERBOSE(g_log) << this
                << " register EV_READ fiber " << *ev;
            ev->m_dgRead = NULL;
            ev->m_fiberRead = Fiber::getThis();
        }
    }

    // Prep up the write callback state
    if (events == EV_WRITE) {
        ev->m_schedulerWrite = Scheduler::getThis();
        if (dg) {
            MORDOR_LOG_VERBOSE(g_log) << this
                << " register EV_WRITE dg " << *ev;
            ev->m_dgWrite = dg;
            ev->m_fiberWrite.reset();
        } else {
            MORDOR_LOG_VERBOSE(g_log) << this
                << " register EV_WRITE fiber " << *ev;
            ev->m_dgWrite = dg;
            ev->m_fiberWrite = Fiber::getThis();
        }
    }
}

void
IOManagerEvent::cancelEvent(int fd, Event events)
{
    MORDOR_LOG_DEBUG(g_log) << this
        << " cancelEvent(" << fd << ", " << events << ")";

    // Mordor's iomanager interface only allows for one cancel event
    // type at a time, although, this code actually supports
    // both, so removing this assert would be safe
    MORDOR_ASSERT(events == EV_READ || events == EV_WRITE);

    boost::mutex::scoped_lock lock(m_mutex);
    RegisteredEvents::iterator it = m_registeredEvents.find(fd);
    if (it != m_registeredEvents.end()) {
        AsyncEvent* ev = &it->second;
        MORDOR_ASSERT(ev->m_events & events);
        MORDOR_ASSERT(ev->m_ev.ev_fd == fd);

        scheduleEvent(ev, events);

        // If already in a queue, these changes will be picked up so there
        // is no reason to requeue it.  Note, that the event is possibly
        // in the add queue if a register was quickly followed by a
        // cancel
        if (!ev->m_queued) {
            // If not in a queue, this is guaranteed to have received
            // a threadid
            MORDOR_LOG_VERBOSE(g_log) << this
                << " m_modEvents[" << ev->m_tid << "].push_back("
                << *ev << ")";
            m_modEvents[ev->m_tid].push_back(ev);
            tickleLocked(ev->m_tid);
        }
    }
}

Timer::ptr
IOManagerEvent::registerTimer(unsigned long long us, Delegate dg,
                              bool recurring)
{
    // XXX: is register timer thread safe?
    bool atFront;
    Timer::ptr result = TimerManager::registerTimer(us, dg, recurring,
                                                    atFront);
    if (atFront) {
        tickle();
    }
    return result;
}

void
IOManagerEvent::idle()
{
    MORDOR_LOG_DEBUG(g_log) << this << " idle()";
    initThread();

    while (true) {
        processAdds();
        processMods();

        // This needs to be done here instead of as a condition of the loop
        // because we might have things in the registeredEvents that
        // need to be removed via processMods
        if (checkDone()) {
            break;
        }

        addTickle();

        // Run the event loop. All the fds that have activity will have
        // their fibers or delegates scheduled as part of their callbacks.
        // By the time this returns, all events will have been processed.
        int rc = event_base_loop(t_evBase.get(), EVLOOP_ONCE);
        MORDOR_LOG_LEVEL(g_log, rc < 0 ? Log::ERROR : Log::VERBOSE) << this
            << " event_base_loop(" << t_evBase.get() << "): "
            << rc << " (" << errno << ")";
        if (rc < 0) {
            MORDOR_THROW_EXCEPTION(LibEventBaseLoopFailed());
        }

        std::vector<boost::function<void ()> > expired = processTimers();
        MORDOR_LOG_VERBOSE(g_log) << this
            << " expired.size() " << expired.size();
        schedule(expired.begin(), expired.end());

        Fiber::yield();
    }

    cleanupThread();

    MORDOR_LOG_DEBUG(g_log) << this << " idle() done";
}

void
IOManagerEvent::tickle()
{
    MORDOR_LOG_VERBOSE(g_log) << this << " tickle()";
    boost::mutex::scoped_lock lock(m_mutex);
    tickleLocked();
}

void
IOManagerEvent::tickleLocked()
{
    MORDOR_LOG_VERBOSE(g_log) << this << " tickleLocked()";
    for (ThreadTickleState::iterator it(m_threadTickleState.begin());
         it != m_threadTickleState.end(); ++it) {
        tickleLocked(it->second);
    }
}

void
IOManagerEvent::tickle(boost::thread::id id)
{
    MORDOR_LOG_VERBOSE(g_log) << this << " tickle(" << id << ")";
    boost::mutex::scoped_lock lock(m_mutex);
    tickleLocked(id);
}

void
IOManagerEvent::tickleLocked(boost::thread::id id)
{
    MORDOR_LOG_VERBOSE(g_log) << this << " tickleLocked(" << id << ")";

    ThreadTickleState::iterator it = m_threadTickleState.find(id);
    MORDOR_ASSERT(it != m_threadTickleState.end());
    if (it != m_threadTickleState.end()) {
        tickleLocked(it->second);
    }
}

void
IOManagerEvent::tickleLocked(TickleState* ts)
{
    if (ts->m_tickled == false) {
        ts->m_tickled = true;
        int rc = write(ts->m_fds[1], "T", 1);
        MORDOR_LOG_VERBOSE(g_log) << this
            << " write(" << ts->m_fds << ", 1): "
            << rc << " (" << errno << ")";
        MORDOR_VERIFY(rc == 1);
    }
}

void
IOManagerEvent::initThread()
{
    MORDOR_LOG_DEBUG(g_log) << this << " initThread()";

    // We could be more efficient with the use of the lock in this
    // method, but really, it's a one time shot, so who cares
    boost::mutex::scoped_lock lock(m_mutex);

    // Setup the tickle fds
    TickleState* ts = new TickleState;
    ts->m_tickled = false;
    t_tickleState.reset(ts);

    int rc = pipe(ts->m_fds);
    MORDOR_LOG_LEVEL(g_log, rc ? Log::ERROR : Log::VERBOSE)
        << this << " pipe(): " << rc << " (" << errno << ")";
    if (rc) {
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("pipe");
    }

    MORDOR_ASSERT(ts->m_fds[0] >= 0);
    MORDOR_ASSERT(ts->m_fds[1] >= 0);

    m_threadTickleState[boost::this_thread::get_id()] = ts;

    // setup the event base
    t_evBase.reset(event_base_new());
    MORDOR_LOG_LEVEL(g_log, t_evBase.get() == NULL ?
                     Log::ERROR : Log::VERBOSE)
        << this << " event_base_new(): "
        << t_evBase.get() << " " << event_base_get_method(t_evBase.get());
    if (t_evBase.get() == NULL) {
        MORDOR_THROW_EXCEPTION(LibEventBaseNewFailed());
    }

    // setup the tickle event for this thread
    t_evTickle.reset(new struct event);
    event_set(t_evTickle.get(), ts->m_fds[0], EV_READ,
              &IOManagerEvent::tickled, (void*) this);

    rc = event_base_set(t_evBase.get(), t_evTickle.get());
    if (rc < 0) {
        MORDOR_THROW_EXCEPTION(LibEventBaseSetFailed());
    }

    MORDOR_LOG_DEBUG(g_log) << this << " initThread() done";
}

void
IOManagerEvent::cleanupThread()
{
    MORDOR_LOG_DEBUG(g_log) << this << " cleanupThread()";

    // We could be more efficient with the use of the lock in this
    // method, but really, it's a one time shot, so who cares
    boost::mutex::scoped_lock lock(m_mutex);

    // cleanup tickle event
    delete t_evTickle.get();
    t_evTickle.reset(NULL);

    // cleanup event base
    event_base_free(t_evBase.get());
    t_evBase.reset(NULL);

    // cleanup tickle fds
    for (int i = 0; i < 2; i++) {
        MORDOR_LOG_VERBOSE(g_log) << this
            << " close(" << t_tickleState.get()->m_fds[i] << ")";
        close(t_tickleState.get()->m_fds[i]);
    }

    // clean up tickle data structures
    m_threadTickleState.erase(boost::this_thread::get_id());

    delete t_tickleState.get();
    t_tickleState.reset(NULL);

    MORDOR_LOG_DEBUG(g_log) << this << " cleanupDone()";
}

bool
IOManagerEvent::checkDone()
{
    MORDOR_LOG_VERBOSE(g_log) << this
        << " checkDone stopping(): " << stopping();
    if (stopping()) {
        boost::mutex::scoped_lock lock(m_mutex);
        MORDOR_LOG_VERBOSE(g_log) << this
            << " checkDone registeredEvents.size() "
            << m_registeredEvents.size();
        if (m_registeredEvents.empty()) {
            return true;
        }
    }

    return false;
}

void
IOManagerEvent::processAdds()
{
    boost::mutex::scoped_lock lock(m_mutex);

    // Try to balance the incoming fd adds accross the various threads.
    size_t maxTodo = m_addEventsSize / threadCount() + 1;

    while (maxTodo-- && !m_addEvents.empty()) {
        int rc;

        AsyncEvent* ev = m_addEvents.front();
        m_addEvents.pop_front();
        m_addEventsSize--;

        ev->m_queued = false;

        MORDOR_LOG_VERBOSE(g_log) << this << " add " << *ev;

        // This can happen if someone does an registerEvent() followed
        // by a cancleEvent() before we get here
        if (ev->m_events == 0) {
            m_registeredEvents.erase(ev->m_ev.ev_fd);
            continue;
        }

        // Take ownership of the event
        ev->m_tid = boost::this_thread::get_id();

        rc = event_base_set(t_evBase.get(), &ev->m_ev);
        MORDOR_LOG_LEVEL(g_log, rc ? Log::ERROR : Log::VERBOSE) << this
            << " event_base_set(" << t_evBase.get() << ", " << *ev << "): "
            << rc << " (" << errno << ")";
        if (rc < 0) {
            // TODO: see error handling comment in addEvent
            MORDOR_THROW_EXCEPTION(LibEventBaseSetFailed());
        }

        addEvent(ev);
    }
}

void
IOManagerEvent::processMods()
{
    boost::mutex::scoped_lock lock(m_mutex);
    EventList& modEvents = m_modEvents[boost::this_thread::get_id()];

    while (!modEvents.empty()) {
        AsyncEvent* ev = modEvents.front();
        modEvents.pop_front();

        MORDOR_ASSERT(ev->m_tid == boost::this_thread::get_id());
        ev->m_queued = false;

        MORDOR_LOG_VERBOSE(g_log) << this << " mod " << *ev;

        // We need to del even if we are modifying since there is no
        // event_mod call in libevent
        int rc = event_del(&ev->m_ev);
        if (rc < 0) {
            // TODO: see error handling comment in addEvent
            MORDOR_THROW_EXCEPTION(LibEventDelFailed());
        }

        if (ev->m_events != 0) {
            event_set(&ev->m_ev, ev->m_ev.ev_fd, ev->m_events,
                      &IOManagerEvent::eventCb, this);
            addEvent(ev);
        } else {
            m_registeredEvents.erase(ev->m_ev.ev_fd);
        }
    }
}

void
IOManagerEvent::addTickle()
{
    struct timeval *tvp = NULL;
    struct timeval tv;
    unsigned long long nextTimeout = nextTimer();
    if (nextTimeout != ~0ull) {
        tvp = &tv;
        tvp->tv_sec = nextTimeout / 1000000;
        tvp->tv_usec = nextTimeout % 1000000;
    }

    int rc = event_add(t_evTickle.get(), tvp);
    MORDOR_LOG_LEVEL(g_log, rc ? Log::ERROR : Log::VERBOSE) << this
        << " event_add(" << *t_evTickle.get() << "): "
        << rc << " (" << errno << ")";
    if (rc) {
        // TODO: this is basically fatal. Talk with cody about how to
        // best represent that in mordor
        MORDOR_THROW_EXCEPTION(LibEventAddTickleFailed());
    }
}

void
IOManagerEvent::tickled(int fd, short events, void* arg)
{
    IOManagerEvent* self = (IOManagerEvent*) arg;

    MORDOR_LOG_DEBUG(g_log) << self
        << " tickled(" << fd << ", " << events << ")";

    if (events & EV_READ) {
        TickleState* ts = t_tickleState.get();
        unsigned char dummy;
        int rc2 = read(ts->m_fds[0], &dummy, 1);
        MORDOR_ASSERT(rc2 == 1);

        // We do want to be a bit careful with this lock because
        // we want to avoid holding it across the read() so that
        // we don't serialize all the threads doing reads()
        boost::mutex::scoped_lock lock(self->m_mutex);
        MORDOR_ASSERT(ts->m_tickled);
        ts->m_tickled = false;
    }

    MORDOR_LOG_DEBUG(g_log) << self << " tickle done";
}

void
IOManagerEvent::addEvent(AsyncEvent* ev)
{
    int rc = event_add(&ev->m_ev, NULL);
    MORDOR_LOG_LEVEL(g_log, rc ? Log::ERROR : Log::VERBOSE) << this
        << " event_add(" << *ev << "): " << rc << " (" << errno << ")";
    if (rc) {
        // TODO: this is a half hearted error recovery.  We
        // really need to dispatch errors back to the original
        // caller.  Until that is done, this error should be considered
        // fatal
        MORDOR_THROW_EXCEPTION(LibEventAddFailed());
    }
}

void
IOManagerEvent::scheduleEvent(AsyncEvent* ev, int events)
{
    MORDOR_LOG_VERBOSE(g_log) << "scheduleEvent " << *ev;

    if ((events & EV_READ) && (ev->m_events & EV_READ)) {
        if (ev->m_dgRead) {
            ev->m_schedulerRead->schedule(ev->m_dgRead);
        } else {
            ev->m_schedulerRead->schedule(ev->m_fiberRead);
        }
        ev->m_dgRead = NULL;
        ev->m_fiberRead.reset();
    }

    if ((events & EV_WRITE) && (ev->m_events & EV_WRITE)) {
        if (ev->m_dgWrite) {
            ev->m_schedulerWrite->schedule(ev->m_dgWrite);
        } else {
            ev->m_schedulerWrite->schedule(ev->m_fiberWrite);
        }
        ev->m_dgWrite = NULL;
        ev->m_fiberWrite.reset();
    }

    ev->m_events &= ~events;
}

void
IOManagerEvent::eventCb(int fd, short events, void* arg)
{
    IOManagerEvent* self = (IOManagerEvent*) arg;
    MORDOR_LOG_VERBOSE(g_log) << self
        << " eventCb(" << fd << ", " << events << ")";

    boost::mutex::scoped_lock lock(self->m_mutex);
    RegisteredEvents::iterator it = self->m_registeredEvents.find(fd);
    if (it != self->m_registeredEvents.end()) {
        AsyncEvent* ev = &it->second;
        scheduleEvent(ev, events);

        // While both our events and libevents both have one shot semantics,
        // iomanager events are one shot for each type, whereas libevent
        // is one shot for either, which means that we need to reregister
        // this event with libevent if we haven't received all the
        // registered event types.
        if (ev->m_events != 0) {
            self->addEvent(ev);
        } else {
            self->m_registeredEvents.erase(it);
        }
    }
}

struct ev_events
{
    ev_events(int e)
        : m_events(e) { }

    int m_events;
};

std::ostream&
operator<<(std::ostream& os, const struct ev_events& ev)
{
    const char* sep = "";

    os << ev.m_events;

    if (ev.m_events) {
        os << " (";
        if (ev.m_events & EV_TIMEOUT) {
            os << sep << "EV_TIMEOUT";
            sep = "|";
        }

        if (ev.m_events & EV_READ) {
            os << sep << "EV_READ";
            sep = "|";
        }

        if (ev.m_events & EV_WRITE) {
            os << sep << "EV_WRITE";
            sep = "|";
        }

        if (ev.m_events & EV_SIGNAL) {
            os << sep << "EV_SIGNAL";
            sep = "|";
        }

        if (ev.m_events & EV_PERSIST) {
            os << sep << "EV_PERSIST";
            sep = "|";
        }
        os << ")";
    }

    return os;
}

std::ostream&
operator<<(std::ostream& os, const struct event& ev)
{
    os << "ev_base: " << ev.ev_base
       << " ev_fd: " << ev.ev_fd
       << " ev_res: " << ev.ev_res
       << " ev_flags: " << ev.ev_flags;
    return os;
}

std::ostream& operator<<(std::ostream& os,
                         const struct IOManagerEvent::AsyncEvent& ev)
{
    os << "q: " << ev.m_queued
       << " e: " << ev_events(ev.m_events)
       << " tid: " << ev.m_tid
       << " ev: (" << ev.m_ev << ")";
    return os;
}

}

