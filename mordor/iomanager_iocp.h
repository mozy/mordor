#ifndef __MORDOR_IOMANAGER_IOCP_H__
#define __MORDOR_IOMANAGER_IOCP_H__

#include <map>

#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>

#include "fiber.h"
#include "scheduler.h"
#include "timer.h"
#include "version.h"

#ifndef WINDOWS
#error IOManagerIOCP is Windows only
#endif

namespace Mordor {

struct AsyncEventIOCP
{
    AsyncEventIOCP();

    BOOL ret;
    OVERLAPPED overlapped;
    DWORD numberOfBytes;
    ULONG_PTR completionKey;
    DWORD lastError;

    Scheduler  *m_scheduler;
    boost::thread::id m_thread;
    Fiber::ptr  m_fiber;
};

class IOManagerIOCP : public Scheduler, public TimerManager
{
    friend class WaitBlock;
private:
    class WaitBlock : public boost::enable_shared_from_this<WaitBlock>
    {
    public:
        typedef boost::shared_ptr<WaitBlock> ptr;
    public:
        WaitBlock(IOManagerIOCP &outer);
        ~WaitBlock();

        bool registerEvent(HANDLE handle, boost::function<void ()> dg,
            bool recurring);
        bool unregisterEvent(HANDLE handle);

    private:
        void run();

    private:
        boost::mutex m_mutex;
        IOManagerIOCP &m_outer;
        HANDLE m_reconfigured;
        HANDLE m_handles[MAXIMUM_WAIT_OBJECTS];
        Scheduler *m_schedulers[MAXIMUM_WAIT_OBJECTS];
        Fiber::ptr m_fibers[MAXIMUM_WAIT_OBJECTS];
        boost::function<void ()> m_dgs[MAXIMUM_WAIT_OBJECTS];
        bool m_recurring[MAXIMUM_WAIT_OBJECTS];
        int m_inUseCount;
    };

public:
    IOManagerIOCP(int threads = 1, bool useCaller = true);
    ~IOManagerIOCP();

    bool stopping();

    void registerFile(HANDLE handle);
    void registerEvent(AsyncEventIOCP *e);
    // Only use if the async call failed, not for cancelling it
    void unregisterEvent(AsyncEventIOCP *e);
    void registerEvent(HANDLE handle, boost::function<void ()> dg,
        bool recurring = false);
    void registerEvent(HANDLE handle, bool recurring = false)
    { registerEvent(handle, NULL, recurring); }
    bool unregisterEvent(HANDLE handle);
    void cancelEvent(HANDLE hFile, AsyncEventIOCP *e);

    Timer::ptr registerTimer(unsigned long long us,
        boost::function<void ()> dg, bool recurring = false);
    
protected:
    bool stopping(unsigned long long &nextTimeout);
    void idle();
    void tickle();

private:
    HANDLE m_hCompletionPort;
    std::map<OVERLAPPED *, AsyncEventIOCP*> m_pendingEvents;
    boost::mutex m_mutex;
    std::list<WaitBlock::ptr> m_waitBlocks;
};

}

#endif
