#ifndef __MORDOR_IOMANAGER_IOCP_H__
#define __MORDOR_IOMANAGER_IOCP_H__

#include <map>

#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>

#include "scheduler.h"
#include "timer.h"
#include "version.h"

#ifndef WINDOWS
#error IOManagerIOCP is Windows only
#endif

namespace Mordor {

class Fiber;

struct AsyncEvent
{
    AsyncEvent();

    OVERLAPPED overlapped;

    Scheduler  *m_scheduler;
    tid_t m_thread;
    boost::shared_ptr<Fiber> m_fiber;
};

class IOManager : public Scheduler, public TimerManager
{
    friend class WaitBlock;
private:
    class WaitBlock : public boost::enable_shared_from_this<WaitBlock>
    {
    public:
        typedef boost::shared_ptr<WaitBlock> ptr;
    public:
        WaitBlock(IOManager &outer);
        ~WaitBlock();

        bool registerEvent(HANDLE handle, boost::function<void ()> dg,
            bool recurring);
        size_t unregisterEvent(HANDLE handle);

    private:
        void run();
        void removeEntry(int index);

    private:
        boost::mutex m_mutex;
        IOManager &m_outer;
        HANDLE m_reconfigured;
        HANDLE m_handles[MAXIMUM_WAIT_OBJECTS];
        Scheduler *m_schedulers[MAXIMUM_WAIT_OBJECTS];
        boost::shared_ptr<Fiber> m_fibers[MAXIMUM_WAIT_OBJECTS];
        boost::function<void ()> m_dgs[MAXIMUM_WAIT_OBJECTS];
        bool m_recurring[MAXIMUM_WAIT_OBJECTS];
        int m_inUseCount;
    };

public:
    IOManager(size_t threads = 1, bool useCaller = true);
    ~IOManager();

    bool stopping();

    void registerFile(HANDLE handle);
    void registerEvent(AsyncEvent *e);
    // Only use if the async call failed, not for cancelling it
    void unregisterEvent(AsyncEvent *e);
    void registerEvent(HANDLE handle, boost::function<void ()> dg,
        bool recurring = false);
    void registerEvent(HANDLE handle, bool recurring = false)
    { registerEvent(handle, NULL, recurring); }
    size_t unregisterEvent(HANDLE handle);
    void cancelEvent(HANDLE hFile, AsyncEvent *e);

protected:
    bool stopping(unsigned long long &nextTimeout);
    void idle();
    void tickle();

    void onTimerInsertedAtFront() { tickle(); }

private:
    HANDLE m_hCompletionPort;
#ifdef DEBUG
    std::map<OVERLAPPED *, AsyncEvent*> m_pendingEvents;
#endif
    size_t m_pendingEventCount;
    boost::mutex m_mutex;
    std::list<WaitBlock::ptr> m_waitBlocks;
};

}

#endif
