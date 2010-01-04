// Copyright (c) 2009 - Decho Corp.

#include "pch.h"

#include "iomanager_event.h"

#include "assert.h"
#include "exception.h"
#include "log.h"

namespace Mordor {

static Logger::ptr g_log = Log::lookup("mordor:iomanager");
static Logger::ptr g_logAddQ = Log::lookup("mordor:iomanager:addq");
static Logger::ptr g_logModQ = Log::lookup("mordor:iomanager:modq");
static Logger::ptr g_logReg = Log::lookup("mordor:iomanager:registered");

ThreadLocalStorage<struct event_base*> IOManagerEvent::t_evBase;
ThreadLocalStorage<struct event*> IOManagerEvent::t_evTickle;;
ThreadLocalStorage<IOManagerEvent::TickleState*> IOManagerEvent::t_tickleState;

void
IOManagerEvent::AsyncEventDispatch::set(boost::function<void ()>& dg)
{
    m_scheduler = Scheduler::getThis();
    if (dg) {
        m_dg = dg;
        m_fiber.reset();
    } else {
        m_dg = NULL;
        m_fiber = Fiber::getThis();
    }
}

void
IOManagerEvent::AsyncEventDispatch::transfer(AsyncEventDispatch& aed)
{
    m_scheduler= aed.m_scheduler;
    m_fiber = aed.m_fiber;
    m_dg = aed.m_dg;
    aed.m_scheduler = NULL;
    aed.m_dg = NULL;
    aed.m_fiber.reset();
}

void
IOManagerEvent::AsyncEventDispatch::schedule()
{
    if (m_scheduler) {
        if (m_dg) {
            m_scheduler->schedule(m_dg);
        } else {
            m_scheduler->schedule(m_fiber);
        }
    }
}

IOManagerEvent::IOManagerEvent(int threads, bool useCaller)
    : Scheduler(threads, useCaller),
      m_addEventsSize(0)
{
    MORDOR_LOG_DEBUG(g_log) << this
        << " IOManagerEvent(" << threads << ", " << useCaller << ")";

    if (threads - (useCaller ? 1 : 0)) {
        start();
    }

    MORDOR_LOG_DEBUG(g_log) << this
        << " IOManagerEvent(" << threads << ", " << useCaller << ") done";
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

    // Keep track of the thread to tickle, if any.  (avoids holding
    // lock during the write syscall)
    boost::thread::id* id = NULL;
    {
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
            MORDOR_LOG_VERBOSE(g_logReg) << this
                << " m_registeredEvents[fd: " << fd << "] set " << *ev;
            MORDOR_LOG_VERBOSE(g_logAddQ) << this
                << " m_addEvents.push_back(" << *ev << ")";
            m_addEventsSize++;
            m_addEvents.push_back(ev);
            id = NULL;
        } else {
            ev = &it->second;

            // Double registration of the same event type isn't allowed
            MORDOR_ASSERT(!(ev->m_events & events));

            // If the event is already in a queue being passed to a thread
            // we shouldn't requeue it.  The changes get merged and will
            // be processed correctly
            if (!ev->m_queued) {
                MORDOR_LOG_VERBOSE(g_logModQ) << this
                    << " m_modEvents[" << ev->m_tid << "].push_back("
                    << *ev << ")";
                m_modEvents[ev->m_tid].push_back(ev);
                id = &ev->m_tid;
            }
        }

        ev->m_events |= events;
        ev->m_queued = true;

        // Prep up the read callback state
        if (events == EV_READ) {
            ev->m_read.set(dg);
        }

        // Prep up the write callback state
        if (events == EV_WRITE) {
            ev->m_write.set(dg);
        }
    } // lock released

    if (id) {
        tickle(*id);
    } else {
        tickle();
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

    // We *CAN NOT* call Scheduler::schedule() while holding our lock.
    // This will can lead to cyclic lock acquistion if we do this
    // at the same time as the scheduler is holding its lock and then
    // calls tickle.  Save off the scheduler and dispatch state, unlock
    // our lock, and then schedule.
    AsyncEventDispatch readState;
    AsyncEventDispatch writeState;

    // Keep track if we need to
    boost::thread::id* id = NULL;

    {
        boost::mutex::scoped_lock lock(m_mutex);
        RegisteredEvents::iterator it = m_registeredEvents.find(fd);
        if (it != m_registeredEvents.end()) {
            AsyncEvent* ev = &it->second;
            MORDOR_ASSERT(ev->m_events & events);
            MORDOR_ASSERT(ev->m_ev.ev_fd == fd);

            if ((events & EV_READ) && (ev->m_events & EV_READ)) {
                readState.transfer(ev->m_read);
            }

            if ((events & EV_WRITE) && (ev->m_events & EV_WRITE)) {
                writeState.transfer(ev->m_write);
            }

            ev->m_events &= ~events;

            MORDOR_LOG_VERBOSE(g_logReg) << this
                << " m_registeredEvents[fd: " << fd << "] still " << *ev;

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
                id = &ev->m_tid;
            }
        }
    } // lock released

    if (id) {
        tickle(*id);
    }

    readState.schedule();
    writeState.schedule();
}

Timer::ptr
IOManagerEvent::registerTimer(unsigned long long us, Delegate dg,
                              bool recurring)
{
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
        unsigned long long nextTimeout;

        processAdds();
        processMods();

        // This needs to be done here instead of as a condition of the loop
        // because we might have things in the registeredEvents that
        // need to be removed via processMods
        if (checkDone(nextTimeout)) {
            break;
        }

        addTickle(nextTimeout);

        MORDOR_LOG_VERBOSE(g_log) << this
            << " event_base_loop(" << t_evBase
            << ") ticklefd: " << t_tickleState->m_fds[1];

        // Run the event loop. All the fds that have activity will have
        // their fibers or delegates scheduled as part of their callbacks.
        // By the time this returns, all events will have been processed.
        int rc = event_base_loop(t_evBase, EVLOOP_ONCE);
        MORDOR_LOG_LEVEL(g_log, rc < 0 ? Log::ERROR : Log::VERBOSE) << this
            << " event_base_loop(" << t_evBase << "): "
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

    // We possibly need to tickle the other threads if there was 1 lingering
    // event and other threads are blocked in the event_base_loop waiting
    // only for tickle notifications.  This can happen because when they
    // called checkDone() it returned false because we still had at least
    // 1 event at the time, even though it was later removed.
    tickle();
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
    //std::list<TickleState*> ThreadTickleState;
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
            << " tickleLocked write( fd: " << ts->m_fds[1] << ", 1): "
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
    t_tickleState = new TickleState;
    t_tickleState->m_tickled = false;

    int rc = pipe(t_tickleState->m_fds);
    MORDOR_LOG_LEVEL(g_log, rc ? Log::ERROR : Log::VERBOSE)
        << this << " pipe(): " << rc << " (" << errno << ")";
    if (rc) {
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("pipe");
    }

    MORDOR_ASSERT(t_tickleState->m_fds[0] >= 0);
    MORDOR_ASSERT(t_tickleState->m_fds[1] >= 0);

    m_threadTickleState[boost::this_thread::get_id()] = t_tickleState;

    // setup the event base
    t_evBase = event_base_new();
    MORDOR_LOG_LEVEL(g_log, t_evBase == NULL ?
                     Log::ERROR : Log::VERBOSE)
        << this << " event_base_new(): "
        << t_evBase << " " << event_base_get_method(t_evBase);
    if (t_evBase == NULL) {
        MORDOR_THROW_EXCEPTION(LibEventBaseNewFailed());
    }

    // setup the tickle event for this thread
    t_evTickle = new struct event;
    event_set(t_evTickle, t_tickleState->m_fds[0], EV_READ,
              &IOManagerEvent::tickled, (void*) this);

    rc = event_base_set(t_evBase, t_evTickle);
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

    // cleanup event base
    event_base_free(t_evBase);
    t_evBase = NULL;

    // cleanup tickle event (must do this after event_base_free
    // because it might still be intrusively linked into the libevent queues)
    delete t_evTickle;
    t_evTickle = NULL;

    // cleanup tickle fds
    for (int i = 0; i < 2; i++) {
        MORDOR_LOG_VERBOSE(g_log) << this
            << " close(" << t_tickleState->m_fds[i] << ")";
        close(t_tickleState->m_fds[i]);
    }

    // clean up tickle data structures
    m_threadTickleState.erase(boost::this_thread::get_id());

    delete t_tickleState;
    t_tickleState = NULL;

    MORDOR_LOG_DEBUG(g_log) << this << " cleanupDone()";
}

bool
IOManagerEvent::checkDone(unsigned long long& nextTimeout)
{
    MORDOR_LOG_VERBOSE(g_log) << this
        << " checkDone stopping(): " << stopping();
    nextTimeout = nextTimer();
    if (nextTimeout == ~0ull && stopping()) {
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

    MORDOR_LOG_DEBUG(g_logAddQ) << this
        << " processAdds " << m_addEventsSize << " " << m_addEvents.size();


    // Try to balance the incoming fd adds accross the various threads.
    size_t maxTodo = m_addEventsSize / threadCount() + 1;

    while (maxTodo-- && !m_addEvents.empty()) {
        int rc;

        AsyncEvent* ev = m_addEvents.front();
        m_addEvents.pop_front();
        m_addEventsSize--;

        ev->m_queued = false;

        MORDOR_LOG_VERBOSE(g_logAddQ) << this << " add " << *ev;

        // This can happen if someone does an registerEvent() followed
        // by a cancleEvent() before we get here
        if (ev->m_events == 0) {
            MORDOR_LOG_VERBOSE(g_logReg) << this
                << " m_registeredEvents[fd: " << ev->m_ev.ev_fd
                << "] erase " << *ev;
            m_registeredEvents.erase(ev->m_ev.ev_fd);
            continue;
        }

        // Take ownership of the event
        ev->m_tid = boost::this_thread::get_id();

        rc = event_base_set(t_evBase, &ev->m_ev);
        MORDOR_LOG_LEVEL(g_log, rc ? Log::ERROR : Log::VERBOSE) << this
            << " event_base_set(" << t_evBase << ", " << *ev << "): "
            << rc << " (" << errno << ")";
        if (rc < 0) {
            // TODO: see error handling comment in addEvent
            MORDOR_THROW_EXCEPTION(LibEventBaseSetFailed());
        }

        addEvent(ev);
    }

    MORDOR_LOG_DEBUG(g_logAddQ) << this
        << " processAdds done " << m_addEventsSize << " " << m_addEvents.size();
}

void
IOManagerEvent::processMods()
{
    boost::mutex::scoped_lock lock(m_mutex);

    EventList& modEvents = m_modEvents[boost::this_thread::get_id()];
    MORDOR_LOG_DEBUG(g_logModQ) << this << " processMods " << modEvents.size();

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
            MORDOR_LOG_VERBOSE(g_logReg) << this
                << " m_registeredEvents[fd: " << ev->m_ev.ev_fd
                << "] erase " << *ev;
            m_registeredEvents.erase(ev->m_ev.ev_fd);
        }
    }

    MORDOR_LOG_DEBUG(g_logModQ) << this
        << " processModsDone " << modEvents.size();
}

void
IOManagerEvent::addTickle(unsigned long long nextTimeout)
{
    struct timeval *tvp = NULL;
    struct timeval tv;
    if (nextTimeout != ~0ull) {
        tvp = &tv;
        tvp->tv_sec = nextTimeout / 1000000;
        tvp->tv_usec = nextTimeout % 1000000;
    }

    int rc = event_add(t_evTickle, tvp);
    MORDOR_LOG_LEVEL(g_log, rc ? Log::ERROR : Log::VERBOSE) << this
        << " event_add(" << *t_evTickle << "): "
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
        TickleState* ts = t_tickleState;
        unsigned char dummy;
#ifdef DEBUG
        int rc2 =
#endif
        read(ts->m_fds[0], &dummy, 1);
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
IOManagerEvent::eventCb(int fd, short events, void* arg)
{
    IOManagerEvent* self = (IOManagerEvent*) arg;
    MORDOR_LOG_VERBOSE(g_log) << self
        << " eventCb(" << fd << ", " << events << ")";

    // We *CAN NOT* call Scheduler::schedule() while holding our lock.
    // This will can lead to cyclic lock acquistion if we do this
    // at the same time as the scheduler is holding its lock and then
    // calls tickle.  Save off the scheduler and dispatch state, unlock
    // our lock, and then schedule.
    AsyncEventDispatch readState;
    AsyncEventDispatch writeState;

    {
        boost::mutex::scoped_lock lock(self->m_mutex);
        RegisteredEvents::iterator it = self->m_registeredEvents.find(fd);
        if (it != self->m_registeredEvents.end()) {
            AsyncEvent* ev = &it->second;
            MORDOR_ASSERT(ev->m_tid == boost::this_thread::get_id());

            if ((events & EV_READ) && (ev->m_events & EV_READ)) {
                readState.transfer(ev->m_read);
            }

            if ((events & EV_WRITE) && (ev->m_events & EV_WRITE)) {
                writeState.transfer(ev->m_write);
            }

            ev->m_events &= ~events;

            // While both our events and libevents both have one shot semantics,
            // iomanager events are one shot for each type, whereas libevent
            // is one shot for either, which means that we need to reregister
            // this event with libevent if we haven't received all the
            // registered event types.
            if (ev->m_events != 0) {
                MORDOR_LOG_VERBOSE(g_logReg) << self
                    << " m_registeredEvents[fd: " << ev->m_ev.ev_fd
                    << "] still " << *ev;
                self->addEvent(ev);
            } else {
                MORDOR_LOG_VERBOSE(g_logReg) << self
                    << " m_registeredEvents[fd: " << ev->m_ev.ev_fd
                    << "] erase " << *ev;
                self->m_registeredEvents.erase(it);
            }
        }
    } // lock released

    readState.schedule();
    writeState.schedule();
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

